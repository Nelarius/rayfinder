extern "C" {
#include <hosekwilkie-skylightmodel-source-1.4a/ArHosekSkyModel.h>
}

#include <glm/glm.hpp>
#include <stb_image_write.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <format>
#include <numbers>
#include <numeric>
#include <tuple>
#include <vector>

inline constexpr double PI = std::numbers::pi_v<double>;
inline constexpr double PI_2 = PI / 2.0;
inline constexpr double DEGREES_TO_RADIANS = PI / 180.0;

inline constexpr int WIDTH = 720;
inline constexpr int HEIGHT = 720;

static glm::dvec3 expose(const glm::dvec3& x, const double exposure)
{
    return glm::dvec3(2.0) / (glm::dvec3(1.0) + glm::exp(-exposure * x)) - glm::dvec3(1.0);
}

// Source: https://jcgt.org/published/0002/02/01/
// Simple Analytic Approximations to the CIE XYZ Color Matching Functions
// Multi-lobe gaussian fit to the CIE 1931 color matching functions.
static double cie1931X(const double wave)
{
    const double t1 = (wave - 442.0) * ((wave < 442.0) ? 0.0624 : 0.0374);
    const double t2 = (wave - 599.8) * ((wave < 599.8) ? 0.0264 : 0.0323);
    const double t3 = (wave - 501.1) * ((wave < 501.1) ? 0.0490 : 0.0382);
    return 0.362 * std::exp(-0.5 * t1 * t1) + 1.056 * std::exp(-0.5 * t2 * t2) -
           0.065 * std::exp(-0.5 * t3 * t3);
}

static double cie1931Y(const double wave)
{
    const double t1 = (wave - 568.8) * ((wave < 568.8) ? 0.0213 : 0.0247);
    const double t2 = (wave - 530.9) * ((wave < 530.9) ? 0.0613 : 0.0322);
    return 0.821 * std::exp(-0.5 * t1 * t1) + 0.286 * std::exp(-0.5 * t2 * t2);
}

static double cie1931Z(const double wave)
{
    const double t1 = (wave - 437.0) * ((wave < 437.0) ? 0.0845 : 0.0278);
    const double t2 = (wave - 459.0) * ((wave < 459.0) ? 0.0385 : 0.0725);
    return 1.217 * std::exp(-0.5 * t1 * t1) + 0.681 * std::exp(-0.5 * t2 * t2);
}

constexpr std::array<double, 11> wavelengths =
    {320.0, 360.0, 400.0, 440.0, 480.0, 520.0, 560.0, 600.0, 640.0, 680.0, 720.0};

// Source: http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
constexpr glm::dmat3x3 xyzToSrgb(
    // clang-format off
        3.2404542, -0.9692660, 0.0556434,
        -1.5371385, 1.8760108, -0.2040259,
        -0.4985314, 0.0415560, 1.0572252
    // clang-format on
);

int main()
{
    const double     sunZenith = 30.0 * DEGREES_TO_RADIANS;
    const double     sunAzimuth = 0.0 * DEGREES_TO_RADIANS;
    const glm::dvec3 sunDirection = glm::normalize(glm::dvec3(
        std::sin(sunZenith) * std::cos(sunAzimuth),
        std::cos(sunZenith),
        -std::sin(sunZenith) * std::sin(sunAzimuth)));

    const double solarElevation = 0.5 * PI - sunZenith;
    const double turbidity = 1.0;
    const double albedo = 1.0;

    auto* skyState = arhosekskymodelstate_alloc_init(solarElevation, turbidity, albedo);

    std::vector<std::uint32_t> pixelData;
    pixelData.reserve(static_cast<std::size_t>(WIDTH * HEIGHT));

    for (int i = 0; i < HEIGHT; ++i)
    {
        for (int j = 0; j < WIDTH; ++j)
        {
            const auto [x, y] = [](const int i, const int j) -> std::tuple<double, double> {
                // coordinates in [0, 1]
                const double u = static_cast<double>(j) / static_cast<double>(WIDTH);
                const double v = static_cast<double>(i) / static_cast<double>(HEIGHT);

                // coordinates in [-1, 1]
                const double x = 2.0 * u - 1.0;
                const double y = 1.0 - 2.0 * v; // flip y so that (left, top) is written first

                return {x, y};
            }(i, j);

            const double radiusSqr = x * x + y * y;

            glm::dvec4 rgba = glm::vec4(0.0);

            if (radiusSqr < 1.0f)
            {
                // Pixel is inside the hemisphere, compute the ray direction.
                const double     z = std::sqrt(1.0 - radiusSqr);
                const glm::dvec3 v = glm::normalize(glm::vec3(x, z, -y));
                const glm::dvec3 s = sunDirection;

                // Compute the sky radiance.

                [[maybe_unused]] const double theta = std::acos(v.y);
                [[maybe_unused]] const double gamma =
                    std::acos(std::clamp(glm::dot(v, s), -1.0, 1.0));

                // Integrate XYZ tristimulus values over the visible spectrum using the Trapezoidal
                // rule: https://en.wikipedia.org/wiki/Trapezoidal_rule#Uniform_grid

#if 1
                const double           exposure = 0.1;
                std::array<double, 11> radiances = {};
                for (std::size_t idx = 0; idx < wavelengths.size(); ++idx)
                {
                    radiances[idx] =
                        arhosekskymodel_radiance(skyState, theta, gamma, wavelengths[idx]);
                }
#else
                const double           exposure = 0.000002;
                std::array<double, 11> radiances = {};
                const double           solarDiskRadius = theta / PI_2;
                for (std::size_t idx = 0; idx < wavelengths.size(); ++idx)
                {
                    radiances[idx] = arhosekskymodel_solar_disk_radiance(
                        skyState, gamma, solarDiskRadius, wavelengths[idx]);
                }
#endif

                constexpr std::size_t backIdx = wavelengths.size() - 1;
                constexpr double      deltaWl = (wavelengths[backIdx] - wavelengths[0]) /
                                           static_cast<double>(wavelengths.size());

                assert(wavelengths.size() == radiances.size());
                double xradiance = 0.5 * (cie1931X(wavelengths[0]) * radiances[0] +
                                          cie1931X(wavelengths[backIdx]) * radiances[backIdx]);
                for (std::size_t idx = 1; idx < backIdx; ++idx)
                {
                    xradiance += cie1931X(wavelengths[idx]) * radiances[idx];
                }
                xradiance *= deltaWl;

                double yradiance = 0.5 * (cie1931Y(wavelengths[0]) * radiances[0] +
                                          cie1931Y(wavelengths[backIdx]) * radiances[backIdx]);
                for (std::size_t idx = 1; idx < backIdx; ++idx)
                {
                    yradiance += cie1931Y(wavelengths[idx]) * radiances[idx];
                }
                yradiance *= deltaWl;

                double zradiance = 0.5 * (cie1931Z(wavelengths[0]) * radiances[0] +
                                          cie1931Z(wavelengths[backIdx]) * radiances[backIdx]);
                for (std::size_t idx = 1; idx < backIdx; ++idx)
                {
                    zradiance += cie1931Z(wavelengths[idx]) * radiances[idx];
                }
                zradiance *= deltaWl;

                const glm::dvec3 radiance = xyzToSrgb * glm::dvec3(xradiance, yradiance, zradiance);
                const glm::dvec3 color = expose(radiance, exposure);
                rgba = glm::dvec4(color, 1.0);
            }

            const auto r = static_cast<std::uint32_t>(std::min(rgba.r, 1.0) * 255.0);
            const auto g = static_cast<std::uint32_t>(std::min(rgba.g, 1.0) * 255.0);
            const auto b = static_cast<std::uint32_t>(std::min(rgba.b, 1.0) * 255.0);
            const auto a = static_cast<std::uint32_t>(std::min(rgba.a, 1.0) * 255.0);

            const std::uint32_t pixel = (a << 24) | (b << 16) | (g << 8) | r;
            pixelData.push_back(pixel);
        }
    }

    arhosekskymodelstate_free(skyState);

    const int numChannels = 4;
    const int strideBytes = WIDTH * numChannels;

    [[maybe_unused]] const int result = stbi_write_png(
        "hw-sunmodel-integrator.png", WIDTH, HEIGHT, numChannels, pixelData.data(), strideBytes);
    assert(result != 0);

    return 0;
}

extern "C" {
#include <hw-skymodel/ArHosekSkyModel.h>
}

#include <glm/glm.hpp>
#include <stb_image_write.h>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

inline constexpr double PI = std::numbers::pi_v<double>;
inline constexpr double DEGREES_TO_RADIANS = PI / 180.0;

inline constexpr int WIDTH = 720;
inline constexpr int HEIGHT = 720;

static glm::dvec3 expose(const glm::dvec3& x, const double exposure)
{
    return glm::dvec3(2.0) / (glm::dvec3(1.0) + glm::exp(-exposure * x)) - glm::dvec3(1.0);
}

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
    const double albedoR = 1.0;
    const double albedoG = 1.0;
    const double albedoB = 1.0;

    auto* skyStateR = arhosek_rgb_skymodelstate_alloc_init(solarElevation, turbidity, albedoR);
    auto* skyStateG = arhosek_rgb_skymodelstate_alloc_init(solarElevation, turbidity, albedoG);
    auto* skyStateB = arhosek_rgb_skymodelstate_alloc_init(solarElevation, turbidity, albedoB);

    std::vector<std::uint32_t> pixelData;
    pixelData.reserve(static_cast<std::size_t>(WIDTH * HEIGHT));

    for (int i = 0; i < HEIGHT; ++i)
    {
        for (int j = 0; j < WIDTH; ++j)
        {
            // coordinates in [0, 1]
            const double u = static_cast<double>(j) / static_cast<double>(WIDTH);
            const double v = static_cast<double>(i) / static_cast<double>(HEIGHT);

            // coordinates in [-1, 1]
            const double x = 2.0 * u - 1.0;
            const double y = 1.0 - 2.0 * v; // flip y so that (left, top) is written first

            const double radiusSqr = x * x + y * y;

            glm::dvec4 rgba = glm::vec4(0.0);

            if (radiusSqr < 1.0f)
            {
                // Pixel is inside the hemisphere, compute the ray direction.
                const double     z = std::sqrt(1.0 - radiusSqr);
                const glm::dvec3 v = glm::normalize(glm::vec3(x, z, -y));
                const glm::dvec3 s = sunDirection;

                // Compute the sky radiance.
                const double     theta = std::acos(v.y);
                const double     gamma = std::acos(std::clamp(glm::dot(v, s), -1.0, 1.0));
                const glm::dvec3 radiance = glm::dvec3(
                    arhosek_tristim_skymodel_radiance(skyStateR, theta, gamma, 0),
                    arhosek_tristim_skymodel_radiance(skyStateG, theta, gamma, 1),
                    arhosek_tristim_skymodel_radiance(skyStateB, theta, gamma, 2));

                const glm::dvec3 color = expose(radiance, 0.1);
                rgba = glm::dvec4(color, 1.0f);
            }

            const auto r = static_cast<std::uint32_t>(std::min(rgba.r, 1.0) * 255.0);
            const auto g = static_cast<std::uint32_t>(std::min(rgba.g, 1.0) * 255.0);
            const auto b = static_cast<std::uint32_t>(std::min(rgba.b, 1.0) * 255.0);
            const auto a = static_cast<std::uint32_t>(std::min(rgba.a, 1.0) * 255.0);

            const std::uint32_t pixel = (a << 24) | (b << 16) | (g << 8) | r;
            pixelData.push_back(pixel);
        }
    }

    const int numChannels = 4;
    const int strideBytes = WIDTH * numChannels;

    [[maybe_unused]] const int result = stbi_write_png(
        "hw-skymodel-demo.png", WIDTH, HEIGHT, numChannels, pixelData.data(), strideBytes);
    assert(result != 0);

    return 0;
}

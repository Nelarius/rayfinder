#include <hw-skymodel/hw_skymodel.h>

#include <glm/glm.hpp>
#include <stb_image_write.h>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

inline constexpr float PI = std::numbers::pi_v<float>;

inline constexpr float DEGREES_TO_RADIANS = PI / 180.0f;
inline constexpr int   WIDTH = 720;
inline constexpr int   HEIGHT = 720;

static glm::vec3 expose(const glm::vec3& x, const float exposure)
{
    return glm::vec3(2.0f) / (glm::vec3(1.0f) + glm::exp(-exposure * x)) - glm::vec3(1.0f);
}

int main()
{
    const float     sunZenith = 30.f * DEGREES_TO_RADIANS;
    const float     sunAzimuth = 0.0f * DEGREES_TO_RADIANS;
    const glm::vec3 sunDirection = glm::normalize(glm::vec3(
        std::sin(sunZenith) * std::cos(sunAzimuth),
        std::cos(sunZenith),
        -std::sin(sunZenith) * std::sin(sunAzimuth)));

    const SkyParams skyParams{
        .elevation = 0.5f * PI - sunZenith,
        .turbidity = 1.0f,
        .albedo = {1.0f, 1.0f, 1.0f},
    };

    SkyState skyState;

    [[maybe_unused]] const auto r = skyStateNew(&skyParams, &skyState);
    assert(r == SkyStateResult_Success);

    const std::size_t         numBytes = static_cast<std::size_t>(WIDTH * HEIGHT * 3);
    std::vector<std::uint8_t> pixelData;
    pixelData.reserve(numBytes);

    for (int i = 0; i < HEIGHT; ++i)
    {
        for (int j = 0; j < WIDTH; ++j)
        {
            // coordinates in [0, 1]
            const float u = static_cast<float>(j) / static_cast<float>(WIDTH);
            const float v = static_cast<float>(i) / static_cast<float>(HEIGHT);

            // coordinates in [-1, 1]
            const float x = 2.0f * u - 1.0f;
            const float y = 1.0f - 2.0f * v; // flip y so that (left, top) is written first

            const float radiusSqr = x * x + y * y;

            glm::vec4 rgba = glm::vec4(0.0f);

            if (radiusSqr < 1.0f)
            {
                // Pixel is inside the hemisphere, compute the ray direction.
                const float     z = std::sqrt(1.0f - radiusSqr);
                const glm::vec3 v = glm::normalize(glm::vec3(x, z, -y));
                const glm::vec3 s = sunDirection;

                // Compute the sky radiance.
                const float     theta = std::acos(v.y);
                const float     gamma = std::acos(std::clamp(glm::dot(v, s), -1.0f, 1.0f));
                const glm::vec3 radiance = glm::vec3(
                    skyStateRadiance(&skyState, theta, gamma, Channel_R),
                    skyStateRadiance(&skyState, theta, gamma, Channel_G),
                    skyStateRadiance(&skyState, theta, gamma, Channel_B));

                const glm::vec3 color = expose(radiance, 0.1f);
                rgba = glm::vec4(color, 1.0f);
            }

            const auto r = static_cast<std::uint8_t>(std::min(rgba.r, 1.0f) * 255.0f);
            const auto g = static_cast<std::uint8_t>(std::min(rgba.g, 1.0f) * 255.0f);
            const auto b = static_cast<std::uint8_t>(std::min(rgba.b, 1.0f) * 255.0f);
            const auto a = static_cast<std::uint8_t>(std::min(rgba.a, 1.0f) * 255.0f);

            pixelData.push_back(r);
            pixelData.push_back(g);
            pixelData.push_back(b);
            pixelData.push_back(a);
        }
    }

    const int numChannels = 4;
    const int strideBytes = WIDTH * numChannels;

    [[maybe_unused]] const int result = stbi_write_png(
        "hw-skymodel-demo.png", WIDTH, HEIGHT, numChannels, pixelData.data(), strideBytes);
    assert(result != 0);

    return 0;
}

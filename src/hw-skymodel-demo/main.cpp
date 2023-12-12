#include <hw-skymodel/hw_skymodel.h>

#include <glm/glm.hpp>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <numbers>

inline constexpr float PI = std::numbers::pi_v<float>;

inline constexpr float DEGREES_TO_RADIANS = PI / 180.0f;
inline constexpr int   width = 720;
inline constexpr int   height = 720;

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

    std::cout << "P6\n";
    std::cout << width << ' ' << height << '\n';
    std::cout << "255\n";
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            // coordinates in [0, 1]
            const float u = static_cast<float>(j) / static_cast<float>(width);
            const float v = static_cast<float>(i) / static_cast<float>(height);

            // coordinates in [-1, 1]
            const float x = 2.0f * u - 1.0f;
            const float y = 2.0f * v - 1.0f;

            const float radiusSqr = x * x + y * y;

            glm::vec3 color = glm::vec3(0.0f);

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
                color = expose(radiance, 0.1f);
            }

            const auto r = static_cast<std::uint8_t>(std::min(color.r, 1.0f) * 255.0f);
            const auto g = static_cast<std::uint8_t>(std::min(color.g, 1.0f) * 255.0f);
            const auto b = static_cast<std::uint8_t>(std::min(color.b, 1.0f) * 255.0f);
            std::cout << r << g << b;
        }
    }
}

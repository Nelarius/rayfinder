#pragma once

#include <common/assert.hpp>
#include <common/units/angle.hpp>

#include <glm/glm.hpp>
#include <hw-skymodel/hw_skymodel.h>

#include <array>
#include <cstring>
#include <numbers>

namespace nlrs
{
struct Sky
{
    float                turbidity = 1.0f;
    std::array<float, 3> albedo = {1.0f, 1.0f, 1.0f};
    float                sunZenithDegrees = 30.0f;
    float                sunAzimuthDegrees = 0.0f;

    bool operator==(const Sky&) const noexcept = default;
};

// A 16-byte aligned sky state for the hw-skymodel library. Matches the layout of the following WGSL
// struct:
//
// struct SkyState {
//     params: array<f32, 27>,
//     skyRadiances: array<f32, 3>,
//     solarRadiances: array<f32, 3>,
//     sunDirection: vec3<f32>,
// };
struct AlignedSkyState
{
    float     params[27];        // offset: 0
    float     skyRadiances[3];   // offset: 27
    float     solarRadiances[3]; // offset: 30
    float     padding1[3];       // offset: 33
    glm::vec3 sunDirection;      // offset: 36
    float     padding2;          // offset: 39

    inline AlignedSkyState(const Sky& sky)
        : params{0},
          skyRadiances{0},
          solarRadiances{0},
          padding1{0.f, 0.f, 0.f},
          sunDirection(0.f),
          padding2(0.0f)
    {
        const float sunZenith = Angle::degrees(sky.sunZenithDegrees).asRadians();
        const float sunAzimuth = Angle::degrees(sky.sunAzimuthDegrees).asRadians();

        sunDirection = glm::normalize(glm::vec3(
            std::sin(sunZenith) * std::cos(sunAzimuth),
            std::cos(sunZenith),
            -std::sin(sunZenith) * std::sin(sunAzimuth)));

        const sky_params skyParams{
            .elevation = 0.5f * std::numbers::pi_v<float> - sunZenith,
            .turbidity = sky.turbidity,
            .albedo = {sky.albedo[0], sky.albedo[1], sky.albedo[2]}};

        sky_state skyState;
        NLRS_ASSERT(sky_state_new(&skyParams, &skyState) == sky_state_result_success);

        std::memcpy(params, skyState.params, sizeof(skyState.params));
        std::memcpy(skyRadiances, skyState.sky_radiances, sizeof(skyState.sky_radiances));
        std::memcpy(solarRadiances, skyState.solar_radiances, sizeof(skyState.solar_radiances));
    }
};
} // namespace nlrs

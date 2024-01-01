#include "hw_skymodel.h"

#include "params_r.h"
#include "params_g.h"
#include "params_b.h"
#include "radiances_r.h"
#include "radiances_g.h"
#include "radiances_b.h"

#include <assert.h>
#include <stddef.h>
#define _USE_MATH_DEFINES
#include <math.h>

static const float PI = (float)M_PI;
static const float SOLAR_RADIUS_RADIANS = 0.004450589f; // 0.255 degrees

static float quintic_9(const float* const data, const float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float t4 = t2 * t2;
    const float t5 = t4 * t;

    const float inv_t = 1.0f - t;
    const float inv_t2 = inv_t * inv_t;
    const float inv_t3 = inv_t2 * inv_t;
    const float inv_t4 = inv_t2 * inv_t2;
    const float inv_t5 = inv_t4 * inv_t;

    const float m0 = data[0] * inv_t5;
    const float m1 = data[9] * 5.0f * inv_t4 * t;
    const float m2 = data[2 * 9] * 10.0f * inv_t3 * t2;
    const float m3 = data[3 * 9] * 10.0f * inv_t2 * t3;
    const float m4 = data[4 * 9] * 5.0f * inv_t * t4;
    const float m5 = data[5 * 9] * t5;

    return m0 + m1 + m2 + m3 + m4 + m5;
}

static float quintic_1(const float* const data, const float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float t4 = t2 * t2;
    const float t5 = t4 * t;

    const float inv_t = 1.0f - t;
    const float inv_t2 = inv_t * inv_t;
    const float inv_t3 = inv_t2 * inv_t;
    const float inv_t4 = inv_t2 * inv_t2;
    const float inv_t5 = inv_t4 * inv_t;

    const float m0 = data[0] * inv_t5;
    const float m1 = data[1] * 5.0f * inv_t4 * t;
    const float m2 = data[2 * 1] * 10.0f * inv_t3 * t2;
    const float m3 = data[3 * 1] * 10.0f * inv_t2 * t3;
    const float m4 = data[4 * 1] * 5.0f * inv_t * t4;
    const float m5 = data[5 * 1] * t5;

    return m0 + m1 + m2 + m3 + m4 + m5;
}

static void init_params(
    float* const       out_params,
    const float* const data,
    const float        turbidity,
    const float        albedo,
    const float        t)
{
    const size_t turbidity_int = (size_t)turbidity;
    const float  turbidity_rem = fmodf(turbidity, 1.0f);
    assert(turbidity_int > 0);
    const size_t turbidity_min = turbidity_int - 1;
    const size_t turbidity_max = turbidity_int < 9 ? turbidity_int : 9;

    const float* const p0 = data + (9 * 6 * turbidity_min);
    const float* const p1 = data + (9 * 6 * turbidity_max);
    const float* const p2 = data + (9 * 6 * 10 + 9 * 6 * turbidity_min);
    const float* const p3 = data + (9 * 6 * 10 + 9 * 6 * turbidity_max);

    const float s0 = (1.0f - albedo) * (1.0f - turbidity_rem);
    const float s1 = (1.0f - albedo) * turbidity_rem;
    const float s2 = albedo * (1.0f - turbidity_rem);
    const float s3 = albedo * turbidity_rem;

    for (size_t i = 0; i < 9; ++i)
    {
        out_params[i] = 0.0f;
        out_params[i] += s0 * quintic_9(p0 + i, t);
        out_params[i] += s1 * quintic_9(p1 + i, t);
        out_params[i] += s2 * quintic_9(p2 + i, t);
        out_params[i] += s3 * quintic_9(p3 + i, t);
    }
}

static void init_sky_radiance(
    float* const       out_radiance,
    const float* const data,
    const float        turbidity,
    const float        albedo,
    const float        t)
{
    const size_t turbidity_int = (size_t)turbidity;
    const float  turbidity_rem = fmodf(turbidity, 1.0f);
    assert(turbidity_int > 0);
    const size_t turbidity_min = turbidity_int - 1;
    const size_t turbidity_max = turbidity_int < 9 ? turbidity_int : 9;

    const float* const p0 = data + (6 * turbidity_min);
    const float* const p1 = data + (6 * turbidity_max);
    const float* const p2 = data + (6 * 10 + 6 * turbidity_min);
    const float* const p3 = data + (6 * 10 + 6 * turbidity_max);

    const float s0 = (1.0f - albedo) * (1.0f - turbidity_rem);
    const float s1 = (1.0f - albedo) * turbidity_rem;
    const float s2 = albedo * (1.0f - turbidity_rem);
    const float s3 = albedo * turbidity_rem;

    *out_radiance = 0.0f;
    *out_radiance += s0 * quintic_1(p0, t);
    *out_radiance += s1 * quintic_1(p1, t);
    *out_radiance += s2 * quintic_1(p2, t);
    *out_radiance += s3 * quintic_1(p3, t);
}

static void init_solar_radiance(
    float* const       outRadiance,
    const float* const data,
    const float        turbidity)
{
    const size_t turbidity_int = (size_t)turbidity;
    assert(turbidity_int > 0);
    const float  turbidity_rem = fmodf(turbidity, 1.0f);
    const size_t turbidity_min = turbidity_int - 1;
    const size_t turbidity_max = turbidity_int < 9 ? turbidity_int : 9;
    *outRadiance =
        data[turbidity_min] * (1.0f - turbidity_rem) + data[turbidity_max] * turbidity_rem;
}

sky_state_result sky_state_new(const sky_params* const params, sky_state* const state)
{
    const float elevation = params->elevation;
    const float turbidity = params->turbidity;
    const float albedo[3] = {params->albedo[0], params->albedo[1], params->albedo[2]};

    // Validate

    if (elevation < 0.0f || elevation > PI)
    {
        return sky_state_result_elevation_out_of_range;
    }

    if (turbidity < 1.0f || turbidity > 10.0f)
    {
        return sky_state_result_turbidity_out_of_range;
    }

    if ((albedo[0] < 0.0f || albedo[0] > 1.0f) || (albedo[1] < 0.0f || albedo[1] > 1.0f) ||
        (albedo[2] < 0.0f || albedo[2] > 1.0f))
    {
        return sky_state_result_albedo_out_of_range;
    }

    // Init state.

    const float t = powf((elevation / (0.5f * PI)), (1.0f / 3.0f));

    init_params(state->params + 0, params_r, turbidity, albedo[0], t);
    init_params(state->params + 9, params_g, turbidity, albedo[1], t);
    init_params(state->params + (9 * 2), params_b, turbidity, albedo[2], t);
    init_sky_radiance(state->sky_radiances + 0, radiances_r, turbidity, albedo[0], t);
    init_sky_radiance(state->sky_radiances + 1, radiances_g, turbidity, albedo[1], t);
    init_sky_radiance(state->sky_radiances + 2, radiances_b, turbidity, albedo[2], t);
    init_solar_radiance(state->solar_radiances + 0, solar_radiances_r, turbidity);
    init_solar_radiance(state->solar_radiances + 1, solar_radiances_g, turbidity);
    init_solar_radiance(state->solar_radiances + 2, solar_radiances_b, turbidity);

    return sky_state_result_success;
}

float sky_state_radiance(
    const sky_state* const state,
    const float            theta,
    const float            gamma,
    const channel          channel)
{
    const size_t channel_idx = (size_t)channel;

    // Sky dome radiance
    const float  r = state->sky_radiances[channel_idx];
    const float* p = state->params + (9 * channel_idx);
    const float  p0 = p[0];
    const float  p1 = p[1];
    const float  p2 = p[2];
    const float  p3 = p[3];
    const float  p4 = p[4];
    const float  p5 = p[5];
    const float  p6 = p[6];
    const float  p7 = p[7];
    const float  p8 = p[8];

    const float cos_gamma = cosf(gamma);
    const float cos_gamma_2 = cos_gamma * cos_gamma;
    const float cos_theta = fabsf(cosf(theta));

    const float exp_m = expf(p4 * gamma);
    const float ray_m = cos_gamma_2;
    const float mie_m_lhs = 1.0f + cos_gamma_2;
    const float mie_m_rhs = powf(1.0f + p8 * p8 - 2.0f * p8 * cos_gamma, 1.5f);
    const float mie_m = mie_m_lhs / mie_m_rhs;
    const float zenith = sqrtf(cos_theta);
    const float radiance_lhs = 1.0f + p0 * expf(p1 / (cos_theta + 0.01f));
    const float radiance_rhs = p2 + p3 * exp_m + p5 * ray_m + p6 * mie_m + p7 * zenith;
    const float radiance_dist = radiance_lhs * radiance_rhs;

    // Solar radiance
    const float solar_disk_radius = gamma / SOLAR_RADIUS_RADIANS;
    const float solar_radiance =
        solar_disk_radius <= 1.f ? state->solar_radiances[channel_idx] : 0.f;

    return r * radiance_dist + solar_radiance;
}

#ifndef HW_SKYMODEL_INCLUDED
#define HW_SKYMODEL_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sky_params
{
    // The solar elevation (azimuth) angle, in radians. Elevation must be in `[0, π/2]`.
    float elevation;
    // Turbidity must be in `[1, 10]`.
    float turbidity;
    // Ground albedo must be in `[0, 1]`.
    float albedo[3];
} sky_params;

typedef struct sky_state
{
    float params[27];
    float sky_radiances[3];
    float solar_radiances[3];
} sky_state;

typedef enum sky_state_result
{
    sky_state_result_success,
    sky_state_result_elevation_out_of_range,
    sky_state_result_turbidity_out_of_range,
    sky_state_result_albedo_out_of_range,
} sky_state_result;

// Initialize a SkyState instance. Returns 1 if creating the skyState was succesful. Returns 0 if
// any of the sky params are out of range.
sky_state_result sky_state_new(const sky_params* params, sky_state* skyState);

typedef enum channel
{
    channel_r = 0,
    channel_g,
    channel_b
} channel;

float sky_state_radiance(const sky_state* state, float theta, float gamma, channel channel);

#ifdef __cplusplus
}
#endif
#endif // HW_SKYMODEL_INCLUDED

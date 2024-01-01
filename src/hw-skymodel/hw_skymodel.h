#ifndef HW_SKYMODEL_INCLUDED
#define HW_SKYMODEL_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SkyParams
{
    // The solar elevation (azimuth) angle, in radians. Elevation must be in `[0, π/2]`.
    float elevation;
    // Turbidity must be in `[1, 10]`.
    float turbidity;
    // Ground albedo must be in `[0, 1]`.
    float albedo[3];
} SkyParams;

typedef struct SkyState
{
    float params[27];
    float skyRadiance[3];
    float solarRadiance[3];
} SkyState;

typedef enum SkyStateResult
{
    SkyStateResult_Success,
    SkyStateResult_ElevationOutOfRange,
    SkyStateResult_TurbidityOutOfRange,
    SkyStateResult_AlbedoOutOfRange,
} SkyStateResult;

// Initialize a SkyState instance. Returns 1 if creating the skyState was succesful. Returns 0 if
// any of the sky params are out of range.
SkyStateResult skyStateNew(const SkyParams* skyParams, SkyState* skyState);

typedef enum Channel
{
    Channel_R,
    Channel_G,
    Channel_B
} Channel;

float skyStateRadiance(const SkyState* skyState, float theta, float gamma, Channel channel);

#ifdef __cplusplus
}
#endif
#endif // HW_SKYMODEL_INCLUDED

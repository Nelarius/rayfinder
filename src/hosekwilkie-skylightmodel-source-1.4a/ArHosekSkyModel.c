/*
This source is published under the following 3-clause BSD license.

Copyright (c) 2012 - 2013, Lukas Hosek and Alexander Wilkie
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * None of the names of the contributors may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* ============================================================================

This file is part of a sample implementation of the analytical skylight and
solar radiance models presented in the SIGGRAPH 2012 paper


           "An Analytic Model for Full Spectral Sky-Dome Radiance"

and the 2013 IEEE CG&A paper

       "Adding a Solar Radiance Function to the Hosek Skylight Model"

                                   both by

                       Lukas Hosek and Alexander Wilkie
                Charles University in Prague, Czech Republic


                        Version: 1.4a, February 22nd, 2013

Version history:

1.4a  February 22nd, 2013
      Removed unnecessary and counter-intuitive solar radius parameters
      from the interface of the colourspace sky dome initialisation functions.

1.4   February 11th, 2013
      Fixed a bug which caused the relative brightness of the solar disc
      and the sky dome to be off by a factor of about 6. The sun was too
      bright: this affected both normal and alien sun scenarios. The
      coefficients of the solar radiance function were changed to fix this.

1.3   January 21st, 2013 (not released to the public)
      Added support for solar discs that are not exactly the same size as
      the terrestrial sun. Also added support for suns with a different
      emission spectrum ("Alien World" functionality).

1.2a  December 18th, 2012
      Fixed a mistake and some inaccuracies in the solar radiance function
      explanations found in ArHosekSkyModel.h. The actual source code is
      unchanged compared to version 1.2.

1.2   December 17th, 2012
      Native RGB data and a solar radiance function that matches the turbidity
      conditions were added.

1.1   September 2012
      The coefficients of the spectral model are now scaled so that the output
      is given in physical units: W / (m^-2 * sr * nm). Also, the output of the
      XYZ model is now no longer scaled to the range [0...1]. Instead, it is
      the result of a simple conversion from spectral data via the CIE 2 degree
      standard observer matching functions. Therefore, after multiplication
      with 683 lm / W, the Y channel now corresponds to luminance in lm.

1.0   May 11th, 2012
      Initial release.


Please visit http://cgg.mff.cuni.cz/projects/SkylightModelling/ to check if
an updated version of this code has been published!

============================================================================ */

/*

All instructions on how to use this code are in the accompanying header file.

*/

#include "ArHosekSkyModel.h"
#include "ArHosekSkyModelData_Spectral.h"
#include "ArHosekSkyModelData_CIEXYZ.h"
#include "ArHosekSkyModelData_RGB.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

//   Some macro definitions that occur elsewhere in ART, and that have to be
//   replicated to make this a stand-alone module.

#ifndef NIL
#define NIL 0
#endif

#ifndef MATH_PI
#define MATH_PI 3.141592653589793
#endif

#ifndef MATH_DEG_TO_RAD
#define MATH_DEG_TO_RAD (MATH_PI / 180.0)
#endif

#ifndef MATH_RAD_TO_DEG
#define MATH_RAD_TO_DEG (180.0 / MATH_PI)
#endif

#ifndef DEGREES
#define DEGREES *MATH_DEG_TO_RAD
#endif

#ifndef TERRESTRIAL_SOLAR_RADIUS
#define TERRESTRIAL_SOLAR_RADIUS ((0.51 DEGREES) / 2.0)
#endif

#ifndef ALLOC
#define ALLOC(_struct) ((_struct*)malloc(sizeof(_struct)))
#endif

// internal definitions

typedef double* ArHosekSkyModel_Dataset;
typedef double* ArHosekSkyModel_Radiance_Dataset;

// internal functions

void ArHosekSkyModel_CookConfiguration(
    ArHosekSkyModel_Dataset      dataset,
    ArHosekSkyModelConfiguration config,
    double                       turbidity,
    double                       albedo,
    double                       solar_elevation)
{
    double* elev_matrix;

    int    int_turbidity = (int)turbidity;
    double turbidity_rem = turbidity - (double)int_turbidity;

    solar_elevation = pow(solar_elevation / (MATH_PI / 2.0), (1.0 / 3.0));

    // alb 0 low turb

    elev_matrix = dataset + (9 * 6 * (int_turbidity - 1));

    for (unsigned int i = 0; i < 9; ++i)
    {
        //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
        config[i] =
            (1.0 - albedo) * (1.0 - turbidity_rem) *
            (pow(1.0 - solar_elevation, 5.0) * elev_matrix[i] +
             5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[i + 9] +
             10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) *
                 elev_matrix[i + 18] +
             10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) *
                 elev_matrix[i + 27] +
             5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[i + 36] +
             pow(solar_elevation, 5.0) * elev_matrix[i + 45]);
    }

    // alb 1 low turb
    elev_matrix = dataset + (9 * 6 * 10 + 9 * 6 * (int_turbidity - 1));
    for (unsigned int i = 0; i < 9; ++i)
    {
        //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
        config[i] +=
            (albedo) * (1.0 - turbidity_rem) *
            (pow(1.0 - solar_elevation, 5.0) * elev_matrix[i] +
             5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[i + 9] +
             10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) *
                 elev_matrix[i + 18] +
             10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) *
                 elev_matrix[i + 27] +
             5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[i + 36] +
             pow(solar_elevation, 5.0) * elev_matrix[i + 45]);
    }

    if (int_turbidity == 10)
        return;

    // alb 0 high turb
    elev_matrix = dataset + (9 * 6 * (int_turbidity));
    for (unsigned int i = 0; i < 9; ++i)
    {
        //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
        config[i] +=
            (1.0 - albedo) * (turbidity_rem) *
            (pow(1.0 - solar_elevation, 5.0) * elev_matrix[i] +
             5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[i + 9] +
             10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) *
                 elev_matrix[i + 18] +
             10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) *
                 elev_matrix[i + 27] +
             5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[i + 36] +
             pow(solar_elevation, 5.0) * elev_matrix[i + 45]);
    }

    // alb 1 high turb
    elev_matrix = dataset + (9 * 6 * 10 + 9 * 6 * (int_turbidity));
    for (unsigned int i = 0; i < 9; ++i)
    {
        //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
        config[i] +=
            (albedo) * (turbidity_rem) *
            (pow(1.0 - solar_elevation, 5.0) * elev_matrix[i] +
             5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[i + 9] +
             10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) *
                 elev_matrix[i + 18] +
             10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) *
                 elev_matrix[i + 27] +
             5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[i + 36] +
             pow(solar_elevation, 5.0) * elev_matrix[i + 45]);
    }
}

double ArHosekSkyModel_CookRadianceConfiguration(
    ArHosekSkyModel_Radiance_Dataset dataset,
    double                           turbidity,
    double                           albedo,
    double                           solar_elevation)
{
    double* elev_matrix;

    int    int_turbidity = (int)turbidity;
    double turbidity_rem = turbidity - (double)int_turbidity;
    double res;
    solar_elevation = pow(solar_elevation / (MATH_PI / 2.0), (1.0 / 3.0));

    // alb 0 low turb
    elev_matrix = dataset + (6 * (int_turbidity - 1));
    //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
    res = (1.0 - albedo) * (1.0 - turbidity_rem) *
          (pow(1.0 - solar_elevation, 5.0) * elev_matrix[0] +
           5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[1] +
           10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) * elev_matrix[2] +
           10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) * elev_matrix[3] +
           5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[4] +
           pow(solar_elevation, 5.0) * elev_matrix[5]);

    // alb 1 low turb
    elev_matrix = dataset + (6 * 10 + 6 * (int_turbidity - 1));
    //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
    res += (albedo) * (1.0 - turbidity_rem) *
           (pow(1.0 - solar_elevation, 5.0) * elev_matrix[0] +
            5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[1] +
            10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) * elev_matrix[2] +
            10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) * elev_matrix[3] +
            5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[4] +
            pow(solar_elevation, 5.0) * elev_matrix[5]);
    if (int_turbidity == 10)
        return res;

    // alb 0 high turb
    elev_matrix = dataset + (6 * (int_turbidity));
    //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
    res += (1.0 - albedo) * (turbidity_rem) *
           (pow(1.0 - solar_elevation, 5.0) * elev_matrix[0] +
            5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[1] +
            10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) * elev_matrix[2] +
            10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) * elev_matrix[3] +
            5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[4] +
            pow(solar_elevation, 5.0) * elev_matrix[5]);

    // alb 1 high turb
    elev_matrix = dataset + (6 * 10 + 6 * (int_turbidity));
    //(1-t).^3* A1 + 3*(1-t).^2.*t * A2 + 3*(1-t) .* t .^ 2 * A3 + t.^3 * A4;
    res += (albedo) * (turbidity_rem) *
           (pow(1.0 - solar_elevation, 5.0) * elev_matrix[0] +
            5.0 * pow(1.0 - solar_elevation, 4.0) * solar_elevation * elev_matrix[1] +
            10.0 * pow(1.0 - solar_elevation, 3.0) * pow(solar_elevation, 2.0) * elev_matrix[2] +
            10.0 * pow(1.0 - solar_elevation, 2.0) * pow(solar_elevation, 3.0) * elev_matrix[3] +
            5.0 * (1.0 - solar_elevation) * pow(solar_elevation, 4.0) * elev_matrix[4] +
            pow(solar_elevation, 5.0) * elev_matrix[5]);
    return res;
}

double ArHosekSkyModel_GetRadianceInternal(
    ArHosekSkyModelConfiguration configuration,
    double                       theta,
    double                       gamma)
{
    const double expM = exp(configuration[4] * gamma);
    const double rayM = cos(gamma) * cos(gamma);
    const double mieM =
        (1.0 + cos(gamma) * cos(gamma)) /
        pow((1.0 + configuration[8] * configuration[8] - 2.0 * configuration[8] * cos(gamma)), 1.5);
    const double zenith = sqrt(cos(theta));

    return (1.0 + configuration[0] * exp(configuration[1] / (cos(theta) + 0.01))) *
           (configuration[2] + configuration[3] * expM + configuration[5] * rayM +
            configuration[6] * mieM + configuration[7] * zenith);
}

// spectral version

ArHosekSkyModelState* arhosekskymodelstate_alloc_init(
    const double solar_elevation,
    const double atmospheric_turbidity,
    const double ground_albedo)
{
    ArHosekSkyModelState* state = ALLOC(ArHosekSkyModelState);

    state->solar_radius = TERRESTRIAL_SOLAR_RADIUS;
    state->turbidity = atmospheric_turbidity;
    state->albedo = ground_albedo;
    state->elevation = solar_elevation;

    for (unsigned int wl = 0; wl < 11; ++wl)
    {
        ArHosekSkyModel_CookConfiguration(
            datasets[wl],
            state->configs[wl],
            atmospheric_turbidity,
            ground_albedo,
            solar_elevation);

        state->radiances[wl] = ArHosekSkyModel_CookRadianceConfiguration(
            datasetsRad[wl], atmospheric_turbidity, ground_albedo, solar_elevation);
    }

    return state;
}

void arhosekskymodelstate_free(ArHosekSkyModelState* state) { free(state); }

const int pieces = 45;
const int order = 4;

double arhosekskymodel_sr_internal(int turbidity, int wl, double solar_disk_radius)
{
    int pos = (int)(pow((1.0 - solar_disk_radius), 1.0 / 3.0) * pieces); // floor

    if (pos > 44)
        pos = 44;

    const double break_x = pow(((double)pos / (double)pieces), 3.0) * (MATH_PI * 0.5);

    const double* coefs = solarDatasets[wl] + (order * pieces * turbidity + order * (pos + 1) - 1);

    double       res = 0.0;
    const double elevation = 0.5 * MATH_PI * (1.0 - solar_disk_radius);
    const double x = elevation - break_x;
    double       x_exp = 1.0;

    for (int i = 0; i < order; ++i)
    {
        res += x_exp * (*(coefs--));
        x_exp *= x;
    }

    return res;
}

double arhosekskymodel_solar_disk_radiance(
    ArHosekSkyModelState* state,
    double                gamma,
    double                solar_disk_radius,
    double                wavelength)
{
    assert(
        wavelength >= 320.0 && wavelength <= 720.0 && state->turbidity >= 1.0 &&
        state->turbidity <= 10.0);

    int    turb_low = (int)state->turbidity - 1;
    double turb_frac = state->turbidity - (double)(turb_low + 1);

    if (turb_low == 9)
    {
        turb_low = 8;
        turb_frac = 1.0;
    }

    int    wl_low = (int)((wavelength - 320.0) / 40.0);
    double wl_frac = fmod(wavelength, 40.0) / 40.0;

    if (wl_low == 10)
    {
        wl_low = 9;
        wl_frac = 1.0;
    }

    double direct_radiance =
        (1.0 - turb_frac) *
            ((1.0 - wl_frac) * arhosekskymodel_sr_internal(turb_low, wl_low, solar_disk_radius) +
             wl_frac * arhosekskymodel_sr_internal(turb_low, wl_low + 1, solar_disk_radius)) +
        turb_frac *
            ((1.0 - wl_frac) *
                 arhosekskymodel_sr_internal(turb_low + 1, wl_low, solar_disk_radius) +
             wl_frac * arhosekskymodel_sr_internal(turb_low + 1, wl_low + 1, solar_disk_radius));

    double ldCoefficient[6];

    for (int i = 0; i < 6; i++)
        ldCoefficient[i] = (1.0 - wl_frac) * limbDarkeningDatasets[wl_low][i] +
                           wl_frac * limbDarkeningDatasets[wl_low + 1][i];

    // sun distance to diameter ratio, squared

    const double sol_rad_sin = sin(state->solar_radius);
    const double ar2 = 1 / (sol_rad_sin * sol_rad_sin);
    const double singamma = sin(gamma);
    const double sc2 = fmax(0.0, 1.0 - ar2 * singamma * singamma);
    const double sampleCosine = sqrt(sc2);

    //   The following will be improved in future versions of the model:
    //   here, we directly use fitted 5th order polynomials provided by the
    //   astronomical community for the limb darkening effect. Astronomers need
    //   such accurate fittings for their predictions. However, this sort of
    //   accuracy is not really needed for CG purposes, so an approximated
    //   dataset based on quadratic polynomials will be provided in a future
    //   release.

    const double darkeningFactor =
        ldCoefficient[0] + ldCoefficient[1] * sampleCosine +
        ldCoefficient[2] * pow(sampleCosine, 2.0) + ldCoefficient[3] * pow(sampleCosine, 3.0) +
        ldCoefficient[4] * pow(sampleCosine, 4.0) + ldCoefficient[5] * pow(sampleCosine, 5.0);

    direct_radiance *= darkeningFactor;

    return direct_radiance;
}

double arhosekskymodel_skydome_radiance(
    ArHosekSkyModelState* state,
    double                theta,
    double                gamma,
    double                wavelength)
{
    int low_wl = (int)((wavelength - 320.0) / 40.0);

    if (low_wl < 0 || low_wl >= 11)
        return 0.0f;

    double interp = fmod((wavelength - 320.0) / 40.0, 1.0);

    double val_low = ArHosekSkyModel_GetRadianceInternal(state->configs[low_wl], theta, gamma) *
                     state->radiances[low_wl];

    if (interp < 1e-6)
        return val_low;

    double result = (1.0 - interp) * val_low;

    if (low_wl + 1 < 11)
    {
        result += interp *
                  ArHosekSkyModel_GetRadianceInternal(state->configs[low_wl + 1], theta, gamma) *
                  state->radiances[low_wl + 1];
    }

    return result;
}

double arhosekskymodel_radiance(
    ArHosekSkyModelState* state,
    double                theta,
    double                gamma,
    double                wavelength)
{
    const double solar_disk_radius = gamma / TERRESTRIAL_SOLAR_RADIUS;
    const double solar_radiance =
        gamma < TERRESTRIAL_SOLAR_RADIUS
            ? arhosekskymodel_solar_disk_radiance(state, gamma, solar_disk_radius, wavelength)
            : 0.0;

    const double inscattered_radiance =
        arhosekskymodel_skydome_radiance(state, theta, gamma, wavelength);

    return solar_radiance + inscattered_radiance;
}

// xyz and rgb versions

ArHosekSkyModelState* arhosek_xyz_skymodelstate_alloc_init(
    const double turbidity,
    const double albedo,
    const double elevation)
{
    ArHosekSkyModelState* state = ALLOC(ArHosekSkyModelState);

    state->solar_radius = TERRESTRIAL_SOLAR_RADIUS;
    state->turbidity = turbidity;
    state->albedo = albedo;
    state->elevation = elevation;

    for (unsigned int channel = 0; channel < 3; ++channel)
    {
        ArHosekSkyModel_CookConfiguration(
            datasetsXYZ[channel], state->configs[channel], turbidity, albedo, elevation);

        state->radiances[channel] = ArHosekSkyModel_CookRadianceConfiguration(
            datasetsXYZRad[channel], turbidity, albedo, elevation);
    }

    return state;
}

ArHosekSkyModelState* arhosek_rgb_skymodelstate_alloc_init(
    const double turbidity,
    const double albedo,
    const double elevation)
{
    ArHosekSkyModelState* state = ALLOC(ArHosekSkyModelState);

    state->solar_radius = TERRESTRIAL_SOLAR_RADIUS;
    state->turbidity = turbidity;
    state->albedo = albedo;
    state->elevation = elevation;

    for (unsigned int channel = 0; channel < 3; ++channel)
    {
        ArHosekSkyModel_CookConfiguration(
            datasetsRGB[channel], state->configs[channel], turbidity, albedo, elevation);

        state->radiances[channel] = ArHosekSkyModel_CookRadianceConfiguration(
            datasetsRGBRad[channel], turbidity, albedo, elevation);
    }

    return state;
}

double arhosek_tristim_skymodel_radiance(
    ArHosekSkyModelState* state,
    double                theta,
    double                gamma,
    int                   channel)
{
    return ArHosekSkyModel_GetRadianceInternal(state->configs[channel], theta, gamma) *
           state->radiances[channel];
}

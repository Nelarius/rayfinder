struct VertexInput {
    @location(0) position: vec2f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texCoord: vec2f,
}

@vertex
fn vsMain(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4f(in.position, 0.0, 1.0);
    out.texCoord = 0.5 * in.position + vec2f(0.5);

    return out;
}

struct SkyState {
    params: array<f32, 27>,
    skyRadiances: array<f32, 3>,
    solarRadiances: array<f32, 3>,
    sunDirection: vec3<f32>,
};

struct Uniforms {
    inverseViewProjectionMat: mat4x4f,
    cameraEye: vec4f
}

@group(0) @binding(0) var<storage, read> skyState: SkyState;
@group(1) @binding(0) var<uniform> uniforms: Uniforms;

const CHANNEL_R = 0u;
const CHANNEL_G = 1u;
const CHANNEL_B = 2u;

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    let uv = in.texCoord;
    let world = worldFromUv(uv);
    let v = normalize((world - uniforms.cameraEye).xyz);
    let s = skyState.sunDirection;

    let theta = acos(v.y);
    let gamma = acos(clamp(dot(v, s), -1f, 1f));
    let color = vec3f(
        skyRadiance(theta, gamma, CHANNEL_R),
        skyRadiance(theta, gamma, CHANNEL_G),
        skyRadiance(theta, gamma, CHANNEL_B)
    );

    let exposure = 1f / pow(2f, 4);
    return vec4(acesFilmic(exposure * color), 1.0);
}

fn worldFromUv(uv: vec2f) -> vec4f {
    let ndc = vec4(2.0 * uv - vec2(1.0), 0.0, 1.0);
    let worldInvW = uniforms.inverseViewProjectionMat * ndc;
    let world = worldInvW / worldInvW.w;
    return world;
}

const PI = 3.1415927f;
const DEGREES_TO_RADIANS = PI / 180f;
const TERRESTRIAL_SOLAR_RADIUS = 0.255f * DEGREES_TO_RADIANS;

@must_use
fn skyRadiance(theta: f32, gamma: f32, channel: u32) -> f32 {
    // Sky dome radiance
    let r = skyState.skyRadiances[channel];
    let idx = 9u * channel;
    let p0 = skyState.params[idx + 0u];
    let p1 = skyState.params[idx + 1u];
    let p2 = skyState.params[idx + 2u];
    let p3 = skyState.params[idx + 3u];
    let p4 = skyState.params[idx + 4u];
    let p5 = skyState.params[idx + 5u];
    let p6 = skyState.params[idx + 6u];
    let p7 = skyState.params[idx + 7u];
    let p8 = skyState.params[idx + 8u];

    let cosGamma = cos(gamma);
    let cosGamma2 = cosGamma * cosGamma;
    let cosTheta = abs(cos(theta));

    let expM = exp(p4 * gamma);
    let rayM = cosGamma2;
    let mieMLhs = 1.0 + cosGamma2;
    let mieMRhs = pow(1.0 + p8 * p8 - 2.0 * p8 * cosGamma, 1.5f);
    let mieM = mieMLhs / mieMRhs;
    let zenith = sqrt(cosTheta);
    let radianceLhs = 1.0 + p0 * exp(p1 / (cosTheta + 0.01));
    let radianceRhs = p2 + p3 * expM + p5 * rayM + p6 * mieM + p7 * zenith;
    let radianceDist = radianceLhs * radianceRhs;

    // Solar radiance
    let solarDiskRadius = gamma / TERRESTRIAL_SOLAR_RADIUS;
    let solarRadiance = select(0f, skyState.solarRadiances[channel], solarDiskRadius <= 1f);

    return r * radianceDist + solarRadiance;
}

@must_use
fn acesFilmic(x: vec3f) -> vec3f {
    let a = 2.51f;
    let b = 0.03f;
    let c = 2.43f;
    let d = 0.59f;
    let e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

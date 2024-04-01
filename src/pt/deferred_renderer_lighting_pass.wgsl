struct VertexInput {
    @location(0) position: vec2f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texCoord: vec2f,
}

@vertex
fn vsMain(in: VertexInput) -> VertexOutput {
    let uv = 0.5 * in.position + vec2f(0.5);
    var out: VertexOutput;
    out.position = vec4f(in.position, 0.0, 1.0);
    out.texCoord = vec2f(uv.x, 1.0 - uv.y);

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
    cameraEye: vec4f,
    framebufferSize: vec2f,
    exposure: f32,
}

@group(0) @binding(0) var<storage, read> skyState: SkyState;
@group(1) @binding(0) var<uniform> uniforms: Uniforms;
@group(2) @binding(0) var gbufferAlbedo: texture_2d<f32>;
@group(2) @binding(1) var gbufferNormal: texture_2d<f32>;
@group(2) @binding(2) var gbufferDepth: texture_depth_2d;

const CHANNEL_R = 0u;
const CHANNEL_G = 1u;
const CHANNEL_B = 2u;

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    var color = vec3f(0.0, 0.0, 0.0);

    let uv = in.texCoord;
    let idx = vec2u(floor(uv * uniforms.framebufferSize));
    let d = textureLoad(gbufferDepth, idx, 0);
    if d == 1.0 {
        let world = worldFromUv(uv);
        let v = normalize((world - uniforms.cameraEye).xyz);
        let s = skyState.sunDirection;

        let theta = acos(v.y);
        let gamma = acos(clamp(dot(v, s), -1f, 1f));
        color = vec3f(
            skyRadiance(theta, gamma, CHANNEL_R),
            skyRadiance(theta, gamma, CHANNEL_G),
            skyRadiance(theta, gamma, CHANNEL_B)
        );
    } else {
        let albedo = textureLoad(gbufferAlbedo, idx, 0).rgb;
        color = albedo;
    }

    return vec4(acesFilmic(uniforms.exposure * color), 1.0);
}

fn worldFromUv(uv: vec2f) -> vec4f {
    let ndc = vec4(2.0 * vec2(uv.x, 1.0 - uv.y) - vec2(1.0), 0.0, 1.0);
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

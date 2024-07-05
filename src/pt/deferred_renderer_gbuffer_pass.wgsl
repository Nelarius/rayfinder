struct Uniforms {
    currentViewReverseZProjectionMat: mat4x4f,
    previousViewReverseZProjectionMat: mat4x4f,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

struct VertexInput {
    @location(0) position: vec4f,
    @location(1) normal: vec4f,
    @location(2) texCoord: vec2f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    // TODO: is there a better way? perhaps use position in fragment shader?
    @location(0) currentClip: vec4f,
    @location(1) previousClip: vec4f,
    @location(2) normal: vec4f,
    @location(3) texCoord: vec2f,
}

@vertex
fn vsMain(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uniforms.currentViewReverseZProjectionMat * in.position;
    out.currentClip = out.position;
    out.previousClip = uniforms.previousViewReverseZProjectionMat * in.position;
    out.normal = in.normal;
    out.texCoord = in.texCoord;
    return out;
}

@group(1) @binding(0) var textureSampler: sampler;
@group(2) @binding(0) var texture: texture_2d<f32>;

struct GbufferOutput {
    @location(0) albedo: vec4<f32>,
    @location(1) normal: vec4<f32>,
    @location(2) velocity: vec2<f32>,
}

@fragment
fn fsMain(in: VertexOutput) -> GbufferOutput {
    let linearAlbedo = pow(textureSample(texture, textureSampler, in.texCoord).xyz, vec3(2.2f));
    let encodedNormal = 0.5f * in.normal.xyz + vec3f(0.5f);
    let currentNdc = in.currentClip.xy / in.currentClip.w;
    let previousNdc = in.previousClip.xy / in.previousClip.w;
    // TODO: can this be rolled into uvVelocity? compute ndcVelocity first.
    let currentUv = currentNdc * vec2f(0.5f, -0.5f) + vec2f(0.5f);
    let previousUv = previousNdc * vec2f(0.5f, -0.5f) + vec2f(0.5f);
    let uvVelocity = previousUv - currentUv;    // From the current pixel to the previous one
    return GbufferOutput(vec4(linearAlbedo, 1f), vec4(encodedNormal, 1f), uvVelocity);
}

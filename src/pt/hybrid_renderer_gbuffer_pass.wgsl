@group(0) @binding(0) var<uniform> viewProjectionMat: mat4x4f;

struct VertexInput {
    @location(0) position: vec4f,
    @location(1) normal: vec4f,
    @location(2) texCoord: vec2f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) normal: vec4f,
    @location(1) texCoord: vec2f,
}

@vertex
fn vsMain(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = viewProjectionMat * in.position;
    out.normal = in.normal;
    out.texCoord = in.texCoord;
    return out;
}

@group(1) @binding(0) var textureSampler: sampler;
@group(2) @binding(0) var texture: texture_2d<f32>;

struct GbufferOutput {
    @location(0) albedo: vec4<f32>,
    @location(1) normal: vec4<f32>,
}

@fragment
fn fsMain(in: VertexOutput) -> GbufferOutput {
    var out: GbufferOutput;
    out.albedo = textureSample(texture, textureSampler, in.texCoord);
    out.normal = vec4(normalize(in.normal.xyz), 1.0);
    return out;
}

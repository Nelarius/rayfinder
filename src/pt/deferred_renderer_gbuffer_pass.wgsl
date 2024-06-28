struct Uniforms {
    viewReverseZProjectionMat: mat4x4f,
    jitterMat: mat4x4f
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

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
    out.position = uniforms.jitterMat * uniforms.viewReverseZProjectionMat * in.position;
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
    let linearAlbedo = pow(textureSample(texture, textureSampler, in.texCoord).xyz, vec3(2.2f));
    let encodedNormal = 0.5f * in.normal.xyz + vec3f(0.5f);
    return GbufferOutput(vec4(linearAlbedo, 1f), vec4(encodedNormal, 1f));
}

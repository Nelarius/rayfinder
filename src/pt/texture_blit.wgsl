struct Uniforms {
    viewProjectionMatrix: mat4x4f,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

struct VertexInput {
    @location(0) position: vec2f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texCoord: vec2f,
}

@vertex
fn vsMain(in: VertexInput) -> VertexOutput {
    let uv = in.position + vec2f(0.5);
    var out: VertexOutput;
    out.position = uniforms.viewProjectionMatrix * vec4f(in.position, 0.0, 1.0);
    out.texCoord = vec2f(uv.x, 1.0 - uv.y); // flip y axis

    return out;
}

@group(1) @binding(0) var texture: texture_2d<f32>;
@group(1) @binding(1) var textureSampler: sampler;

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    return textureSample(texture, textureSampler, in.texCoord);
}

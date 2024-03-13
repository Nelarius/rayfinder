@group(0) @binding(0) var<uniform> viewProjectionMat: mat4x4f;

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texCoord: vec2f,
}

@vertex
fn vsMain(@location(0) position: vec4f, @location(1) texCoord: vec2f) -> VertexOutput {
    var out: VertexOutput;
    out.position = viewProjectionMat * position;
    out.texCoord = texCoord;
    return out;
}

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    return vec4(in.texCoord, 0.0, 1.0);
}

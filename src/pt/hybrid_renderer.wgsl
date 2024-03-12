@group(0) @binding(0) var<uniform> viewProjectionMat: mat4x4f;

struct VertexOutput {
    @builtin(position) position: vec4f
}

@vertex
fn vsMain(@location(0) position: vec4f) -> VertexOutput {
    var out: VertexOutput;
    out.position = viewProjectionMat * position;
    return out;
}

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    return vec4f(1.0, 0.0, 0.0, 1.0);
}

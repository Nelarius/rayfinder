struct Uniforms {
    viewProjectionMatrix: mat4x4<f32>,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

struct FrameData {
    dimensions: vec2u,
    frameCount: u32,
}

@group(1) @binding(0) var<uniform> frameData: FrameData;
@group(1) @binding(1) var<storage, read_write> pixelBuffer: array<array<f32, 3>>;

struct VertexInput {
    @location(0) position: vec2f,
    @location(1) texCoord: vec2f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texCoord: vec2f,
}

@vertex
fn vsMain(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uniforms.viewProjectionMatrix * vec4f(in.position, 0.0, 1.0);
    out.texCoord = in.texCoord;

    return out;
}

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    let u = in.texCoord.x;
    let v = in.texCoord.y;

    let j =  u32(u * f32(frameData.dimensions.x));
    let i =  u32(v * f32(frameData.dimensions.y));
    let idx = frameData.dimensions.x * i + j;
    let p = pixelBuffer[idx];

    return vec4f(p[0], p[1], p[2], 1.0);
}

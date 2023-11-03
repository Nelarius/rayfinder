struct FrameData {
    dimensions: vec2u,
    frameCount: u32,
}

@group(0) @binding(0) var<uniform> frameData: FrameData;
@group(0) @binding(1) var<storage, read_write> pixelBuffer: array<array<f32, 3>>;

@compute @workgroup_size(8,8)
fn main(@builtin(global_invocation_id) globalId: vec3u) {
    let j = globalId.x;
    let i = globalId.y;

    var rngState = initRng(vec2(j, i), frameData.dimensions, frameData.frameCount);
    let v = rngNextFloat(&rngState);

    let r = rngNextFloat(&rngState);
    let g = rngNextFloat(&rngState);
    let b = rngNextFloat(&rngState);

    if j < frameData.dimensions.x && i < frameData.dimensions[1] {
        let idx = frameData.dimensions.x * i + j;
        pixelBuffer[idx] = array<f32, 3>(r, g, b);
    }
}

fn initRng(pixel: vec2<u32>, resolution: vec2<u32>, frame: u32) -> u32 {
    // Adapted from https://github.com/boksajak/referencePT
    let seed = dot(pixel, vec2<u32>(1u, resolution.x)) ^ jenkinsHash(frame);
    return jenkinsHash(seed);
}

fn jenkinsHash(input: u32) -> u32 {
    var x = input;
    x += x << 10u;
    x ^= x >> 6u;
    x += x << 3u;
    x ^= x >> 11u;
    x += x << 15u;
    return x;
}

fn rngNextFloat(state: ptr<function, u32>) -> f32 {
    rngNextInt(state);
    return f32(*state) / f32(0xffffffffu);
}

fn rngNextInt(state: ptr<function, u32>) {
    // PCG random number generator
    // Based on https://www.shadertoy.com/view/XlGcRh

    let oldState = *state + 747796405u + 2891336453u;
    let word = ((oldState >> ((oldState >> 28u) + 4u)) ^ oldState) * 277803737u;
    *state = (word >> 22u) ^ word;
}

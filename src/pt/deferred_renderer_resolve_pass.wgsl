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

struct Uniforms {
    framebufferSize: vec2f,
    exposure: f32,
    frameCount: u32,
}

// TODO: consider merging these into one bind group

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

@group(1) @binding(0) var<storage, read_write> sampleBuffer: array<array<f32, 3>>;
@group(1) @binding(1) var<storage, read_write> accumulationBuffer: array<array<f32, 3>>;
@group(1) @binding(2) var gbufferVelocity: texture_2d<f32>;

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    let uv = in.texCoord;
    let textureIdx = vec2u(floor(uv * uniforms.framebufferSize));
    let sampleBufferIdx = textureIdxToLinearIdx(textureIdx);
    let currentColor = bilinearSample(uv);
    var color = vec3f(0f);
    if uniforms.frameCount == 0u {
        color = currentColor;
        accumulationBuffer[sampleBufferIdx] = array<f32, 3>(color.r, color.g, color.b);
    } else {
        let uvVelocity = bilinearVelocity(uv);
        let previousUv = uv + uvVelocity;
        if previousUv.x >= 0f && previousUv.x < 1f && previousUv.y >= 0f && previousUv.y < 1f {
            let previousColor = bilinearAccumulationSample(previousUv);
            color = 0.1 * currentColor + 0.9 * previousColor;
            accumulationBuffer[sampleBufferIdx] = array<f32, 3>(color.r, color.g, color.b);
        } else {
            color = currentColor;
            accumulationBuffer[sampleBufferIdx] = array<f32, 3>(color.r, color.g, color.b);
        }
    }

    let rgb = acesFilmic(uniforms.exposure * color);
    let srgb = pow(rgb, vec3(1f / 2.2f));
    return vec4(srgb, 1f);
}

fn bilinearSample(uv: vec2f) -> vec3f {
    let textureIdx = vec2u(floor(uv * uniforms.framebufferSize));
    let stride = u32(uniforms.framebufferSize.x);
    let tlIdx = textureIdxToLinearIdx(textureIdx);
    let trIdx = textureIdxToLinearIdx(textureIdx + vec2u(1u, 0u));
    let blIdx = textureIdxToLinearIdx(textureIdx + vec2u(0u, 1u));
    let brIdx = textureIdxToLinearIdx(textureIdx + vec2u(1u, 1u));
    let tl: array<f32, 3> = sampleBuffer[tlIdx];
    let tr: array<f32, 3> = sampleBuffer[trIdx];
    let bl: array<f32, 3> = sampleBuffer[blIdx];
    let br: array<f32, 3> = sampleBuffer[brIdx];
    let ctl = vec3f(tl[0], tl[1], tl[2]);
    let ctr = vec3f(tr[0], tr[1], tr[2]);
    let cbl = vec3f(bl[0], bl[1], bl[2]);
    let cbr = vec3f(br[0], br[1], br[2]);
    let tx = fract(uv.x * uniforms.framebufferSize.x);
    let top = mix(ctl, ctr, tx);
    let bottom = mix(cbl, cbr, tx);
    let ty = fract(uv.y * uniforms.framebufferSize.y);
    return mix(top, bottom, ty);
}

fn bilinearAccumulationSample(uv: vec2f) -> vec3f {
    let textureIdx = vec2u(floor(uv * uniforms.framebufferSize));
    let stride = u32(uniforms.framebufferSize.x);
    let tlIdx = textureIdxToLinearIdx(textureIdx);
    let trIdx = textureIdxToLinearIdx(textureIdx + vec2u(1u, 0u));
    let blIdx = textureIdxToLinearIdx(textureIdx + vec2u(0u, 1u));
    let brIdx = textureIdxToLinearIdx(textureIdx + vec2u(1u, 1u));
    let tl: array<f32, 3> = accumulationBuffer[tlIdx];
    let tr: array<f32, 3> = accumulationBuffer[trIdx];
    let bl: array<f32, 3> = accumulationBuffer[blIdx];
    let br: array<f32, 3> = accumulationBuffer[brIdx];
    let ctl = vec3f(tl[0], tl[1], tl[2]);
    let ctr = vec3f(tr[0], tr[1], tr[2]);
    let cbl = vec3f(bl[0], bl[1], bl[2]);
    let cbr = vec3f(br[0], br[1], br[2]);
    let tx = fract(uv.x * uniforms.framebufferSize.x);
    let top = mix(ctl, ctr, tx);
    let bottom = mix(cbl, cbr, tx);
    let ty = fract(uv.y * uniforms.framebufferSize.y);
    return mix(top, bottom, ty);
}

@must_use
fn bilinearVelocity(uv: vec2f) -> vec2f {
    let textureIdx = vec2u(floor(uv * uniforms.framebufferSize));
    let tl = textureLoad(gbufferVelocity, textureIdx, 0).rg;
    let tr = textureLoad(gbufferVelocity, textureIdx + vec2u(1u, 0u), 0).rg;
    let bl = textureLoad(gbufferVelocity, textureIdx + vec2u(0u, 1u), 0).rg;
    let br = textureLoad(gbufferVelocity, textureIdx + vec2u(1u, 1u), 0).rg;
    let tx = fract(uv.x * uniforms.framebufferSize.x);
    let top = mix(tl, tr, tx);
    let bottom = mix(bl, br, tx);
    let ty = fract(uv.y * uniforms.framebufferSize.y);
    return mix(top, bottom, ty);
}

@must_use
fn textureIdxToLinearIdx(idx: vec2u) -> u32 {
    let clampedIdx = min(idx, vec2u(uniforms.framebufferSize) - vec2u(1));
    return clampedIdx.y * u32(uniforms.framebufferSize.x) + clampedIdx.x;
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

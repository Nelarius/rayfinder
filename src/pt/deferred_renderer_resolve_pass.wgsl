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
    currentInverseViewProjectionMat: mat4x4f,
    previousViewProjectionMat: mat4x4f,
    framebufferSize: vec2f,
    exposure: f32,
    frameCount: u32,
}

// TODO: consider merging these into one bind group

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

@group(1) @binding(0) var<storage, read_write> sampleBuffer: array<array<f32, 3>>;
@group(1) @binding(1) var<storage, read_write> accumulationBuffer: array<array<f32, 3>>;

@group(2) @binding(0) var gbufferDepth: texture_depth_2d;

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    let uv = in.texCoord;
    let textureIdx = vec2u(floor(uv * uniforms.framebufferSize));
    let sampleBufferIdx = textureIdx.y * u32(uniforms.framebufferSize.x) + textureIdx.x;
    let sample: array<f32, 3> = sampleBuffer[sampleBufferIdx];
    let currentColor = vec3f(sample[0], sample[1], sample[2]);

    var outputColor = vec3f(0f);
    if uniforms.frameCount == 0u {
        accumulationBuffer[sampleBufferIdx] = sample;
        outputColor = currentColor;
    } else {
        let depth = textureLoad(gbufferDepth, textureIdx, 0);
        // let depth = bilinearDepth(uv);
        let previousUv = cameraReproject(uv, depth);
        // let previousColor = bilinearSample(previousUv);
        let previousTextureIdx = vec2u(floor(previousUv * uniforms.framebufferSize));
        let previousSampleBufferIdx = previousTextureIdx.y * u32(uniforms.framebufferSize.x) + previousTextureIdx.x;
        let previousSample: array<f32, 3> = accumulationBuffer[previousSampleBufferIdx];
        let previousColor = vec3f(previousSample[0], previousSample[1], previousSample[2]);
        outputColor = 0.1 * currentColor + 0.9 * previousColor;
        accumulationBuffer[sampleBufferIdx] = array<f32, 3>(outputColor.r, outputColor.g, outputColor.b);
    }

    let rgb = acesFilmic(uniforms.exposure * outputColor);
    let srgb = pow(rgb, vec3(1f / 2.2f));
    return vec4(srgb, 1f);
}

@must_use
fn cameraReproject(uv: vec2f, depth: f32) -> vec2f {
    let ndc = vec4f(2f * vec2(uv.x, 1f - uv.y) - vec2(1f), depth, 1f);
    let worldInvW = uniforms.currentInverseViewProjectionMat * ndc;
    let world = worldInvW / worldInvW.w;
    let previousClip = uniforms.previousViewProjectionMat * world;
    let previousNdc = previousClip / previousClip.w;
    let flippedUv = 0.5 * previousNdc.xy + 0.5;
    return vec2(flippedUv.x, 1.0 - flippedUv.y);
}

@must_use
fn bilinearSample(uv: vec2f) -> vec3f {
    let textureIdx = vec2u(floor(uv * uniforms.framebufferSize));
    let stride = u32(uniforms.framebufferSize.x);
    let tlIdx = textureIdx.y * stride + textureIdx.x;
    let trIdx = tlIdx + 1u;
    let blIdx = (textureIdx.y + 1u) * stride + textureIdx.x;
    let brIdx = blIdx + 1u;
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
fn bilinearDepth(uv: vec2f) -> f32 {
    let textureIdx = vec2u(floor(uv * uniforms.framebufferSize));
    let tl = textureLoad(gbufferDepth, textureIdx, 0);
    let tr = textureLoad(gbufferDepth, textureIdx + vec2u(1u, 0u), 0);
    let bl = textureLoad(gbufferDepth, textureIdx + vec2u(0u, 1u), 0);
    let br = textureLoad(gbufferDepth, textureIdx + vec2u(1u, 1u), 0);
    let tx = fract(uv.x * uniforms.framebufferSize.x);
    let top = mix(tl, tr, tx);
    let bottom = mix(bl, br, tx);
    let ty = fract(uv.y * uniforms.framebufferSize.y);
    return mix(top, bottom, ty);
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

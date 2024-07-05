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

    var color = vec3f(0f);
    if uniforms.frameCount == 0u {
        accumulationBuffer[sampleBufferIdx] = sample;
        color = currentColor;
    } else {
        let depth = textureLoad(gbufferDepth, textureIdx, 0);
        let previousUv = cameraReproject(uv, depth);
        if previousUv.x >= 0f && previousUv.x < 1f && previousUv.y >= 0f && previousUv.y < 1f {
            let previousTextureIdx = vec2u(floor(previousUv * uniforms.framebufferSize));
            let previousSampleBufferIdx = previousTextureIdx.y * u32(uniforms.framebufferSize.x) + previousTextureIdx.x;
            let previousSample: array<f32, 3> = accumulationBuffer[previousSampleBufferIdx];
            let previousColor = vec3f(previousSample[0], previousSample[1], previousSample[2]);
            color = 0.1 * currentColor + 0.9 * previousColor;
            accumulationBuffer[sampleBufferIdx] = array<f32, 3>(color.r, color.g, color.b);
        } else {
            accumulationBuffer[sampleBufferIdx] = sample;
            color = currentColor;
        }
    }

    let rgb = acesFilmic(uniforms.exposure * color);
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
fn acesFilmic(x: vec3f) -> vec3f {
    let a = 2.51f;
    let b = 0.03f;
    let c = 2.43f;
    let d = 0.59f;
    let e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

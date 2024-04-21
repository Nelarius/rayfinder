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

@group(0) @binding(0) var<uniform> framebufferSize: vec2f;
@group(1) @binding(0) var gbufferAlbedo: texture_2d<f32>;
@group(1) @binding(1) var gbufferNormal: texture_2d<f32>;
@group(1) @binding(2) var gbufferDepth: texture_depth_2d;

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    let c = in.texCoord;
    let idx = vec2u(floor(c * framebufferSize));
    if c.x < 0.333 {
        return textureLoad(gbufferAlbedo, idx, 0);
    } else if c.x < 0.666 {
        return textureLoad(gbufferNormal, idx, 0);
    } else {
        let d = vec3(1.0) - textureLoad(gbufferDepth, idx, 0);
        let x = d;
        let a = 0.1;
        return vec4((1.0 + a) * x / (x + vec3(a)), 1.0);
    }
}

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
@group(1) @binding(3) var gbufferVelocity: texture_2d<f32>;

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    let c = in.texCoord;
    let idx = vec2u(floor(c * framebufferSize));
    var rgb = vec3f(0f);
    if c.x < 0.25 {
        rgb = textureLoad(gbufferAlbedo, idx, 0).rgb;
    } else if c.x < 0.5 {
        rgb = textureLoad(gbufferNormal, idx, 0).rgb;
    } else if c.x < 0.75 {
        let d = textureLoad(gbufferDepth, idx, 0);
        let x = d;
        let a = 0.1;
        rgb = vec3((1.0 + a) * x / (x + vec3(a)));
    } else {
        let velocity = textureLoad(gbufferVelocity, idx, 0).rg;
        rgb = vec3f(50f * abs(velocity), 0.0);
    }
    let srgb = pow(rgb, vec3(1.0 / 2.2));
    return vec4(srgb, 1.0);
}

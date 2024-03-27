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
    out.texCoord = vec2f(uv.x, 1.0 - uv.y); // flip y axis

    return out;
}

@group(0) @binding(0) var textureSampler: sampler;
@group(0) @binding(1) var gbufferAlbedo: texture_2d<f32>;
@group(0) @binding(2) var gbufferNormal: texture_2d<f32>;

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    // NOTE: textureSample can't be called from non-uniform control flow
    // TODO: replace with textureLoad calls which can be called from non-uniform control flow and get rid of the sampler
    let c = in.texCoord;
    let a = textureSample(gbufferAlbedo, textureSampler, c);
    let n = textureSample(gbufferNormal, textureSampler, c);
    if c.x < 0.5 {
        return a;
    } else {
        return vec4(vec3(0.5) * (n.xyz + vec3(1f)), 1.0);
    }
}

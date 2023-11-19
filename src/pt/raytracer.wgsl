struct Uniforms {
    viewProjectionMatrix: mat4x4f,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

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

// render params bind group
@group(1) @binding(0) var<uniform> renderParams: RenderParams;

// scene bind group
// TODO: these are `read` only buffers. How can I create a buffer layout type which allows this?
// Annotating these as read causes validation failures.
@group(2) @binding(0) var<storage, read_write> bvhNodes: array<BvhNode>;
@group(2) @binding(1) var<storage, read_write> triangles: array<Triangle>;
@group(2) @binding(2) var<storage, read_write> normals: array<array<vec3f, 3>>;
@group(2) @binding(3) var<storage, read_write> texCoords: array<array<vec2f, 3>>;
@group(2) @binding(4) var<storage, read_write> textureDescriptorIndices: array<u32>;
@group(2) @binding(5) var<storage, read_write> textureDescriptors: array<TextureDescriptor>;
@group(2) @binding(6) var<storage, read_write> textures: array<u32>;

// image bind group
@group(3) @binding(0) var<storage, read_write> imageBuffer: array<vec3f>;

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    let u = in.texCoord.x;
    let v = in.texCoord.y;

    let dimensions = renderParams.frameData.dimensions;
    let frameCount = renderParams.frameData.frameCount;

    let j = u32(u * f32(dimensions.x));
    let i = u32(v * f32(dimensions.y));
    let idx = i * dimensions.x + j;

    var accumulatedSampleCount = renderParams.samplingState.accumulatedSampleCount;

    if accumulatedSampleCount == 0u {
        imageBuffer[idx] = vec3(0f);
    }

    if accumulatedSampleCount < renderParams.samplingState.numSamplesPerPixel {
        var rngState = initRng(vec2(j, i), dimensions, frameCount);
        let uoffset = rngNextFloat(&rngState) / f32(dimensions.x);
        let voffset = rngNextFloat(&rngState) / f32(dimensions.y);
        let primaryRay = generateCameraRay(renderParams.camera, &rngState, u + uoffset, v + voffset);
        imageBuffer[idx] += rayColor(primaryRay, &rngState);
        accumulatedSampleCount += 1u;
    }

    let estimator = imageBuffer[idx] / f32(accumulatedSampleCount);
    let rgb = expose(estimator, 0.1f);

    return vec4f(rgb, 1f);
}

const EPSILON = 0.00001f;

const PI = 3.1415927f;
const FRAC_1_PI = 0.31830987f;
const FRAC_PI_2 = 1.5707964f;

const T_MIN = 0.001f;
const T_MAX = 10000f;

const NUM_BOUNCES = 4u;
const UNIFORM_HEMISPHERE_MULTIPLIER = 2f * PI;

const CHANNEL_R = 0u;
const CHANNEL_G = 1u;
const CHANNEL_B = 2u;

struct RenderParams {
  frameData: FrameData,
  camera: Camera,
  samplingState: SamplingState,
  skyState: SkyState,
}

struct FrameData {
    dimensions: vec2u,
    frameCount: u32,
}

struct Camera {
    origin: vec3f,
    lowerLeftCorner: vec3f,
    horizontal: vec3f,
    vertical: vec3f,
    lensRadius: f32,
}

struct SamplingState {
    numSamplesPerPixel: u32,
    numBounces: u32,
    accumulatedSampleCount: u32,
}

struct SkyState {
    params: array<f32, 27>,
    radiances: array<f32, 3>,
    sunDirection: vec3<f32>,
};

struct Aabb {
    min: vec3f,
    max: vec3f,
}

struct BvhNode {
    aabb: Aabb,
    trianglesOffset: u32,
    secondChildOffset: u32,
    triangleCount: u32,
    splitAxis: u32,
}

// TODO: rename to positions
struct Triangle {
    v0: vec3f,
    v1: vec3f,
    v2: vec3f,
}

struct Ray {
    origin: vec3f,
    direction: vec3f
}

struct Intersection {
    p: vec3f,
    n: vec3f,
    uv: vec2f,
    triangleIdx: u32,
}

struct TriangleHit {
    p: vec3f,
    b: vec3f,
    t: f32,
}

struct Scatter {
    wi: vec3f,
    throughput: vec3f,
}

fn rayColor(primaryRay: Ray, rngState: ptr<function, u32>) -> vec3f {
    var ray = primaryRay;

    var color = vec3(0f);
    var throughput = vec3(1f);
    for (var bounces = 0u; bounces < NUM_BOUNCES; bounces += 1u) {
        var intersection: Intersection;
        if rayIntersectBvh(ray, T_MAX, &intersection) {
            let p = intersection.p;
            let scatter = evalImplicitLambertian(intersection, rngState);
            ray = Ray(p, scatter.wi);
            throughput *= scatter.throughput;
        } else {
            // TODO: I don't think this needs to be normalized
            let v = normalize(ray.direction);
            let s = renderParams.skyState.sunDirection;

            let theta = acos(v.y);
            let gamma = acos(clamp(dot(v, s), -1f, 1f));

            let skyRadiance = vec3f(
                radiance(theta, gamma, CHANNEL_R),
                radiance(theta, gamma, CHANNEL_G),
                radiance(theta, gamma, CHANNEL_B)
            );

            color += throughput * skyRadiance;

            break;
        }
    }

    return color;
}

fn generateCameraRay(camera: Camera, rngState: ptr<function, u32>, u: f32, v: f32) -> Ray {
    let origin = camera.origin;
    let direction = camera.lowerLeftCorner + u * camera.horizontal + v * camera.vertical - origin;

    return Ray(origin, direction);
}

@must_use
fn radiance(theta: f32, gamma: f32, channel: u32) -> f32 {
    let r = renderParams.skyState.radiances[channel];
    let idx = 9u * channel;
    let p0 = renderParams.skyState.params[idx + 0u];
    let p1 = renderParams.skyState.params[idx + 1u];
    let p2 = renderParams.skyState.params[idx + 2u];
    let p3 = renderParams.skyState.params[idx + 3u];
    let p4 = renderParams.skyState.params[idx + 4u];
    let p5 = renderParams.skyState.params[idx + 5u];
    let p6 = renderParams.skyState.params[idx + 6u];
    let p7 = renderParams.skyState.params[idx + 7u];
    let p8 = renderParams.skyState.params[idx + 8u];

    let cosGamma = cos(gamma);
    let cosGamma2 = cosGamma * cosGamma;
    let cosTheta = abs(cos(theta));

    let expM = exp(p4 * gamma);
    let rayM = cosGamma2;
    let mieMLhs = 1.0 + cosGamma2;
    let mieMRhs = pow(1.0 + p8 * p8 - 2.0 * p8 * cosGamma, 1.5f);
    let mieM = mieMLhs / mieMRhs;
    let zenith = sqrt(cosTheta);
    let radianceLhs = 1.0 + p0 * exp(p1 / (cosTheta + 0.01));
    let radianceRhs = p2 + p3 * expM + p5 * rayM + p6 * mieM + p7 * zenith;
    let radianceDist = radianceLhs * radianceRhs;
    return r * radianceDist;
}

@must_use
fn expose(v: vec3f, exposure: f32) -> vec3f {
    return vec3(2.0f) / (vec3(1.0f) + exp(-exposure * v)) - vec3(1.0f);
}

fn evalImplicitLambertian(hit: Intersection, rngState: ptr<function, u32>) -> Scatter {
    let v = rngNextInCosineWeightedHemisphere(rngState);
    let onb = pixarOnb(hit.n);
    let wi = onb * v;

    let textureDesc = textureDescriptors[textureDescriptorIndices[hit.triangleIdx]];
    let uv = hit.uv;
    let albedo = textureLookup(textureDesc, uv);

    return Scatter(wi, albedo);
}

fn pixarOnb(n: vec3f) -> mat3x3f {
    // https://www.jcgt.org/published/0006/01/01/paper-lowres.pdf
    let s = select(-1f, 1f, n.z >= 0f);
    let a = -1f / (s + n.z);
    let b = n.x * n.y * a;
    let u = vec3(1f + s * n.x * n.x * a, s * b, -s * n.x);
    let v = vec3(b, s + n.y * n.y * a, -n.y);

    return mat3x3(u, v, n);
}

fn rayIntersectBvh(ray: Ray, rayTMax: f32, hit: ptr<function, Intersection>) -> bool {
    let intersector = rayAabbIntersector(ray);
    var toVisitOffset = 0u;
    var currentNodeIdx = 0u;
    var nodesToVisit: array<u32, 32u>;
    var didIntersect: bool = false;
    var tmax = rayTMax;

    loop {
        let node: BvhNode = bvhNodes[currentNodeIdx];

        if rayIntersectAabb(intersector, node.aabb, tmax) {
            if node.triangleCount > 0u {
                for (var idx = 0u; idx < node.triangleCount; idx = idx + 1u) {
                    let triangle: Triangle = triangles[node.trianglesOffset + idx];
                    var trihit: TriangleHit;
                    if rayIntersectTriangle(ray, triangle, tmax, &trihit) {
                        tmax = trihit.t;

                        let b = trihit.b;

                        let p = trihit.p;

                        let triangleIdx = node.trianglesOffset + idx;
                        let ns = normals[triangleIdx];
                        let n = b[0] * ns[0] + b[1] * ns[1] + b[2] * ns[2];

                        let uvs = texCoords[triangleIdx];
                        let uv = b[0] * uvs[0] + b[1] * uvs[1] + b[2] * uvs[2];

                        *hit = Intersection(p, n, uv, triangleIdx);
                        didIntersect = true;
                    }
                }
                if toVisitOffset == 0u {
                    break;
                }
                toVisitOffset -= 1u;
                currentNodeIdx = nodesToVisit[toVisitOffset];
            } else {
                // Is intersector.invDir[node.splitAxis] < 0f? If so, visit second child first.
                if intersector.dirNeg[node.splitAxis] == 1u {
                    nodesToVisit[toVisitOffset] = currentNodeIdx + 1u;
                    currentNodeIdx = node.secondChildOffset;
                } else {
                    nodesToVisit[toVisitOffset] = node.secondChildOffset;
                    currentNodeIdx = currentNodeIdx + 1u;
                }
                toVisitOffset += 1u;
            }
        } else {
            if toVisitOffset == 0u {
                break;
            }
            toVisitOffset -= 1u;
            currentNodeIdx = nodesToVisit[toVisitOffset];
        }
    }

    return didIntersect;
}

struct RayAabbIntersector {
    origin: vec3f,
    invDir: vec3f,
    dirNeg: vec3u,
}

@must_use
fn rayAabbIntersector(ray: Ray) -> RayAabbIntersector {
    let invDirection = vec3f(1f / ray.direction.x, 1f / ray.direction.y, 1f / ray.direction.z);
    return RayAabbIntersector(
        ray.origin,
        invDirection,
        vec3u(select(0u, 1u, (invDirection.x < 0f)), select(0u, 1u, (invDirection.y < 0f)), select(0u, 1u, (invDirection.z < 0f)))
    );
}

@must_use
fn rayIntersectAabb(intersector: RayAabbIntersector, aabb: Aabb, rayTMax: f32) -> bool {
    let bounds: array<vec3f, 2> = array(aabb.min, aabb.max);

    var tmin: f32 = (bounds[intersector.dirNeg[0u]].x - intersector.origin.x) * intersector.invDir.x;
    var tmax: f32 = (bounds[1u - intersector.dirNeg[0u]].x - intersector.origin.x) * intersector.invDir.x;

    let tymin: f32 = (bounds[intersector.dirNeg[1u]].y - intersector.origin.y) * intersector.invDir.y;
    let tymax: f32 = (bounds[1 - intersector.dirNeg[1u]].y - intersector.origin.y) * intersector.invDir.y;

    if (tmin > tymax) || (tymin > tmax) {
        return false;
    }

    tmin = max(tymin, tmin);
    tmax = min(tymax, tmax);

    let tzmin: f32 = (bounds[intersector.dirNeg[2u]].z - intersector.origin.z) * intersector.invDir.z;
    let tzmax: f32 = (bounds[1 - intersector.dirNeg[2u]].z - intersector.origin.z) * intersector.invDir.z;

    if (tmin > tzmax) || (tzmin > tmax) {
        return false;
    }

    tmin = max(tzmin, tmin);
    tmax = min(tzmax, tmax);

    return (tmin < rayTMax) && (tmax > 0.0);
}

fn rayIntersectTriangle(ray: Ray, tri: Triangle, tmax: f32, hit: ptr<function, TriangleHit>) -> bool {
    // Mäller-Trumbore algorithm
    // https://en.wikipedia.org/wiki/Möller–Trumbore_intersection_algorithm
    let e1 = tri.v1 - tri.v0;
    let e2 = tri.v2 - tri.v0;

    let h = cross(ray.direction, e2);
    let det = dot(e1, h);

    if det > -EPSILON && det < EPSILON {
        return false;
    }

    let invDet = 1.0f / det;
    let s = ray.origin - tri.v0;
    let u = invDet * dot(s, h);

    if u < 0.0f || u > 1.0f {
        return false;
    }

    let q = cross(s, e1);
    let v = invDet * dot(ray.direction, q);

    if v < 0.0f || u + v > 1.0f {
        return false;
    }

    let t = invDet * dot(e2, q);

    if t > EPSILON && t < tmax {
        // https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection.html
        // e1 = v1 - v0
        // e2 = v2 - v0
        // -> p = v0 + u * e1 + v * e2
        let p = tri.v0 + u * e1 + v * e2;
        let b = vec3f(1f - u - v, u, v);
        *hit = TriangleHit(p, b, t);
        return true;
    } else {
        return false;
    }
}

struct TextureDescriptor {
    width: u32,
    height: u32,
    offset: u32,
}

@must_use
fn textureLookup(desc: TextureDescriptor, uv: vec2f) -> vec3f {
    let u = clamp(uv.x, 0f, 1f);
    let v = clamp(uv.y, 0f, 1f);

    let j = u32(u * f32(desc.width));
    let i = u32(v * f32(desc.height));
    let idx = i * desc.width + j;

    let rgba = textures[desc.offset + idx];
    return vec3f(f32(rgba & 0xffu), f32((rgba >> 8u) & 0xffu), f32((rgba >> 16u) & 0xffu)) / 255f;
}

@must_use
fn rngNextInCosineWeightedHemisphere(state: ptr<function, u32>) -> vec3f {
    let r1 = rngNextFloat(state);
    let r2 = rngNextFloat(state);
    let sqrtR2 = sqrt(r2);

    let z = sqrt(1f - r2);
    let phi = 2f * PI * r1;
    let x = cos(phi) * sqrtR2;
    let y = sin(phi) * sqrtR2;

    return vec3(x, y, z);
}

@must_use
fn initRng(pixel: vec2u, resolution: vec2u, frame: u32) -> u32 {
    // Adapted from https://github.com/boksajak/referencePT
    let seed = dot(pixel, vec2u(1u, resolution.x)) ^ jenkinsHash(frame);
    return jenkinsHash(seed);
}

@must_use
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

struct Uniforms {
    viewProjectionMatrix: mat4x4f,
}

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

struct VertexInput {
    @location(0) position: vec2f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) texCoord: vec2f,
}

@vertex
fn vsMain(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uniforms.viewProjectionMatrix * vec4f(in.position, 0.0, 1.0);
    out.texCoord = in.position + vec2f(0.5);

    return out;
}

// render params bind group
@group(1) @binding(0) var<uniform> renderParams: RenderParams;
@group(1) @binding(1) var<uniform> postProcessingParams: PostProcessingParams;
@group(1) @binding(2) var<storage, read_write> skyState: SkyState;

// scene bind group
// TODO: these are `read` only buffers. How can I create a buffer layout type which allows this?
// Annotating these as read causes validation failures.
@group(2) @binding(0) var<storage, read_write> bvhNodes: array<BvhNode>;
@group(2) @binding(1) var<storage, read_write> positionAttributes: array<Positions>;
@group(2) @binding(2) var<storage, read_write> vertexAttributes: array<VertexAttributes>;
@group(2) @binding(3) var<storage, read_write> textureDescriptors: array<TextureDescriptor>;
@group(2) @binding(4) var<storage, read_write> textures: array<u32>;

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

    let stops = f32(postProcessingParams.stops);
    let exposure = 1f / pow(2f, stops);

    let tonemapFn = postProcessingParams.tonemapFn;
    let rgb = expose(tonemapFn, exposure * estimator);

    let srgb = pow(rgb, vec3(1f / 2.2f));
    return vec4f(srgb, 1f);
}

const EPSILON = 0.00001f;

const PI = 3.1415927f;
const FRAC_1_PI = 0.31830987f;
const FRAC_PI_2 = 1.5707964f;

const T_MIN = 0.001f;
const T_MAX = 10000f;

const CHANNEL_R = 0u;
const CHANNEL_G = 1u;
const CHANNEL_B = 2u;

const DEGREES_TO_RADIANS = PI / 180f;
const TERRESTRIAL_SOLAR_RADIUS = 0.255f * DEGREES_TO_RADIANS;

const SOLAR_COS_THETA_MAX = cos(TERRESTRIAL_SOLAR_RADIUS);
const SOLAR_INV_PDF = 2f * PI * (1f - SOLAR_COS_THETA_MAX);

struct RenderParams {
  frameData: FrameData,
  camera: Camera,
  samplingState: SamplingState,
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
    up: vec3f,
    right: vec3f,
    lensRadius: f32,
}

struct SamplingState {
    numSamplesPerPixel: u32,
    numBounces: u32,
    accumulatedSampleCount: u32,
}

struct PostProcessingParams {
    stops: u32,
    tonemapFn: u32,
}

struct SkyState {
    params: array<f32, 27>,
    skyRadiances: array<f32, 3>,
    solarRadiances: array<f32, 3>,
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

struct Positions {
    p0: vec3f,
    p1: vec3f,
    p2: vec3f,
}

struct VertexAttributes {
    n0: vec3f,
    n1: vec3f,
    n2: vec3f,

    uv0: vec2f,
    uv1: vec2f,
    uv2: vec2f,

    textureDescriptorIdx: u32,
}

struct Ray {
    origin: vec3f,
    direction: vec3f
}

struct Intersection {
    p: vec3f,
    n: vec3f,
    uv: vec2f,
    textureDescriptorIdx: u32,
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

@must_use
fn rayColor(primaryRay: Ray, rngState: ptr<function, u32>) -> vec3f {
    var ray = primaryRay;
    var radiance = vec3(0f);
    var throughput = vec3(1f);

    var bounce = 1u;
    let numBounces = renderParams.samplingState.numBounces;
    loop {
        var hit: Intersection;
        if rayIntersectBvh(ray, T_MAX, &hit) {
            let albedo = evalTexture(hit.textureDescriptorIdx, hit.uv);
            let p = hit.p;

            let lightDirection = sampleSolarDiskDirection(SOLAR_COS_THETA_MAX, skyState.sunDirection, rngState);
            let lightIntensity = vec3(
                skyState.solarRadiances[CHANNEL_R],
                skyState.solarRadiances[CHANNEL_G],
                skyState.solarRadiances[CHANNEL_B]
            );
            let brdf = albedo * FRAC_1_PI;
            let reflectance = brdf * dot(hit.n, lightDirection);
            let lightVisibility = shadowRay(Ray(p, lightDirection), T_MAX);
            radiance += throughput * lightIntensity * reflectance * lightVisibility * SOLAR_INV_PDF;

            if bounce == numBounces {
                break;
            }

            let scatter = evalImplicitLambertian(hit.n, albedo, rngState);
            ray = Ray(p, scatter.wi);
            throughput *= scatter.throughput;
        } else {
            let v = ray.direction;
            let s = skyState.sunDirection;

            let theta = acos(v.y);
            let gamma = acos(clamp(dot(v, s), -1f, 1f));

            let skyRadiance = vec3f(
                skyRadiance(theta, gamma, CHANNEL_R),
                skyRadiance(theta, gamma, CHANNEL_G),
                skyRadiance(theta, gamma, CHANNEL_B)
            );

            radiance += throughput * skyRadiance;

            break;
        }

        bounce += 1u;
    }

    return radiance;
}

@must_use
fn generateCameraRay(camera: Camera, rngState: ptr<function, u32>, u: f32, v: f32) -> Ray {
    let randomPointInLens = camera.lensRadius * rngNextVec2InUnitDisk(rngState);
    let lensOffset = randomPointInLens.x * camera.right + randomPointInLens.y * camera.up;

    let origin = camera.origin + lensOffset;
    let direction = normalize(camera.lowerLeftCorner + u * camera.horizontal + v * camera.vertical - origin);

    return Ray(origin, direction);
}

@must_use
fn skyRadiance(theta: f32, gamma: f32, channel: u32) -> f32 {
    let r = skyState.skyRadiances[channel];
    let idx = 9u * channel;
    let p0 = skyState.params[idx + 0u];
    let p1 = skyState.params[idx + 1u];
    let p2 = skyState.params[idx + 2u];
    let p3 = skyState.params[idx + 3u];
    let p4 = skyState.params[idx + 4u];
    let p5 = skyState.params[idx + 5u];
    let p6 = skyState.params[idx + 6u];
    let p7 = skyState.params[idx + 7u];
    let p8 = skyState.params[idx + 8u];

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
fn expose(tonemapFn: u32, x: vec3f) -> vec3f {
    switch tonemapFn {
        case 1u: {
            return acesFilmic(x);
        }

        default: {
            return x;
        }
    }
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

@must_use
fn sampleSolarDiskDirection(cosThetaMax: f32, direction: vec3f, state: ptr<function, u32>) -> vec3f {
    let v = rngNextInCone(state, cosThetaMax);
    let onb = pixarOnb(direction);
    return onb * v;
}

@must_use
fn evalImplicitLambertian(n: vec3f, albedo: vec3f, rngState: ptr<function, u32>) -> Scatter {
    let v = rngNextInCosineWeightedHemisphere(rngState);
    let onb = pixarOnb(n);
    let wi = onb * v;

    return Scatter(wi, albedo);
}

@must_use
fn evalTexture(textureDescriptorIdx: u32, uv: vec2f) -> vec3f {
    let textureDesc = textureDescriptors[textureDescriptorIdx];
    return textureLookup(textureDesc, uv);
}

@must_use
fn pixarOnb(n: vec3f) -> mat3x3f {
    // https://www.jcgt.org/published/0006/01/01/paper-lowres.pdf
    let s = select(-1f, 1f, n.z >= 0f);
    let a = -1f / (s + n.z);
    let b = n.x * n.y * a;
    let u = vec3(1f + s * n.x * n.x * a, s * b, -s * n.x);
    let v = vec3(b, s + n.y * n.y * a, -n.y);

    return mat3x3(u, v, n);
}

// Returns 1.0 if no forward intersections, 0.0 otherwise.
@must_use
fn shadowRay(ray: Ray, rayTMax: f32) -> f32 {
    let intersector = rayAabbIntersector(ray);
    var toVisitOffset = 0u;
    var currentNodeIdx = 0u;
    var nodesToVisit: array<u32, 32u>;

    loop {
        let node: BvhNode = bvhNodes[currentNodeIdx];

        if rayIntersectAabb(intersector, node.aabb, rayTMax) {
            if node.triangleCount > 0u {
                for (var idx = 0u; idx < node.triangleCount; idx = idx + 1u) {
                    let triangle: Positions = positionAttributes[node.trianglesOffset + idx];
                    // TODO: trihit not actually used. A different code path could be used?
                    var trihit: TriangleHit;
                    if rayIntersectTriangle(ray, triangle, rayTMax, &trihit) {
                        return 0f;
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

    return 1f;
}

@must_use
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
                    let triangle: Positions = positionAttributes[node.trianglesOffset + idx];
                    var trihit: TriangleHit;
                    if rayIntersectTriangle(ray, triangle, tmax, &trihit) {
                        tmax = trihit.t;
                        didIntersect = true;

                        let b = trihit.b;
                        let triangleIdx = node.trianglesOffset + idx;
                        let vert = vertexAttributes[triangleIdx];

                        let p = trihit.p;
                        let n = b[0] * vert.n0 + b[1] * vert.n1 + b[2] * vert.n2;
                        let uv = b[0] * vert.uv0 + b[1] * vert.uv1 + b[2] * vert.uv2;
                        let textureDescriptorIdx = vertexAttributes[triangleIdx].textureDescriptorIdx;

                        *hit = Intersection(p, n, uv, textureDescriptorIdx);
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

@must_use
fn rayIntersectTriangle(ray: Ray, tri: Positions, tmax: f32, hit: ptr<function, TriangleHit>) -> bool {
    // Mäller-Trumbore algorithm
    // https://en.wikipedia.org/wiki/Möller–Trumbore_intersection_algorithm
    let e1 = tri.p1 - tri.p0;
    let e2 = tri.p2 - tri.p0;

    let h = cross(ray.direction, e2);
    let det = dot(e1, h);

    if det > -EPSILON && det < EPSILON {
        return false;
    }

    let invDet = 1.0f / det;
    let s = ray.origin - tri.p0;
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
        let p = tri.p0 + u * e1 + v * e2;
        let n = normalize(cross(e1, e2));
        let b = vec3f(1f - u - v, u, v);
        *hit = TriangleHit(offsetRay(p, n), b, t);
        return true;
    } else {
        return false;
    }
}

const ORIGIN = 1f / 32f;
const FLOAT_SCALE = 1f / 65536f;
const INT_SCALE = 256f;

@must_use
fn offsetRay(p: vec3f, n: vec3f) -> vec3f {
    // Source: A Fast and Robust Method for Avoiding Self-Intersection, Ray Tracing Gems
    let offset = vec3i(i32(INT_SCALE * n.x), i32(INT_SCALE * n.y), i32(INT_SCALE * n.z));
    // Offset added straight into the mantissa bits to ensure the offset is scale-invariant,
    // except for when close to the origin, where we use FLOAT_SCALE as a small epsilon.
    let po = vec3f(
        bitcast<f32>(bitcast<i32>(p.x) + select(offset.x, -offset.x, (p.x < 0))),
        bitcast<f32>(bitcast<i32>(p.y) + select(offset.y, -offset.y, (p.y < 0))),
        bitcast<f32>(bitcast<i32>(p.z) + select(offset.z, -offset.z, (p.z < 0)))
    );

    return vec3f(
        select(po.x, p.x + FLOAT_SCALE * n.x, (abs(p.x) < ORIGIN)),
        select(po.y, p.y + FLOAT_SCALE * n.y, (abs(p.y) < ORIGIN)),
        select(po.z, p.z + FLOAT_SCALE * n.z, (abs(p.z) < ORIGIN))
    );
}

struct TextureDescriptor {
    width: u32,
    height: u32,
    offset: u32,
}

@must_use
fn textureLookup(desc: TextureDescriptor, uv: vec2f) -> vec3f {
    let u = fract(uv.x);
    let v = fract(uv.y);

    let j = u32(u * f32(desc.width));
    let i = u32(v * f32(desc.height));
    let idx = i * desc.width + j;

    let pixel = textures[desc.offset + idx];
    let srgb = vec3(f32(pixel & 0xffu), f32((pixel >> 8u) & 0xffu), f32((pixel >> 16u) & 0xffu)) / 255f;
    let linearRgb = pow(srgb, vec3(2.2f));
    return linearRgb;
}

@must_use
fn rngNextInCone(state: ptr<function, u32>, cosThetaMax: f32) -> vec3f {
    let u1 = rngNextFloat(state);
    let u2 = rngNextFloat(state);

    let cosTheta = 1f - u1 * (1f - cosThetaMax);
    let sinTheta = sqrt(1f - cosTheta * cosTheta);
    let phi = 2f * PI * u2;

    let x = cos(phi) * sinTheta;
    let y = sin(phi) * sinTheta;
    let z = cosTheta;

    return vec3(x, y, z);
}

@must_use
fn rngNextInCosineWeightedHemisphere(state: ptr<function, u32>) -> vec3f {
    let u1 = rngNextFloat(state);
    let u2 = rngNextFloat(state);

    let phi = 2f * PI * u2;
    let sinTheta = sqrt(1f - u1);

    let x = cos(phi) * sinTheta;
    let y = sin(phi) * sinTheta;
    let z = sqrt(u1);

    return vec3(x, y, z);
}

@must_use
fn rngNextVec2InUnitDisk(state: ptr<function, u32>) -> vec2f {
    // Generate numbers uniformly in a disk:
    // https://stats.stackexchange.com/a/481559

    // r^2 is distributed as U(0, 1).
    let r = sqrt(rngNextFloat(state));
    let alpha = 2f * PI * rngNextFloat(state);

    let x = r * cos(alpha);
    let y = r * sin(alpha);

    return vec2(x, y);
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

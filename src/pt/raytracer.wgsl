struct Uniforms {
    viewProjectionMatrix: mat4x4<f32>,
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

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    let u = in.texCoord.x;
    let v = in.texCoord.y;

    let dimensions = renderParams.frameData.dimensions;
    let frameCount = renderParams.frameData.frameCount;

    let j = u32(u * f32(dimensions.x));
    let i = u32(v * f32(dimensions.y));

    var rngState = initRng(vec2(j, i), dimensions, frameCount);

    let primaryRay = generateCameraRay(renderParams.camera, &rngState, u, v);
    let rgb = rayColor(primaryRay, &rngState);

    return vec4f(rgb, 1f);
}

const EPSILON = 0.00001f;

const PI = 3.1415927f;

const T_MIN = 0.001f;
const T_MAX = 10000f;

struct RenderParams {
  frameData: FrameData,
  camera: Camera,
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
    b: vec3f,
    t: f32,
}

fn rayColor(primaryRay: Ray, rngState: ptr<function, u32>) -> vec3f {
    var ray = primaryRay;

    let unitDirection = normalize(ray.direction);
    let t = 0.5f * (unitDirection.y + 1f);
    var color = (1f - t) * vec3(1f, 1f, 1f) + t * vec3(0.5f, 0.7f, 1f);

    var intersection = Intersection();
    var triangleIdx = 0u;
    if rayIntersectBvh(ray, T_MAX, &intersection, &triangleIdx) {
        let b = intersection.b;

        let uvs = texCoords[triangleIdx];
        let uv = b[0] * uvs[0] + b[1] * uvs[1] + b[2] * uvs[2];
        let textureDesc = textureDescriptors[textureDescriptorIndices[triangleIdx]];
        color = textureLookup(textureDesc, uv);
    }

    return color;
}

fn generateCameraRay(camera: Camera, rngState: ptr<function, u32>, u: f32, v: f32) -> Ray {
    let origin = camera.origin;
    let direction = camera.lowerLeftCorner + u * camera.horizontal + v * camera.vertical - origin;

    return Ray(origin, direction);
}

fn rayIntersectBvh(ray: Ray, rayTMax: f32, hit: ptr<function, Intersection>, triangleIdx: ptr<function, u32>) -> bool {
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
                    if rayIntersectTriangle(ray, triangle, tmax, hit) {
                        tmax = (*hit).t;
                        *triangleIdx = node.trianglesOffset + idx;
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

fn rayIntersectTriangle(ray: Ray, tri: Triangle, tmax: f32, hit: ptr<function, Intersection>) -> bool {
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
        *hit = Intersection(p, b, t);
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

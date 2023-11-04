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

@group(1) @binding(0) var<uniform> frameData: FrameData;
@group(1) @binding(1) var<uniform> camera: Camera;

@fragment
fn fsMain(in: VertexOutput) -> @location(0) vec4f {
    let u = in.texCoord.x;
    let v = in.texCoord.y;

    let j =  u32(u * f32(frameData.dimensions.x));
    let i =  u32(v * f32(frameData.dimensions.y));

    var rngState = initRng(vec2(j, i), frameData.dimensions, frameData.frameCount);

    let primaryRay = generateCameraRay(camera, &rngState, u, v);
    let rgb = rayColor(primaryRay, &rngState);

    return vec4f(rgb, 1f);
}

const PI = 3.1415927f;

const T_MIN = 0.001f;
const T_MAX = 1000f;

struct FrameData {
    dimensions: vec2u,
    frameCount: u32,
}

struct Camera {
    eye: vec3f,
    lowerLeftCorner: vec3f,
    horizontal: vec3f,
    vertical: vec3f,
    lensRadius: f32,
}

struct Sphere {
    center: vec3f,
    radius: f32,
}

struct Ray {
    origin: vec3f,
    direction: vec3f
}

struct Intersection {
    p: vec3f,
    n: vec3f,
    t: f32,
}

fn rayColor(primaryRay: Ray, rngState: ptr<function, u32>) -> vec3f {
    var ray = primaryRay;
    var color = vec3f(0f, 0f, 0f);
    var closestIntersect = Intersection();

    let spheres = array<Sphere, 4>(
        Sphere(vec3f(0.0, -500.0, -1.0), 500.0),
        Sphere(vec3f(0.0, 1.0, 0.0), 1.0),
        Sphere(vec3f(-5.0, 1.0, 0.0), 1.0),
        Sphere(vec3f(5.0, 1.0, 0.0), 1.0)
    );

    var tClosest = T_MAX;
    var testIntersect = Intersection();
    var hit = false;
    for (var idx = 0u; idx < 4u; idx = idx + 1u) {
        let sphere = spheres[idx];
        if rayIntersectSphere(ray, sphere, T_MIN, T_MAX, &testIntersect) {
            if testIntersect.t < tClosest {
                tClosest = testIntersect.t;
                closestIntersect = testIntersect;
                hit = true;
            }
        }
    }

    if hit {
        color = 0.5f * (vec3f(1f, 1f, 1f) + closestIntersect.n);
    } else {
        let unitDirection = normalize(ray.direction);
        let t = 0.5f * (unitDirection.y + 1f);
        color = (1f - t) * vec3(1f, 1f, 1f) + t * vec3(0.5f, 0.7f, 1f);
    }

    return color;
}

fn generateCameraRay(camera: Camera, rngState: ptr<function, u32>, u: f32, v: f32) -> Ray {
    let origin = camera.eye;
    let direction = camera.lowerLeftCorner + u * camera.horizontal + v * camera.vertical - origin;

    return Ray(origin, direction);
}

fn rayIntersectSphere(ray: Ray, sphere: Sphere, tmin: f32, tmax: f32, hit: ptr<function, Intersection>) -> bool {
    let oc = ray.origin - sphere.center;
    let a = dot(ray.direction, ray.direction);
    let b = dot(oc, ray.direction);
    let c = dot(oc, oc) - sphere.radius * sphere.radius;
    let discriminant = b * b - a * c;

    if discriminant > 0f {
        var t = (-b - sqrt(b * b - a * c)) / a;
        if t < tmax && t > tmin {
            *hit = sphereIntersection(ray, sphere, t);
            return true;
        }

        t = (-b + sqrt(b * b - a * c)) / a;
        if t < tmax && t > tmin {
            *hit = sphereIntersection(ray, sphere, t);
            return true;
        }
    }

    return false;
}

fn sphereIntersection(ray: Ray, sphere: Sphere, t: f32) -> Intersection {
    let p = rayPointAtParameter(ray, t);
    let n = (1f / sphere.radius) * (p - sphere.center);
    let theta = acos(-n.y);
    let phi = atan2(-n.z, n.x) + PI;

    return Intersection(p, n, t);
}

fn rayPointAtParameter(ray: Ray, t: f32) -> vec3f {
    return ray.origin + t * ray.direction;
}

fn initRng(pixel: vec2u, resolution: vec2u, frame: u32) -> u32 {
    // Adapted from https://github.com/boksajak/referencePT
    let seed = dot(pixel, vec2u(1u, resolution.x)) ^ jenkinsHash(frame);
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

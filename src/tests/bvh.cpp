#include <common/bvh.hpp>
#include <common/camera.hpp>
#include <common/gltf_model.hpp>
#include <common/ray_intersection.hpp>
#include <common/units/angle.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace nlrs;

bool bruteForceRayIntersectModel(
    const Ray&                      ray,
    const std::span<const Triangle> triangles,
    float                           rayTMax,
    Intersection&                   intersect)
{
    bool didIntersect = false;
    for (const Triangle& tri : triangles)
    {
        if (rayIntersectTriangle(ray, tri, rayTMax, intersect))
        {
            rayTMax = intersect.t;
            didIntersect = true;
        }
    }
    return didIntersect;
}

TEST_CASE("Bvh intersection matches brute-force intersection", "[bvh]")
{
    static_assert(
        sizeof(Positions) == sizeof(Triangle), "Positions and Triangle must have the same layout");
    static_assert(
        offsetof(Positions, v0) == offsetof(Triangle, v0),
        "Positions and Triangle must have the same layout");
    static_assert(
        offsetof(Positions, v1) == offsetof(Triangle, v1),
        "Positions and Triangle must have the same layout");
    static_assert(
        offsetof(Positions, v2) == offsetof(Triangle, v2),
        "Positions and Triangle must have the same layout");

    GltfModel model("Duck.glb");
    REQUIRE_FALSE(model.positions().empty());

    const Bvh bvh = [&model]() -> Bvh {
        const std::span<const Triangle> triangles = std::span<const Triangle>(
            reinterpret_cast<const Triangle*>(model.positions().data()), model.positions().size());
        return buildBvh(triangles);
    }();
    REQUIRE_FALSE(bvh.nodes.empty());
    REQUIRE_FALSE(bvh.triangles.empty());

    const Camera camera = [&bvh]() -> Camera {
        const Aabb modelAabb = [&bvh]() -> Aabb {
            Aabb aabb;
            for (const Triangle& tri : bvh.triangles)
            {
                aabb = merge(aabb, tri.v0);
                aabb = merge(aabb, tri.v1);
                aabb = merge(aabb, tri.v2);
            }
            return aabb;
        }();

        const glm::vec3 rootDiagonal = diagonal(modelAabb);
        const glm::vec3 rootCentroid = centroid(modelAabb);
        const int       maxDim = maxDimension(modelAabb);

        const float aperture = 0.0f;
        const float focusDistance = 1.0f;
        const Angle vfov = Angle::degrees(70.0f);

        return createCamera(
            rootCentroid -
                glm::vec3(-0.8f * rootDiagonal[maxDim], 0.0f, 0.8f * rootDiagonal[maxDim]),
            rootCentroid,
            aperture,
            focusDistance,
            vfov,
            1.0f);
    }();

    const float rayTMax = 1000.0f;
    const int   numRaysX = 64;
    const int   numRaysY = 64;

    for (int i = 0; i < numRaysX; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(numRaysX);
        for (int j = 0; j < numRaysY; ++j)
        {
            const float v = static_cast<float>(j) / static_cast<float>(numRaysY);
            const Ray   ray = generateCameraRay(camera, u, v);

            Intersection bruteForceIntersection;
            const bool   didIntersect =
                bruteForceRayIntersectModel(ray, bvh.triangles, rayTMax, bruteForceIntersection);
            Intersection bvhIntersection;
            const bool   bvhDidIntersect = rayIntersectBvh(ray, bvh, rayTMax, bvhIntersection);

            REQUIRE(bvhDidIntersect == didIntersect);

            if (didIntersect)
            {
                REQUIRE(bruteForceIntersection.t == Catch::Approx(bvhIntersection.t));
            }
        }
    }
}

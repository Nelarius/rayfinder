#include <common/bvh.hpp>
#include <common/camera.hpp>
#include <common/gltf_model.hpp>
#include <common/ray_intersection.hpp>
#include <common/units/angle.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace pt;

bool bruteForceRayIntersectModel(
    const Ray&                      ray,
    const std::span<const Triangle> triangles,
    float                           rayTMax,
    Intersection&                   intersect)
{
    bool didIntersect = false;
    for (const Triangle& tri : triangles)
    {
        if (rayIntersectTriangle(ray, Triangle48(tri), rayTMax, intersect))
        {
            rayTMax = intersect.t;
            didIntersect = true;
        }
    }
    return didIntersect;
}

TEST_CASE("Bvh intersection matches brute-force intersection", "[bvh]")
{
    GltfModel model("Duck.glb");
    REQUIRE_FALSE(model.triangles().empty());
    const std::span<const Triangle> triangles = model.triangles();

    const Bvh bvh = buildBvh(model.triangles());
    REQUIRE_FALSE(bvh.nodes.empty());
    REQUIRE_FALSE(bvh.triangles.empty());

    const Camera camera = [&triangles]() -> Camera {
        const Aabb modelAabb = [&triangles]() -> Aabb {
            Aabb aabb;
            for (const Triangle& tri : triangles)
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
        const int   viewportWidth = 1;
        const int   viewportHeight = 1;

        return createCamera(
            rootCentroid -
                glm::vec3(-0.8f * rootDiagonal[maxDim], 0.0f, 0.8f * rootDiagonal[maxDim]),
            rootCentroid,
            aperture,
            focusDistance,
            vfov,
            viewportWidth,
            viewportHeight);
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
                bruteForceRayIntersectModel(ray, triangles, rayTMax, bruteForceIntersection);
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

#include <common/aabb.hpp>
#include <common/ray.hpp>
#include <common/ray_intersection.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

TEST_CASE("Aabb with default constructor", "[aabb]")
{
    const nlrs::Aabb aabb;

    SECTION("Merging with a point yields just the point")
    {
        const nlrs::Aabb merged = nlrs::merge(aabb, glm::vec3(0.0f));

        REQUIRE(merged.min == glm::vec3{0.0f});
        REQUIRE(merged.max == glm::vec3{0.0f});
    }

    SECTION("Merging with another Aabb yields just the bounding box")
    {
        const nlrs::Aabb other = nlrs::Aabb(glm::vec3(-1.0f), glm::vec3(1.0f));
        const nlrs::Aabb merged = nlrs::merge(aabb, other);

        REQUIRE(merged.min == glm::vec3(-1.0f));
        REQUIRE(merged.max == glm::vec3(1.0f));
    }
}

TEST_CASE("Aabb max dimension", "[aabb]")
{
    SECTION("The z-axis is returned when all axes have the same extents")
    {
        const nlrs::Aabb aabb(glm::vec3{-1.0f}, glm::vec3{1.0f});
        const int        maxDimension = nlrs::maxDimension(aabb);

        REQUIRE(maxDimension == 2);
    }

    SECTION("Different extents")
    {
        const nlrs::Aabb aabb(glm::vec3{-3.0f, -2.0f, -1.0f}, glm::vec3{1.0f, 1.0f, 1.0f});
        const int        maxDimension = nlrs::maxDimension(aabb);

        REQUIRE(maxDimension == 0);
    }
}

TEST_CASE("Aabb surface area", "[aabb]")
{
    const nlrs::Aabb aabb(glm::vec3{-1.0f}, glm::vec3{1.0f});
    const float      surfaceArea = nlrs::surfaceArea(aabb);

    REQUIRE(surfaceArea == Catch::Approx(24.0f));
}

// TODO: do these tests really belong among AABB tests? Aabb32 is an implementation detail of BVH,
// and is defined in bvh.hpp. Perhaps bvh intersection testing definitions could be moved to
// bvh.hpp, and the tests to bvh.cpp.

TEST_CASE("Ray-Aabb intersection test", "[bvh]")
{
    SECTION("Ray intersects x slab")
    {
        const nlrs::Ray ray{
            .origin = glm::vec3{-2.0f, 0.0f, 0.0f},
            .direction = glm::vec3{1.0f, 0.0f, 0.0f},
        };
        const nlrs::Aabb aabb(glm::vec3{-1.0f, -1.0f, -1.0f}, glm::vec3{1.0f, 1.0f, 1.0f});

        const nlrs::RayAabbIntersector intersector(ray);
        const bool intersects = nlrs::rayIntersectAabb(intersector, aabb, 100.0f);

        REQUIRE(intersects);
    }

    SECTION("Ray intersects y slab")
    {
        const nlrs::Ray ray{
            .origin = glm::vec3{0.0f, -1.0f, 0.0f},
            .direction = glm::vec3{0.0f, 1.0f, 0.0f},
        };
        const nlrs::Aabb aabb(glm::vec3{-1.0f, 0.0f, -1.0f}, glm::vec3{1.0f, 1.0f, 1.0f});

        const nlrs::RayAabbIntersector intersector(ray);
        const bool intersects = nlrs::rayIntersectAabb(intersector, aabb, 100.0f);

        REQUIRE(intersects);
    }

    SECTION("Ray intersects z slab")
    {
        const nlrs::Ray ray{
            .origin = glm::vec3{0.0f, 0.0f, -1.0f},
            .direction = glm::vec3{0.0f, 0.0f, 1.0f},
        };
        const nlrs::Aabb aabb(glm::vec3{-1.0f, -1.0f, 0.0f}, glm::vec3{1.0f, 1.0f, 1.0f});

        const nlrs::RayAabbIntersector intersector(ray);
        const bool intersects = nlrs::rayIntersectAabb(intersector, aabb, 100.0f);

        REQUIRE(intersects);
    }

    SECTION("Ray intersects corner")
    {
        const nlrs::Ray ray{
            .origin = glm::vec3{-1.0f, -1.0f, -1.0f},
            .direction = glm::vec3{1.0f, 1.0f, 1.0f},
        };
        const nlrs::Aabb aabb(glm::vec3{-1.0f, -1.0f, -1.0f}, glm::vec3{1.0f, 1.0f, 1.0f});

        const nlrs::RayAabbIntersector intersector(ray);
        const bool intersects = nlrs::rayIntersectAabb(intersector, aabb, 100.0f);

        REQUIRE(intersects);
    }

    SECTION("Ray misses aabb")
    {
        const nlrs::Ray ray{
            .origin = glm::vec3{-2.0f, 0.0f, -1.0f},
            .direction = glm::vec3{0.0f, 1.0f, 0.0f},
        };
        const nlrs::Aabb aabb(glm::vec3{-1.0f, -1.0f, -1.0f}, glm::vec3{1.0f, 1.0f, 1.0f});

        const nlrs::RayAabbIntersector intersector(ray);
        const bool intersects = nlrs::rayIntersectAabb(intersector, aabb, 100.0f);

        REQUIRE_FALSE(intersects);
    }
}

#include <common/bvh.hpp>
#include <common/ray.hpp>
#include <common/ray_intersection.hpp>
#include <common/triangle_attributes.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

TEST_CASE("Ray intersects triangle", "[intersection]")
{
    const nlrs::Ray ray{
        .origin = glm::vec3{0.0f, 0.0f, 0.0f},
        .direction = glm::vec3{0.0f, 0.0f, 1.0f},
    };
    const nlrs::Positions triangle{
        .v0 = glm::vec3{0.0f, 0.0f, 1.0f},
        .v1 = glm::vec3{1.0f, 0.0f, 1.0f},
        .v2 = glm::vec3{0.0f, 1.0f, 1.0f},
    };

    nlrs::Intersection isect;
    const bool         intersects = rayIntersectTriangle(ray, triangle, 1000.0f, isect);

    REQUIRE(intersects);
    REQUIRE(isect.p.x == Catch::Approx(0.0f));
    REQUIRE(isect.p.y == Catch::Approx(0.0f));
    REQUIRE(isect.p.z == Catch::Approx(1.0f));
}

#include <common/bvh.hpp>
#include <common/ray.hpp>
#include <common/ray_intersection.hpp>
#include <common/triangle_attributes.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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
    REQUIRE_THAT(isect.p.x, Catch::Matchers::WithinRel(0.0f, 0.001f));
    REQUIRE_THAT(isect.p.y, Catch::Matchers::WithinRel(0.0f, 0.001f));
    REQUIRE_THAT(isect.p.z, Catch::Matchers::WithinRel(1.0f, 0.001f));
}

#include <common/math.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Fract", "[math]")
{
    REQUIRE(nlrs::fract(0.0f) == 0.0f);
    REQUIRE(nlrs::fract(1.0f) == 0.0f);
    REQUIRE(nlrs::fract(1.5f) == 0.5f);
    REQUIRE(nlrs::fract(-1.5f) == -0.5f);
    REQUIRE(nlrs::fract(-1.0f) == -0.0f);
    REQUIRE(nlrs::fract(-0.5f) == -0.5f);
    REQUIRE(nlrs::fract(-0.0f) == -0.0f);
}

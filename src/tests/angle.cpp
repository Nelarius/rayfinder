#include <common/units/angle.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <numbers>

TEST_CASE("Angle to radians", "[angle]")
{
    pt::Angle angle = pt::Angle::degrees(90.0f);
    REQUIRE(angle.as_radians() == Catch::Approx(0.5f * std::numbers::pi_v<float>));
    REQUIRE(angle.as_degrees() == Catch::Approx(90.0f));
}

TEST_CASE("Angle to degrees", "[angle]")
{
    pt::Angle angle = pt::Angle::radians(0.5f * std::numbers::pi_v<float>);
    REQUIRE(angle.as_degrees() == Catch::Approx(90.0f));
    REQUIRE(angle.as_radians() == Catch::Approx(0.5f * std::numbers::pi_v<float>));
}

TEST_CASE("Angle add", "[angle]")
{
    pt::Angle lhs = pt::Angle::degrees(90.0f);
    pt::Angle rhs = pt::Angle::degrees(90.0f);
    pt::Angle result = lhs + rhs;
    REQUIRE(result.as_degrees() == Catch::Approx(180.0f));
    REQUIRE(result.as_radians() == Catch::Approx(std::numbers::pi_v<float>));
}

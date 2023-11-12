#include <common/units/angle.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <numbers>

TEST_CASE("Angle to radians", "[angle]")
{
    nlrs::Angle angle = nlrs::Angle::degrees(90.0f);
    REQUIRE(angle.asRadians() == Catch::Approx(0.5f * std::numbers::pi_v<float>));
    REQUIRE(angle.asDegrees() == Catch::Approx(90.0f));
}

TEST_CASE("Angle to degrees", "[angle]")
{
    nlrs::Angle angle = nlrs::Angle::radians(0.5f * std::numbers::pi_v<float>);
    REQUIRE(angle.asDegrees() == Catch::Approx(90.0f));
    REQUIRE(angle.asRadians() == Catch::Approx(0.5f * std::numbers::pi_v<float>));
}

TEST_CASE("Angle add", "[angle]")
{
    nlrs::Angle lhs = nlrs::Angle::degrees(90.0f);
    nlrs::Angle rhs = nlrs::Angle::degrees(90.0f);
    nlrs::Angle result = lhs + rhs;
    REQUIRE(result.asDegrees() == Catch::Approx(180.0f));
    REQUIRE(result.asRadians() == Catch::Approx(std::numbers::pi_v<float>));
}

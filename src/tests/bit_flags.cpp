#include <common/bit_flags.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace nlrs;

enum class TestFlag : uint32_t
{
    A = 1 << 0,
    B = 1 << 1,
    C = 1 << 2,
};
using TestFlags = BitFlags<TestFlag>;

TEST_CASE("Bitwise-and", "[bit_flags]")
{
    const TestFlags flags{TestFlag::A, TestFlag::B};
    REQUIRE(flags.has(TestFlag::A));
    REQUIRE(flags.has(TestFlag::B));
    REQUIRE_FALSE(flags.has(TestFlag::C));
}

TEST_CASE("BitFlags::none", "[bit_flags]")
{
    const TestFlags flags = TestFlags::none();
    REQUIRE_FALSE(flags.has(TestFlag::A));
    REQUIRE_FALSE(flags.has(TestFlag::B));
    REQUIRE_FALSE(flags.has(TestFlag::C));
}

TEST_CASE("BitFlags::all", "[bit_flags]")
{
    const TestFlags flags = TestFlags::all();
    REQUIRE(flags.has(TestFlag::A));
    REQUIRE(flags.has(TestFlag::B));
    REQUIRE(flags.has(TestFlag::C));
}

TEST_CASE("BitFlags size", "[bit_flags]") { REQUIRE(sizeof(TestFlags) == sizeof(uint32_t)); }

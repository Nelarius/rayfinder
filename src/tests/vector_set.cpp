#include <common/vector_set.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstring>

SCENARIO("Insert into VectorSet", "[vector_set]")
{
    GIVEN("an empty VectorSet")
    {
        pt::VectorSet<int> s;

        THEN("the size is zero and s is empty")
        {
            REQUIRE(s.empty());
            REQUIRE(s.size() == 0);
        }

        WHEN("inserting a value")
        {
            const auto r = s.insert(1);

            THEN("the returned iterator points to the inserted value") { REQUIRE(*r.first == 1); }

            THEN("the return value indicates success") { REQUIRE(r.second); }

            THEN("the VectorSet is no longer empty")
            {
                REQUIRE_FALSE(s.empty());
                REQUIRE(s.size() == 1);
            }

            THEN("the value is contained") { REQUIRE(s.contains(1)); }

            THEN("the iterator returned by find does not equal end()")
            {
                const auto it = s.find(1);
                REQUIRE(it != s.end());

                AND_THEN("the dereferenced iterator is the correct value") { REQUIRE(*it == 1); }
            }

            THEN("inserting the same value again does not insert anything")
            {
                const auto r2 = s.insert(1);

                REQUIRE(*r2.first == 1);
                REQUIRE_FALSE(r2.second);
                REQUIRE(s.size() == 1);
            }
        }
    }
}

SCENARIO("Inserting an element into non-empty VectorSet", "[vector_set]")
{
    GIVEN("a VectorSet with elements")
    {
        pt::VectorSet<int> s({1, 5, 10});

        WHEN("inserting in the middle of the range")
        {
            s.insert(3);

            THEN("order is retained")
            {
                const std::array<int, 4> ints{1, 3, 5, 10};
                REQUIRE(std::memcmp(s.data(), ints.data(), sizeof(ints)) == 0);
            }

            THEN("all elements are contained")
            {
                REQUIRE(s.contains(1));
                REQUIRE(s.contains(3));
                REQUIRE(s.contains(5));
                REQUIRE(s.contains(10));
            }
        }

        WHEN("inserting before the range")
        {
            s.insert(0);

            THEN("first element is the inserted value") { REQUIRE(s[0] == 0); }
        }

        WHEN("inserting after the range")
        {
            s.insert(16);

            THEN("the last element is the inserted value") { REQUIRE(s[3] == 16); }
        }
    }
}

SCENARIO("Erasing an element from VectorSet", "[vector_set]")
{
    GIVEN("a VectorSet with some values")
    {
        pt::VectorSet<int> s({1, 2, 3});

        WHEN("erasing an element by the key value")
        {
            const auto it = s.erase(1);

            THEN("the returned iterator points to the following element") { REQUIRE(*it == 2); }

            THEN("the value is no longer contained") { REQUIRE_FALSE(s.contains(1)); }

            THEN("the value can no longer be found") { REQUIRE(s.find(1) == s.end()); }
        }

        WHEN("erasing a non-existent element")
        {
            const auto it = s.erase(0);

            THEN("the returned iterator equals end()") { REQUIRE(it == s.end()); }

            THEN("the size and number of elements is unchanged")
            {
                REQUIRE_FALSE(s.empty());
                REQUIRE(s.size() == 3);
            }
        }
    }
}

TEST_CASE("Instantiating with pointers works", "[vector_set]")
{
    const std::array<int, 2>  ints{1, 2};
    pt::VectorSet<const int*> s({&ints[0], &ints[1]});

    REQUIRE_FALSE(s.empty());
    REQUIRE(s.size() == 2);

    SECTION("contains and find works")
    {
        REQUIRE(s.contains(ints.data()));
        REQUIRE(s.contains(ints.data() + 1));
        REQUIRE_FALSE(s.contains(ints.data() + 2));

        const auto it1 = s.find(ints.data());
        const auto it2 = s.find(ints.data() + 1);
        REQUIRE(it1 != s.end());
        REQUIRE(it2 != s.end());

        REQUIRE(**it1 == 1);
        REQUIRE(**it2 == 2);
    }
}

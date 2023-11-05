#include <common/bvh.hpp>
#include <common/gltf_model.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <fstream>
#include <sstream>

namespace pt
{
using ByteStream = std::basic_stringstream<char>;
using OutputByteStream = std::basic_ostream<char>;
using InputByteStream = std::basic_istream<char>;

void serialize(OutputByteStream& ostream, const Bvh& bvh)
{
    {
        const std::size_t nodeCount = bvh.nodes.size();
        ostream.write(reinterpret_cast<const char*>(&nodeCount), sizeof(std::size_t));
        ostream.write(reinterpret_cast<const char*>(bvh.nodes.data()), sizeof(BvhNode) * nodeCount);
    }
    {
        const std::size_t triangleCount = bvh.triangles.size();
        ostream.write(reinterpret_cast<const char*>(&triangleCount), sizeof(std::size_t));
        ostream.write(
            reinterpret_cast<const char*>(bvh.triangles.data()), sizeof(Triangle) * triangleCount);
    }
}

void deserialize(InputByteStream& istream, Bvh& bvh)
{
    {
        std::size_t nodeCount;
        istream.read(reinterpret_cast<char*>(&nodeCount), sizeof(std::size_t));

        bvh.nodes.resize(nodeCount);
        istream.read(reinterpret_cast<char*>(bvh.nodes.data()), sizeof(BvhNode) * nodeCount);
    }
    {
        std::size_t triangleCount;
        istream.read(reinterpret_cast<char*>(&triangleCount), sizeof(std::size_t));

        bvh.triangles.resize(triangleCount);
        istream.read(
            reinterpret_cast<char*>(bvh.triangles.data()), sizeof(Triangle) * triangleCount);
    }
}
} // namespace pt

SCENARIO("Serialize and deserializing a bvh", "[archive]")
{
    GIVEN("a BVH")
    {
        pt::GltfModel model("Duck.glb");
        REQUIRE_FALSE(model.triangles().empty());

        const pt::Bvh bvh = buildBvh(model.triangles());
        REQUIRE_FALSE(bvh.nodes.empty());
        REQUIRE_FALSE(bvh.triangles.empty());

        WHEN("serializing to a bytestream")
        {
            pt::ByteStream byteStream;
            pt::serialize(byteStream, bvh);
            REQUIRE_FALSE(byteStream.view().empty());

            THEN("bvh2 deserialized from bytestream equals original bvh")
            {
                pt::Bvh bvh2;
                pt::deserialize(byteStream, bvh2);

                REQUIRE(bvh2.nodes.size() == bvh.nodes.size());
                REQUIRE(bvh2.triangles.size() == bvh.triangles.size());

                const std::size_t byteSizeNodes = sizeof(pt::BvhNode) * bvh2.nodes.size();
                const std::size_t byteSizeTriangles = sizeof(pt::Triangle) * bvh2.triangles.size();
                REQUIRE(std::memcmp(bvh2.nodes.data(), bvh.nodes.data(), byteSizeNodes) == 0);
                REQUIRE(
                    std::memcmp(bvh2.triangles.data(), bvh.triangles.data(), byteSizeTriangles) ==
                    0);
            }
        }

        WHEN("serializing to an ofstream")
        {
            {
                std::ofstream file("bvh.testbin", std::ios::binary);
                pt::serialize(file, bvh);
            }

            THEN("bvh2 deserialized from ifstream equals original bvh")
            {
                pt::Bvh       bvh2;
                std::ifstream file("bvh.testbin", std::ios::binary);
                pt::deserialize(file, bvh2);

                REQUIRE(bvh2.nodes.size() == bvh.nodes.size());
                REQUIRE(bvh2.triangles.size() == bvh.triangles.size());

                const std::size_t byteSizeNodes = sizeof(pt::BvhNode) * bvh2.nodes.size();
                const std::size_t byteSizeTriangles = sizeof(pt::Triangle) * bvh2.triangles.size();
                REQUIRE(std::memcmp(bvh2.nodes.data(), bvh.nodes.data(), byteSizeNodes) == 0);
                REQUIRE(
                    std::memcmp(bvh2.triangles.data(), bvh.triangles.data(), byteSizeTriangles) ==
                    0);
            }
        }
    }
}

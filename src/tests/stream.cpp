#include <common/assert.hpp>
#include <common/buffer_stream.hpp>
#include <common/bvh.hpp>
#include <common/flattened_model.hpp>
#include <common/file_stream.hpp>
#include <common/gltf_model.hpp>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace nlrs;
namespace fs = std::filesystem;

void serialize(OutputStream& ostream, const Bvh& bvh)
{
    {
        const std::size_t nodeCount = bvh.nodes.size();
        ostream.write(reinterpret_cast<const char*>(&nodeCount), sizeof(std::size_t));
        ostream.write(reinterpret_cast<const char*>(bvh.nodes.data()), sizeof(BvhNode) * nodeCount);
    }
    {
        const std::size_t idxCount = bvh.triangleIndices.size();
        ostream.write(reinterpret_cast<const char*>(&idxCount), sizeof(std::size_t));
        ostream.write(
            reinterpret_cast<const char*>(bvh.triangleIndices.data()),
            sizeof(std::size_t) * idxCount);
    }
}

void deserialize(InputStream& istream, Bvh& bvh)
{
    {
        std::size_t nodeCount;
        istream.read(reinterpret_cast<char*>(&nodeCount), sizeof(std::size_t));

        bvh.nodes.resize(nodeCount);
        const std::size_t numBytes = sizeof(BvhNode) * nodeCount;
        NLRS_ASSERT(istream.read(reinterpret_cast<char*>(bvh.nodes.data()), numBytes) == numBytes);
    }
    {
        std::size_t idxCount;
        istream.read(reinterpret_cast<char*>(&idxCount), sizeof(std::size_t));
        const std::size_t numBytes = sizeof(std::size_t) * idxCount;
        bvh.triangleIndices.resize(idxCount);
        NLRS_ASSERT(
            istream.read(reinterpret_cast<char*>(bvh.triangleIndices.data()), numBytes) ==
            numBytes);
    }
}

SCENARIO("Serialize and deserialize a bvh", "[stream]")
{
    GIVEN("A bvh")
    {
        GltfModel      model{"Duck.glb"};
        FlattenedModel flattenedModel{model};
        REQUIRE_FALSE(flattenedModel.positions.empty());

        const Bvh sourceBvh = buildBvh(std::span(flattenedModel.positions));
        REQUIRE_FALSE(sourceBvh.nodes.empty());
        REQUIRE_FALSE(sourceBvh.triangleIndices.empty());

        WHEN("serializing to a BufferStream")
        {
            BufferStream bufferStream;
            serialize(bufferStream, sourceBvh);

            THEN("deserializing from BufferStream yields original bvh")
            {
                Bvh bvh;
                deserialize(bufferStream, bvh);

                REQUIRE(bvh.nodes.size() == sourceBvh.nodes.size());
                REQUIRE(bvh.triangleIndices.size() == sourceBvh.triangleIndices.size());

                const std::size_t byteSizeNodes = sizeof(BvhNode) * bvh.nodes.size();
                const std::size_t byteSizeIndices =
                    sizeof(std::size_t) * bvh.triangleIndices.size();
                REQUIRE(std::memcmp(bvh.nodes.data(), sourceBvh.nodes.data(), byteSizeNodes) == 0);
                REQUIRE(
                    std::memcmp(
                        bvh.triangleIndices.data(),
                        sourceBvh.triangleIndices.data(),
                        byteSizeIndices) == 0);
            }
        }

        WHEN("serializing to OutputFileStream")
        {
            const fs::path testFile = fs::current_path() / fs::path("bvh.testbin");
            {
                OutputFileStream file(testFile);
                serialize(file, sourceBvh);
            }

            THEN("deserializing from InputFileStream equals original bvh")
            {
                Bvh bvh;
                {
                    InputFileStream file(testFile);
                    deserialize(file, bvh);
                }

                REQUIRE(bvh.nodes.size() == sourceBvh.nodes.size());
                REQUIRE(bvh.triangleIndices.size() == sourceBvh.triangleIndices.size());

                const std::size_t byteSizeNodes = sizeof(BvhNode) * bvh.nodes.size();
                const std::size_t byteSizeIndices =
                    sizeof(std::size_t) * bvh.triangleIndices.size();
                REQUIRE(std::memcmp(bvh.nodes.data(), sourceBvh.nodes.data(), byteSizeNodes) == 0);
                REQUIRE(
                    std::memcmp(
                        bvh.triangleIndices.data(),
                        sourceBvh.triangleIndices.data(),
                        byteSizeIndices) == 0);
            }
            fs::remove(testFile);
        }
    }
}

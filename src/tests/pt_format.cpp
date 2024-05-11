#include <common/buffer_stream.hpp>
#include <common/file_stream.hpp>
#include <pt-format/pt_format.hpp>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string_view>

namespace fs = std::filesystem;
using namespace nlrs;

SCENARIO("Serialize and deserialize PtFormat", "[pt-format]")
{
    GIVEN("A pt format instance")
    {
        PtFormat ptFormat{"Duck.glb"};

        WHEN("serializing to a buffer stream")
        {
            BufferStream stream;
            serialize(stream, ptFormat);

            THEN("deserializing from the buffer stream yields the same object")
            {
                PtFormat deserializedPtFormat;
                deserialize(stream, deserializedPtFormat);

                REQUIRE(
                    std::memcmp(
                        ptFormat.bvhNodes.data(),
                        deserializedPtFormat.bvhNodes.data(),
                        ptFormat.bvhNodes.size() * sizeof(BvhNode)) == 0);
                const std::size_t bvhPositionAttributesBytes =
                    ptFormat.bvhPositionAttributes.size() * sizeof(Positions);
                REQUIRE(
                    std::memcmp(
                        ptFormat.bvhPositionAttributes.data(),
                        deserializedPtFormat.bvhPositionAttributes.data(),
                        bvhPositionAttributesBytes) == 0);
                const std::size_t trianglePositionAttributesBytes =
                    ptFormat.trianglePositionAttributes.size() * sizeof(PositionAttribute);
                REQUIRE(
                    std::memcmp(
                        ptFormat.trianglePositionAttributes.data(),
                        deserializedPtFormat.trianglePositionAttributes.data(),
                        trianglePositionAttributesBytes) == 0);
                const std::size_t triangleVertexAttributesBytes =
                    ptFormat.triangleVertexAttributes.size() * sizeof(VertexAttributes);
                REQUIRE(
                    std::memcmp(
                        ptFormat.triangleVertexAttributes.data(),
                        deserializedPtFormat.triangleVertexAttributes.data(),
                        triangleVertexAttributesBytes) == 0);
                REQUIRE(
                    ptFormat.baseColorTextures.size() ==
                    deserializedPtFormat.baseColorTextures.size());
                for (std::size_t i = 0; i < ptFormat.baseColorTextures.size(); ++i)
                {
                    const auto& sourceTexture = ptFormat.baseColorTextures[i];
                    const auto& destTexture = deserializedPtFormat.baseColorTextures[i];
                    REQUIRE(sourceTexture.dimensions().width == destTexture.dimensions().width);
                    REQUIRE(sourceTexture.dimensions().height == destTexture.dimensions().height);
                    const std::size_t pixelBytes =
                        sourceTexture.pixels().size() * sizeof(Texture::BgraPixel);
                    REQUIRE(
                        std::memcmp(
                            sourceTexture.pixels().data(),
                            destTexture.pixels().data(),
                            pixelBytes) == 0);
                }
            }
        }

        const fs::path testFile = fs::current_path() / fs::path("test.pt");

        WHEN("serializing to OutputFileStream")
        {
            {
                OutputFileStream file(testFile);
                serialize(file, ptFormat);
            }

            THEN("deserializing using InputFileStream should yield the same pt format")
            {
                PtFormat deserializedPtFormat;
                {
                    InputFileStream file(testFile);
                    deserialize(file, deserializedPtFormat);
                }

                REQUIRE(
                    std::memcmp(
                        ptFormat.bvhNodes.data(),
                        deserializedPtFormat.bvhNodes.data(),
                        ptFormat.bvhNodes.size() * sizeof(BvhNode)) == 0);

                const std::size_t bvhPositionAttributesBytes =
                    ptFormat.bvhPositionAttributes.size() * sizeof(Positions);
                REQUIRE(
                    std::memcmp(
                        ptFormat.bvhPositionAttributes.data(),
                        deserializedPtFormat.bvhPositionAttributes.data(),
                        bvhPositionAttributesBytes) == 0);

                const std::size_t trianglePositionAttributesBytes =
                    ptFormat.trianglePositionAttributes.size() * sizeof(PositionAttribute);
                REQUIRE(
                    std::memcmp(
                        ptFormat.trianglePositionAttributes.data(),
                        deserializedPtFormat.trianglePositionAttributes.data(),
                        trianglePositionAttributesBytes) == 0);

                const std::size_t triangleVertexAttributesBytes =
                    ptFormat.triangleVertexAttributes.size() * sizeof(VertexAttributes);
                REQUIRE(
                    std::memcmp(
                        ptFormat.triangleVertexAttributes.data(),
                        deserializedPtFormat.triangleVertexAttributes.data(),
                        triangleVertexAttributesBytes) == 0);

                REQUIRE(
                    ptFormat.baseColorTextures.size() ==
                    deserializedPtFormat.baseColorTextures.size());
                for (std::size_t i = 0; i < ptFormat.baseColorTextures.size(); ++i)
                {
                    const auto& sourceTexture = ptFormat.baseColorTextures[i];
                    const auto& destTexture = deserializedPtFormat.baseColorTextures[i];
                    REQUIRE(sourceTexture.dimensions().width == destTexture.dimensions().width);
                    REQUIRE(sourceTexture.dimensions().height == destTexture.dimensions().height);
                    const std::size_t pixelBytes =
                        sourceTexture.pixels().size() * sizeof(Texture::BgraPixel);
                    REQUIRE(
                        std::memcmp(
                            sourceTexture.pixels().data(),
                            destTexture.pixels().data(),
                            pixelBytes) == 0);
                }
            }

            BENCHMARK("deserializing pt format from a file")
            {
                PtFormat              deserializedPtFormat;
                nlrs::InputFileStream file(testFile);
                nlrs::deserialize(file, deserializedPtFormat);
            };
        }

        fs::remove(testFile);
    }
}

SCENARIO("invalid magic bytes", "[pt-format]")
{
    GIVEN("mismatching magic bytes")
    {
        constexpr std::string_view magicBytes = "PTFORMAT0";

        BufferStream stream;
        stream.write(magicBytes.data(), magicBytes.size());

        THEN("deserializing should throw")
        {
            PtFormat format;
            REQUIRE_THROWS_WITH(
                deserialize(stream, format),
                "Mismatching PtFormat file version. Invalid version in magic bytes: expected "
                "'PTFORMAT2', got 'PTFORMAT0'.");
        }
    }

    GIVEN("Invalid magic bytes")
    {
        constexpr std::string_view magicBytes = "INVALID  ";

        BufferStream stream;
        stream.write(magicBytes.data(), magicBytes.size());

        THEN("deserializing should throw")
        {
            PtFormat format;
            REQUIRE_THROWS_WITH(
                deserialize(stream, format), "Invalid file format: expected PtFormat file.");
        }
    }
}

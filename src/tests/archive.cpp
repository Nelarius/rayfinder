#include <common/assert.hpp>
#include <common/bvh.hpp>
#include <common/flattened_model.hpp>
#include <common/gltf_model.hpp>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace nlrs
{
class OutputStream
{
public:
    virtual void write(const char* data, std::size_t numBytes) = 0;
};

class InputStream
{
public:
    virtual std::size_t read(char* data, std::size_t numBytes) = 0;
};

class BufferStream : public InputStream, public OutputStream
{
public:
    BufferStream() = default;
    ~BufferStream() = default;

    virtual std::size_t read(char* data, std::size_t numBytes)
    {
        mBufferStream.read(data, numBytes);
        return mBufferStream.gcount();
    }

    virtual void write(const char* data, std::size_t numBytes)
    {
        mBufferStream.write(data, numBytes);
    }

private:
    std::basic_stringstream<char> mBufferStream;
};

class InputFileStream : public InputStream
{
public:
    InputFileStream(const std::filesystem::path file)
    {
        mFileStream.open(file, std::ios::binary | std::ios::in);
        if (!mFileStream.is_open())
        {
            throw std::runtime_error(fmt::format("Failed to open file: {}", file.c_str()));
        }
    }
    ~InputFileStream() = default;

    virtual std::size_t read(char* data, std::size_t numBytes) override
    {
        mFileStream.read(data, numBytes);
        return mFileStream.gcount();
    }

private:
    std::ifstream mFileStream;
};

class OutputFileStream : public OutputStream
{
public:
    OutputFileStream(std::filesystem::path file)
    {
        mFileStream.open(file, std::ios::binary | std::ios::out);
        if (!mFileStream.is_open())
        {
            throw std::runtime_error(fmt::format("Failed to open file: {}", file.c_str()));
        }
    }
    ~OutputFileStream() = default;

    virtual void write(const char* data, std::size_t numBytes) override
    {
        mFileStream.write(data, numBytes);
    }

private:
    std::ofstream mFileStream;
};

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

void serialize(OutputStream& ostream, const GltfModel& model)
{
    const std::size_t numMeshes = model.meshes().size();
    ostream.write(reinterpret_cast<const char*>(&numMeshes), sizeof(std::size_t));
    for (const GltfMesh& mesh : model.meshes())
    {
        const std::size_t numPositions = mesh.positions().size();
        ostream.write(reinterpret_cast<const char*>(&numPositions), sizeof(std::size_t));
        ostream.write(
            reinterpret_cast<const char*>(mesh.positions().data()),
            sizeof(glm::vec3) * numPositions);

        const std::size_t numNormals = mesh.normals().size();
        ostream.write(reinterpret_cast<const char*>(&numNormals), sizeof(std::size_t));
        ostream.write(
            reinterpret_cast<const char*>(mesh.normals().data()), sizeof(glm::vec3) * numNormals);

        const std::size_t numTexCoords = mesh.texCoords().size();
        ostream.write(reinterpret_cast<const char*>(&numTexCoords), sizeof(std::size_t));
        ostream.write(
            reinterpret_cast<const char*>(mesh.texCoords().data()),
            sizeof(glm::vec2) * numTexCoords);

        const std::size_t numIndices = mesh.indices().size();
        ostream.write(reinterpret_cast<const char*>(&numIndices), sizeof(std::size_t));
        ostream.write(
            reinterpret_cast<const char*>(mesh.indices().data()),
            sizeof(std::uint32_t) * numIndices);

        const std::size_t baseColorTextureIndex = mesh.baseColorTextureIndex();
        ostream.write(reinterpret_cast<const char*>(&baseColorTextureIndex), sizeof(std::size_t));
    }

    const std::size_t numTextures = model.baseColorTextures().size();
    ostream.write(reinterpret_cast<const char*>(&numTextures), sizeof(std::size_t));
    for (const Texture& texture : model.baseColorTextures())
    {
        const std::size_t numPixels = texture.pixels().size();
        ostream.write(reinterpret_cast<const char*>(&numPixels), sizeof(std::size_t));
        ostream.write(
            reinterpret_cast<const char*>(texture.pixels().data()),
            sizeof(Texture::RgbaPixel) * numPixels);

        const auto dimensions = texture.dimensions();
        ostream.write(reinterpret_cast<const char*>(&dimensions), sizeof(Texture::Dimensions));
    }
}

void deserialize(InputStream& istream, GltfModel& model)
{
    std::vector<GltfMesh> meshes;
    std::size_t           numMeshes;
    istream.read(reinterpret_cast<char*>(&numMeshes), sizeof(std::size_t));
    meshes.reserve(numMeshes);
    for (std::size_t i = 0; i < numMeshes; ++i)
    {
        std::size_t numPositions;
        istream.read(reinterpret_cast<char*>(&numPositions), sizeof(std::size_t));
        std::vector<glm::vec3> positions(numPositions);
        const std::size_t      numBytes = sizeof(glm::vec3) * numPositions;
        NLRS_ASSERT(istream.read(reinterpret_cast<char*>(positions.data()), numBytes) == numBytes);

        std::size_t numNormals;
        istream.read(reinterpret_cast<char*>(&numNormals), sizeof(std::size_t));
        std::vector<glm::vec3> normals(numNormals);
        const std::size_t      numBytesNormals = sizeof(glm::vec3) * numNormals;
        NLRS_ASSERT(
            istream.read(reinterpret_cast<char*>(normals.data()), numBytesNormals) ==
            numBytesNormals);

        std::size_t numTexCoords;
        istream.read(reinterpret_cast<char*>(&numTexCoords), sizeof(std::size_t));
        std::vector<glm::vec2> texCoords(numTexCoords);
        const std::size_t      numBytesTexCoords = sizeof(glm::vec2) * numTexCoords;
        NLRS_ASSERT(
            istream.read(reinterpret_cast<char*>(texCoords.data()), numBytesTexCoords) ==
            numBytesTexCoords);

        std::size_t numIndices;
        istream.read(reinterpret_cast<char*>(&numIndices), sizeof(std::size_t));
        std::vector<std::uint32_t> indices(numIndices);
        const std::size_t          numBytesIndices = sizeof(std::uint32_t) * numIndices;
        NLRS_ASSERT(
            istream.read(reinterpret_cast<char*>(indices.data()), numBytesIndices) ==
            numBytesIndices);

        std::size_t baseColorTextureIndex;
        istream.read(reinterpret_cast<char*>(&baseColorTextureIndex), sizeof(std::size_t));

        meshes.emplace_back(
            std::move(positions),
            std::move(normals),
            std::move(texCoords),
            std::move(indices),
            baseColorTextureIndex);
    }

    std::vector<Texture> textures;
    std::size_t          numTextures;
    istream.read(reinterpret_cast<char*>(&numTextures), sizeof(std::size_t));
    textures.reserve(numTextures);
    for (std::size_t i = 0; i < numTextures; ++i)
    {
        std::size_t numPixels;
        istream.read(reinterpret_cast<char*>(&numPixels), sizeof(std::size_t));
        std::vector<Texture::RgbaPixel> pixels(numPixels);
        const std::size_t               numBytes = sizeof(Texture::RgbaPixel) * numPixels;
        NLRS_ASSERT(istream.read(reinterpret_cast<char*>(pixels.data()), numBytes) == numBytes);

        Texture::Dimensions dimensions;
        istream.read(reinterpret_cast<char*>(&dimensions), sizeof(Texture::Dimensions));

        textures.emplace_back(std::move(pixels), dimensions);
    }

    model = GltfModel{std::move(meshes), std::move(textures)};
}
} // namespace nlrs

namespace fs = std::filesystem;

SCENARIO("Serialize and deserializing a bvh", "[archive]")
{
    GIVEN("a BVH")
    {
        nlrs::GltfModel      model{"Duck.glb"};
        nlrs::FlattenedModel flattenedModel{model};
        REQUIRE_FALSE(flattenedModel.positions().empty());

        const nlrs::Bvh sourceBvh = buildBvh(flattenedModel.positions());
        REQUIRE_FALSE(sourceBvh.nodes.empty());
        REQUIRE_FALSE(sourceBvh.triangleIndices.empty());

        WHEN("serializing to a BufferStream")
        {
            nlrs::BufferStream bufferStream;
            nlrs::serialize(bufferStream, sourceBvh);

            THEN("deserializing from BufferStream equals original bvh")
            {
                nlrs::Bvh bvh;
                nlrs::deserialize(bufferStream, bvh);

                REQUIRE(bvh.nodes.size() == sourceBvh.nodes.size());
                REQUIRE(bvh.triangleIndices.size() == sourceBvh.triangleIndices.size());

                const std::size_t byteSizeNodes = sizeof(nlrs::BvhNode) * bvh.nodes.size();
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
                nlrs::OutputFileStream file(testFile);
                nlrs::serialize(file, sourceBvh);
            }

            THEN("deserializing from InputFileStream equals original bvh")
            {
                nlrs::Bvh bvh;
                {
                    nlrs::InputFileStream file(testFile);
                    nlrs::deserialize(file, bvh);
                }

                REQUIRE(bvh.nodes.size() == sourceBvh.nodes.size());
                REQUIRE(bvh.triangleIndices.size() == sourceBvh.triangleIndices.size());

                const std::size_t byteSizeNodes = sizeof(nlrs::BvhNode) * bvh.nodes.size();
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

SCENARIO("Archiving a model without compression", "[archive]")
{
    GIVEN("a model")
    {
        const nlrs::GltfModel model{"Duck.glb"};
        const fs::path        testFile = fs::current_path() / fs::path("model.testbin");

        WHEN("serializing to OutputFileStream")
        {
            {
                nlrs::OutputFileStream file(testFile);
                nlrs::serialize(file, model);
            }

            THEN("deserializing using InputFileStream should yield the same model")
            {
                nlrs::GltfModel deserializedModel;
                {
                    nlrs::InputFileStream file(testFile);
                    nlrs::deserialize(file, deserializedModel);
                }
                REQUIRE(deserializedModel == model);

                BENCHMARK("deserializing uncompressed data from a file")
                {
                    nlrs::InputFileStream file(testFile);
                    nlrs::deserialize(file, deserializedModel);
                };
            }
        }

        fs::remove(testFile);
    }
}

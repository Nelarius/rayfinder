#include <common/assert.hpp>
#include <common/bvh.hpp>
#include <common/flattened_model.hpp>
#include <common/gltf_model.hpp>

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>
#include <zstd.h>

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

class CompressingStreamAdapter : public OutputStream
{
public:
    CompressingStreamAdapter(OutputStream& ostream)
        : mOutputStream(ostream),
          mBuffer()
    {
        mBuffer.reserve(1 << 20);
    }

    CompressingStreamAdapter(const CompressingStreamAdapter&) = delete;
    CompressingStreamAdapter& operator=(const CompressingStreamAdapter&) = delete;

    CompressingStreamAdapter(CompressingStreamAdapter&&) = delete;
    CompressingStreamAdapter& operator=(CompressingStreamAdapter&&) = delete;

    ~CompressingStreamAdapter()
    {
        const std::size_t compressedSize = ZSTD_compressBound(mBuffer.size());
        std::vector<char> compressedBuffer(compressedSize);
        const std::size_t compressedBytes = ZSTD_compress(
            compressedBuffer.data(), compressedSize, mBuffer.data(), mBuffer.size(), 1);
        NLRS_ASSERT(ZSTD_isError(compressedBytes) == 0);
        mOutputStream.write(compressedBuffer.data(), compressedBytes);
    }

    virtual void write(const char* data, std::size_t numBytes) override
    {
        const std::size_t offset = mBuffer.size();
        mBuffer.resize(offset + numBytes);
        std::memcpy(mBuffer.data() + offset, data, numBytes);
    }

private:
    OutputStream&     mOutputStream;
    std::vector<char> mBuffer;
};

class DecompressingStreamAdapter : public InputStream
{
public:
    DecompressingStreamAdapter(InputStream& istream)
        : mInputStream(istream),
          mDecompressedBuffer(),
          mOffset(0)
    {
        std::vector<char> compressedBuffer;
        compressedBuffer.reserve(1 << 20);
        constexpr std::size_t chunkBytes = 1 << 10;
        std::size_t           bytesRead = 0;
        // NOTE: the compressedSize should be used instead of compressedBuffer.size() since the
        // buffer is expanded by chunk size.
        std::size_t compressedSize = 0;
        do
        {
            const std::size_t offset = compressedBuffer.size();
            compressedBuffer.resize(offset + chunkBytes);
            bytesRead = mInputStream.read(compressedBuffer.data() + offset, chunkBytes);
            compressedSize += bytesRead;
        } while (bytesRead == chunkBytes);

        const std::size_t decompressedSize =
            ZSTD_getFrameContentSize(compressedBuffer.data(), compressedSize);
        NLRS_ASSERT(decompressedSize != ZSTD_CONTENTSIZE_ERROR);
        NLRS_ASSERT(decompressedSize != ZSTD_CONTENTSIZE_UNKNOWN);
        mDecompressedBuffer.resize(decompressedSize);
        NLRS_ASSERT(
            ZSTD_decompress(
                mDecompressedBuffer.data(),
                decompressedSize,
                compressedBuffer.data(),
                compressedSize) == decompressedSize);
    }

    DecompressingStreamAdapter(const DecompressingStreamAdapter&) = delete;
    DecompressingStreamAdapter& operator=(const DecompressingStreamAdapter&) = delete;

    DecompressingStreamAdapter(DecompressingStreamAdapter&&) = delete;
    DecompressingStreamAdapter& operator=(DecompressingStreamAdapter&&) = delete;

    ~DecompressingStreamAdapter() = default;

    virtual std::size_t read(char* data, std::size_t numBytes) override
    {
        const std::size_t bytesToRead = std::min(numBytes, mDecompressedBuffer.size() - mOffset);
        std::memcpy(data, mDecompressedBuffer.data() + mOffset, bytesToRead);
        mOffset += bytesToRead;
        return bytesToRead;
    }

private:
    InputStream&      mInputStream;
    std::vector<char> mDecompressedBuffer;
    std::size_t       mOffset;
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
} // namespace nlrs

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

        WHEN("serializing to a OutputFileStream")
        {
            namespace fs = std::filesystem;
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

        WHEN("serializing to a CompressingStreamAdapter")
        {
            nlrs::BufferStream bufferStream;
            {
                nlrs::CompressingStreamAdapter compressingStream(bufferStream);
                nlrs::serialize(compressingStream, sourceBvh);
            }

            THEN("deserializing from DecompressingStreamAdapter equals original bvh")
            {
                nlrs::Bvh bvh;
                {
                    nlrs::DecompressingStreamAdapter decompressingStream(bufferStream);
                    nlrs::deserialize(decompressingStream, bvh);
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
        }
    }
}

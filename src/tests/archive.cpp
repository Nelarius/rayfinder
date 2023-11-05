#include <common/bvh.hpp>
#include <common/gltf_model.hpp>

#include <catch2/catch_test_macros.hpp>
#include <zlib.h>

#include <cstring>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace pt
{
class OutputStream
{
public:
    virtual void write(const char* data, std::size_t numBytes) = 0;
};

class InputStream
{
public:
    virtual void read(char* data, std::size_t numBytes) = 0;
};

class BufferStream : public InputStream, public OutputStream
{
public:
    BufferStream() = default;
    ~BufferStream() = default;

    // TODO: what about the number of read bytes?
    virtual void read(char* data, std::size_t numBytes) { mBufferStream.read(data, numBytes); }

    virtual void write(const char* data, std::size_t numBytes)
    {
        mBufferStream.write(data, numBytes);
    }

    std::string_view view() const { return mBufferStream.view(); }

private:
    std::basic_stringstream<char> mBufferStream;
};

class InputFileStream : public InputStream
{
public:
    InputFileStream(const std::string_view filename)
    {
        mFileStream.open(filename.data(), std::ios::binary | std::ios::in);
        if (!mFileStream.is_open())
        {
            throw std::runtime_error(std::format("Failed to open file: {}", filename.data()));
        }
    }
    ~InputFileStream() = default;

    virtual void read(char* data, std::size_t numBytes) override
    {
        mFileStream.read(data, numBytes);
    }

private:
    std::ifstream mFileStream;
};

class OutputFileStream : public OutputStream
{
public:
    OutputFileStream(const std::string_view filename)
    {
        mFileStream.open(filename.data(), std::ios::binary | std::ios::out);
        if (!mFileStream.is_open())
        {
            throw std::runtime_error(std::format("Failed to open file: {}", filename.data()));
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

// Compresses the bytes written to this stream using zlib. Writes the compressed bytes to the given
// output stream.
template<std::size_t CHUNK_SIZE = 1024>
class DeflatingStreamAdapter : public OutputStream
{
public:
    DeflatingStreamAdapter(OutputStream& ostream)
        : mOutputStream(ostream),
          mBytes(),
          mZstream()
    {
        mBytes.reserve(1024);

        // Source: https://github.com/madler/zlib/blob/develop/examples/zpipe.c

        // Allocate deflate state
        mZstream.zalloc = Z_NULL;
        mZstream.zfree = Z_NULL;
        mZstream.opaque = Z_NULL;
        // Compression levels are from 0 (no compression) to 9 (maximum compression).
        const int ret = deflateInit(&mZstream, Z_DEFAULT_COMPRESSION);
        if (ret != Z_OK)
        {
            // TODO: a custom exception type?
            throw std::runtime_error("Failed to initialize zlib deflate stream");
        }
    }

    ~DeflatingStreamAdapter()
    {
        if (mBytes.empty())
        {
            inflateEnd(&mZstream);
            return;
        }

        mZstream.avail_in = static_cast<uInt>(mBytes.size());
        mZstream.next_in = reinterpret_cast<Bytef*>(mBytes.data());

        std::array<char, CHUNK_SIZE> chunk;

        int state = Z_OK;
        do
        {
            mZstream.avail_out = CHUNK_SIZE;
            mZstream.next_out = reinterpret_cast<Bytef*>(chunk.data());

            // `deflate` can be called with flush = Z_FINISH if the intention is to run the
            // compression in one pass. In this case, `deflate` will return Z_STREAM_END when all
            // input has been consumed.
            state = deflate(&mZstream, Z_FINISH);
            assert(state != Z_STREAM_ERROR); // state not clobbered

            const std::size_t numBytes = CHUNK_SIZE - mZstream.avail_out;
            if (numBytes > 0)
            {
                mOutputStream.write(chunk.data(), numBytes);
            }
        } while (state != Z_STREAM_END); // loop while all input not consumed
        assert(mZstream.avail_in == 0);  // all input consumed
        inflateEnd(&mZstream);
    }

    virtual void write(const char* data, std::size_t numBytes) override
    {
        const std::size_t offset = mBytes.size();
        mBytes.resize(offset + numBytes);
        std::memcpy(mBytes.data() + offset, data, numBytes);
    }

private:
    OutputStream&     mOutputStream;
    std::vector<char> mBytes;
    z_stream          mZstream;
};

void serialize(OutputStream& ostream, const Bvh& bvh)
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

void deserialize(InputStream& istream, Bvh& bvh)
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
            pt::BufferStream bufferStream;
            pt::serialize(bufferStream, bvh);
            REQUIRE_FALSE(bufferStream.view().empty());

            THEN("bvh2 deserialized from buffer stream equals original bvh")
            {
                pt::Bvh bvh2;
                pt::deserialize(bufferStream, bvh2);

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
                pt::OutputFileStream file("bvh.testbin");
                pt::serialize(file, bvh);
            }

            THEN("bvh2 deserialized from ifstream equals original bvh")
            {
                pt::Bvh             bvh2;
                pt::InputFileStream file("bvh.testbin");
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

TEST_CASE("DeflatingStreamAdapter", "[archive]")
{
    pt::BufferStream bufferStream;
    {
        pt::DeflatingStreamAdapter deflatingStream(bufferStream);

        const std::string_view testString = "Hello world!";
        deflatingStream.write(testString.data(), testString.size());
    }

    const std::string_view compressedString = bufferStream.view();
    REQUIRE_FALSE(compressedString.empty());
}

TEST_CASE("DeflatingStreamAdapter writes to file", "[archive]")
{
    pt::GltfModel model("Duck.glb");
    REQUIRE_FALSE(model.triangles().empty());

    const pt::Bvh bvh = buildBvh(model.triangles());
    REQUIRE_FALSE(bvh.nodes.empty());
    REQUIRE_FALSE(bvh.triangles.empty());

    {
        pt::OutputFileStream       file("bvh.compressed");
        pt::DeflatingStreamAdapter deflatingStream(file);
        pt::serialize(deflatingStream, bvh);
    }

    {
        pt::OutputFileStream file("bvh.uncompressed");
        pt::serialize(file, bvh);
    }
}

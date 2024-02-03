#include "assert.hpp"
#include "buffer_stream.hpp"

namespace nlrs
{
std::size_t BufferStream::read(char* data, std::size_t numBytes)
{
    mBufferStream.read(data, numBytes);
    const auto bytesRead = mBufferStream.gcount();
    NLRS_ASSERT(bytesRead >= 0);
    return static_cast<std::size_t>(bytesRead);
}

void BufferStream::write(const char* data, std::size_t numBytes)
{
    mBufferStream.write(data, numBytes);
}
} // namespace nlrs

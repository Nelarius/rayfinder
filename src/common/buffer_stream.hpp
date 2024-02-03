#include "stream.hpp"

#include <cstddef>
#include <sstream>

namespace nlrs
{
class BufferStream : public InputStream, public OutputStream
{
public:
    BufferStream() = default;
    virtual ~BufferStream() = default;

    virtual std::size_t read(char* data, std::size_t numBytes) override;

    virtual void write(const char* data, std::size_t numBytes) override;

private:
    std::basic_stringstream<char> mBufferStream;
};
} // namespace nlrs

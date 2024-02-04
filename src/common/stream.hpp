#pragma once

#include <cstddef>

namespace nlrs
{
class OutputStream
{
public:
    virtual ~OutputStream() = default;
    virtual void write(const char* data, std::size_t numBytes) = 0;
};

class InputStream
{
public:
    virtual ~InputStream() = default;
    virtual std::size_t read(char* data, std::size_t numBytes) = 0;
};
} // namespace nlrs

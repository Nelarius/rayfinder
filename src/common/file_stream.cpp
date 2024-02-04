#include "assert.hpp"
#include "file_stream.hpp"

#include <fmt/format.h>

#include <stdexcept>

namespace nlrs
{
InputFileStream::InputFileStream(const std::filesystem::path file)
{
    mFileStream.open(file, std::ios::binary | std::ios::in);
    if (!mFileStream.is_open())
    {
        throw std::runtime_error(fmt::format("Failed to open file: {}", file.string()));
    }
}
std::size_t InputFileStream::read(char* data, std::size_t numBytes)
{
    mFileStream.read(data, numBytes);
    const auto bytesRead = mFileStream.gcount();
    NLRS_ASSERT(bytesRead >= 0);
    return static_cast<std::size_t>(bytesRead);
}

OutputFileStream::OutputFileStream(const std::filesystem::path file)
{
    mFileStream.open(file, std::ios::binary | std::ios::out);
    if (!mFileStream.is_open())
    {
        throw std::runtime_error(fmt::format("Failed to open file: {}", file.string()));
    }
}

void OutputFileStream::write(const char* data, std::size_t numBytes)
{
    mFileStream.write(data, numBytes);
}
} // namespace nlrs

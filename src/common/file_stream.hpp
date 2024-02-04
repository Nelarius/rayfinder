#pragma once

#include "stream.hpp"

#include <filesystem>
#include <fstream>

namespace nlrs
{
class InputFileStream : public InputStream
{
public:
    InputFileStream(std::filesystem::path);
    virtual ~InputFileStream() = default;

    virtual std::size_t read(char* data, std::size_t numBytes) override;

private:
    std::ifstream mFileStream;
};

class OutputFileStream : public OutputStream
{
public:
    OutputFileStream(std::filesystem::path);
    virtual ~OutputFileStream() = default;

    virtual void write(const char* data, std::size_t numBytes) override;

private:
    std::ofstream mFileStream;
};
} // namespace nlrs

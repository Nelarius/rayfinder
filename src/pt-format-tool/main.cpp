#include <common/file_stream.hpp>
#include <pt-format/pt_format.hpp>

#include <fmt/core.h>

#include <cstdio>
#include <exception>
#include <filesystem>

namespace fs = std::filesystem;
using namespace nlrs;

void printHelp() { std::printf("Usage:\n\tpt-format-tool <input_gltf_file>\n"); }

int main(int argc, char** argv)
try
{
    if (argc != 2)
    {
        printHelp();
        return 0;
    }

    fs::path path = argv[1];
    if (!fs::exists(path))
    {
        fmt::print(stderr, "File {} does not exist\n", path.string());
        return 1;
    }

    PtFormat ptFormat{path};
    path.replace_extension(".pt");
    OutputFileStream fileStream(path);
    serialize(fileStream, ptFormat);
}
catch (const std::exception& e)
{
    fmt::println(stderr, "Exception occurred. {}", e.what());
    return 1;
}
catch (...)
{
    fmt::println(stderr, "Unknown exception occurred.");
    return 1;
}
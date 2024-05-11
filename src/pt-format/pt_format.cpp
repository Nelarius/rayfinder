#include "pt_format.hpp"

#include <common/assert.hpp>
#include <common/gltf_model.hpp>
#include <common/flattened_model.hpp>
#include <common/stream.hpp>

#include <fmt/core.h>

#include <algorithm>
#include <exception>
#include <regex>
#include <span>
#include <string_view>
#include <string>
#include <utility>

namespace nlrs
{
PtFormat::PtFormat(std::filesystem::path gltfPath)
    : bvhNodes(),
      bvhPositionAttributes(),
      trianglePositionAttributes(),
      triangleVertexAttributes(),
      baseColorTextures()
{
    nlrs::GltfModel      model{gltfPath};
    const FlattenedModel flattenedModel{model};
    auto [nodes, triangleIndices] = nlrs::buildBvh(flattenedModel.positions);

    auto positions = nlrs::reorderAttributes(std::span(flattenedModel.positions), triangleIndices);
    const auto normals =
        nlrs::reorderAttributes(std::span(flattenedModel.normals), triangleIndices);
    const auto texCoords =
        nlrs::reorderAttributes(std::span(flattenedModel.texCoords), triangleIndices);
    const auto textureIndices =
        nlrs::reorderAttributes(std::span(flattenedModel.baseColorTextureIndices), triangleIndices);
    NLRS_ASSERT(positions.size() == normals.size());
    NLRS_ASSERT(positions.size() == texCoords.size());
    NLRS_ASSERT(positions.size() == textureIndices.size());

    std::vector<nlrs::PositionAttribute> positionAttributes;
    std::vector<nlrs::VertexAttributes>  vertexAttributes;
    positionAttributes.reserve(positions.size());
    vertexAttributes.reserve(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i)
    {
        const auto& ps = positions[i];
        const auto& ns = normals[i];
        const auto& uvs = texCoords[i];
        const auto  textureIdx = textureIndices[i];

        positionAttributes.push_back(
            nlrs::PositionAttribute{.p0 = ps.v0, .p1 = ps.v1, .p2 = ps.v2});
        vertexAttributes.push_back(nlrs::VertexAttributes{
            .n0 = ns.n0,
            .n1 = ns.n1,
            .n2 = ns.n2,
            .uv0 = uvs.uv0,
            .uv1 = uvs.uv1,
            .uv2 = uvs.uv2,
            .textureIdx = textureIdx});
    }

    bvhNodes = std::move(nodes);
    bvhPositionAttributes = std::move(positions);
    trianglePositionAttributes = std::move(positionAttributes);
    triangleVertexAttributes = std::move(vertexAttributes);
    baseColorTextures = std::move(model.baseColorTextures);
}

template<typename T>
void serialize(OutputStream& stream, const std::span<const T>& data)
{
    const std::size_t numElements = data.size();
    stream.write(reinterpret_cast<const char*>(&numElements), sizeof(std::size_t));
    const std::size_t numBytes = sizeof(T) * numElements;
    stream.write(reinterpret_cast<const char*>(data.data()), numBytes);
}

template<typename T>
void deserialize(InputStream& stream, std::vector<T>& data)
{
    std::size_t numElements;
    stream.read(reinterpret_cast<char*>(&numElements), sizeof(std::size_t));
    data.resize(numElements);
    const std::size_t numBytes = sizeof(T) * numElements;
    NLRS_ASSERT(stream.read(reinterpret_cast<char*>(data.data()), numBytes) == numBytes);
}

void serialize(OutputStream& stream, const Texture& texture)
{
    const auto dimensions = texture.dimensions();
    stream.write(reinterpret_cast<const char*>(&dimensions), sizeof(Texture::Dimensions));
    serialize(stream, texture.pixels());
}

void deserialize(InputStream& stream, Texture& texture)
{
    Texture::Dimensions dimensions;
    NLRS_ASSERT(
        stream.read(reinterpret_cast<char*>(&dimensions), sizeof(Texture::Dimensions)) ==
        sizeof(Texture::Dimensions));
    std::vector<Texture::BgraPixel> pixels;
    deserialize(stream, pixels);
    texture = Texture{std::move(pixels), dimensions};
}

constexpr std::string_view MAGIC_BYTES = "PTFORMAT2";

void serialize(OutputStream& stream, const PtFormat& format)
{
    stream.write(MAGIC_BYTES.data(), MAGIC_BYTES.size());

    serialize(stream, std::span(format.bvhNodes));
    serialize(stream, std::span(format.bvhPositionAttributes));
    serialize(stream, std::span(format.trianglePositionAttributes));
    serialize(stream, std::span(format.triangleVertexAttributes));

    {
        const std::size_t numTextures = format.baseColorTextures.size();
        stream.write(reinterpret_cast<const char*>(&numTextures), sizeof(std::size_t));
        std::for_each(
            format.baseColorTextures.begin(),
            format.baseColorTextures.end(),
            [&](const auto& texture) { serialize(stream, texture); });
    }
}

void deserialize(InputStream& stream, PtFormat& format)
{
    std::string magicBytes;
    magicBytes.resize(MAGIC_BYTES.size());
    NLRS_ASSERT(stream.read(magicBytes.data(), magicBytes.size()) == magicBytes.size());

    if (magicBytes != MAGIC_BYTES)
    {
        const std::regex pattern("PTFORMAT\\d");
        if (std::regex_search(magicBytes, pattern))
        {
            throw std::runtime_error(fmt::format(
                "Mismatching PtFormat file version. Invalid version in magic bytes: expected "
                "'{}', got '{}'.",
                MAGIC_BYTES,
                magicBytes));
        }
        else
        {
            throw std::runtime_error("Invalid file format: expected PtFormat file.");
        }
    }

    deserialize(stream, format.bvhNodes);
    deserialize(stream, format.bvhPositionAttributes);
    deserialize(stream, format.trianglePositionAttributes);
    deserialize(stream, format.triangleVertexAttributes);

    {
        std::size_t numTextures;
        NLRS_ASSERT(
            stream.read(reinterpret_cast<char*>(&numTextures), sizeof(std::size_t)) ==
            sizeof(std::size_t));
        format.baseColorTextures.resize(numTextures);
        std::for_each(
            format.baseColorTextures.begin(), format.baseColorTextures.end(), [&](auto& texture) {
                deserialize(stream, texture);
            });
    }
}
} // namespace nlrs

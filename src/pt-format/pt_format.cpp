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
      vertexPositions(),
      vertexNormals(),
      vertexTexCoords(),
      vertexIndices(),
      modelVertexPositions(),
      modelVertexNormals(),
      modelVertexTexCoords(),
      modelVertexIndices(),
      modelBaseColorTextureIndices(),
      baseColorTextures()
{
    nlrs::GltfModel model{gltfPath};

    {
        const FlattenedModel flattenedModel{model};
        auto [nodes, triangleIndices] = nlrs::buildBvh(flattenedModel.positions);

        auto positions =
            nlrs::reorderAttributes(std::span(flattenedModel.positions), triangleIndices);
        const auto normals =
            nlrs::reorderAttributes(std::span(flattenedModel.normals), triangleIndices);
        const auto texCoords =
            nlrs::reorderAttributes(std::span(flattenedModel.texCoords), triangleIndices);
        const auto textureIndices = nlrs::reorderAttributes(
            std::span(flattenedModel.baseColorTextureIndices), triangleIndices);
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

        // TODO: why move? why not just add directly to the members?
        bvhNodes = std::move(nodes);
        bvhPositionAttributes = std::move(positions);
        trianglePositionAttributes = std::move(positionAttributes);
        triangleVertexAttributes = std::move(vertexAttributes);
    }

    {
        const auto [numModelVertices, numModelIndices] =
            [&model]() -> std::tuple<std::size_t, std::size_t> {
            std::size_t numModelVertices = 0;
            std::size_t numModelIndices = 0;
            for (const auto& mesh : model.meshes)
            {
                numModelVertices += mesh.positions.size();
                numModelIndices += mesh.indices.size();
                NLRS_ASSERT(mesh.positions.size() == mesh.texCoords.size());
            }
            return std::make_tuple(numModelVertices, numModelIndices);
        }();

        vertexPositions.reserve(numModelVertices);
        modelVertexPositions.reserve(model.meshes.size());
        vertexNormals.reserve(numModelVertices);
        modelVertexNormals.reserve(model.meshes.size());
        vertexTexCoords.reserve(numModelVertices);
        modelVertexTexCoords.reserve(model.meshes.size());
        vertexIndices.reserve(numModelIndices);
        modelVertexIndices.reserve(model.meshes.size());

        for (const auto& mesh : model.meshes)
        {
            const std::size_t vertexOffsetIdx = vertexPositions.size();
            const std::size_t numVertices = mesh.positions.size();

            NLRS_ASSERT(mesh.positions.size() == mesh.normals.size());
            NLRS_ASSERT(mesh.positions.size() == mesh.texCoords.size());

            std::transform(
                mesh.positions.begin(),
                mesh.positions.end(),
                std::back_inserter(vertexPositions),
                [](const glm::vec3& v) -> glm::vec4 { return glm::vec4(v, 1.0f); });
            modelVertexPositions.push_back(
                std::span(vertexPositions).subspan(vertexOffsetIdx, numVertices));

            std::transform(
                mesh.normals.begin(),
                mesh.normals.end(),
                std::back_inserter(vertexNormals),
                [](const glm::vec3& n) -> glm::vec4 { return glm::vec4(n, 0.0f); });
            modelVertexNormals.push_back(
                std::span(vertexNormals).subspan(vertexOffsetIdx, numVertices));

            vertexTexCoords.insert(
                vertexTexCoords.end(), mesh.texCoords.begin(), mesh.texCoords.end());
            modelVertexTexCoords.push_back(
                std::span(vertexTexCoords).subspan(vertexOffsetIdx, numVertices));

            const std::size_t indexOffsetIdx = vertexIndices.size();
            const std::size_t numIndices = mesh.indices.size();
            vertexIndices.insert(vertexIndices.end(), mesh.indices.begin(), mesh.indices.end());
            modelVertexIndices.push_back(
                std::span<const std::uint32_t>(vertexIndices).subspan(indexOffsetIdx, numIndices));

            NLRS_ASSERT(
                mesh.baseColorTextureIndex <
                static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()));
            modelBaseColorTextureIndices.push_back(
                static_cast<std::uint32_t>(mesh.baseColorTextureIndex));
        }
    }

    baseColorTextures = std::move(model.baseColorTextures);
}

template<typename T>
void serialize(OutputStream& stream, const std::span<const T>& data)
{
    const std::uint64_t numElements = static_cast<std::uint64_t>(data.size());
    stream.write(reinterpret_cast<const char*>(&numElements), sizeof(std::uint64_t));
    const std::size_t numBytes = sizeof(T) * numElements;
    stream.write(reinterpret_cast<const char*>(data.data()), numBytes);
}

template<typename T>
void serialize(
    OutputStream&                              stream,
    const std::vector<T>&                      buffer,
    const std::span<const std::span<const T>>& slices)
{
    const std::uint64_t numSlices = static_cast<std::uint64_t>(slices.size());
    stream.write(reinterpret_cast<const char*>(&numSlices), sizeof(std::uint64_t));
    const T* const bufferBegin = buffer.data();
    for (const auto& offset : slices)
    {
        const T* const offsetBegin = offset.data();
        NLRS_ASSERT(bufferBegin <= offsetBegin);
        const std::uint64_t offsetIdx = static_cast<std::uint64_t>(offsetBegin - bufferBegin);
        const std::uint64_t numElements = static_cast<std::uint64_t>(offset.size());
        NLRS_ASSERT(offsetIdx + numElements <= buffer.size());
        stream.write(reinterpret_cast<const char*>(&offsetIdx), sizeof(std::uint64_t));
        stream.write(reinterpret_cast<const char*>(&numElements), sizeof(std::uint64_t));
    }
}

template<typename T>
void deserialize(
    InputStream&                     stream,
    const std::vector<T>&            buffer,
    std::vector<std::span<const T>>& slices)
{
    std::uint64_t numSlices;
    NLRS_ASSERT(
        stream.read(reinterpret_cast<char*>(&numSlices), sizeof(std::uint64_t)) ==
        sizeof(std::uint64_t));
    slices.reserve(static_cast<std::size_t>(numSlices));

    for (std::uint64_t i = 0; i < numSlices; ++i)
    {
        std::uint64_t offsetIdx;
        NLRS_ASSERT(
            stream.read(reinterpret_cast<char*>(&offsetIdx), sizeof(std::uint64_t)) ==
            sizeof(std::uint64_t));
        std::uint64_t numElements;
        NLRS_ASSERT(
            stream.read(reinterpret_cast<char*>(&numElements), sizeof(std::uint64_t)) ==
            sizeof(std::uint64_t));
        NLRS_ASSERT(offsetIdx + numElements <= buffer.size());
        slices.push_back(std::span<const T>{buffer}.subspan(offsetIdx, numElements));
    }
}

template<typename T>
void deserialize(InputStream& stream, std::vector<T>& data)
{
    std::uint64_t numElements;
    stream.read(reinterpret_cast<char*>(&numElements), sizeof(std::uint64_t));
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

constexpr std::string_view MAGIC_BYTES = "PTFORMAT3";

void serialize(OutputStream& stream, const PtFormat& format)
{
    stream.write(MAGIC_BYTES.data(), MAGIC_BYTES.size());

    serialize(stream, std::span(format.bvhNodes));
    serialize(stream, std::span(format.bvhPositionAttributes));
    serialize(stream, std::span(format.trianglePositionAttributes));
    serialize(stream, std::span(format.triangleVertexAttributes));

    serialize(stream, std::span(format.vertexPositions));
    serialize(stream, std::span(format.vertexNormals));
    serialize(stream, std::span(format.vertexTexCoords));
    serialize(stream, std::span(format.vertexIndices));

    serialize(stream, format.vertexPositions, std::span(format.modelVertexPositions));
    serialize(stream, format.vertexNormals, std::span(format.modelVertexNormals));
    serialize(stream, format.vertexTexCoords, std::span(format.modelVertexTexCoords));
    serialize(stream, format.vertexIndices, std::span(format.modelVertexIndices));
    serialize(stream, std::span(format.modelBaseColorTextureIndices));

    {
        const std::uint64_t numTextures =
            static_cast<std::uint64_t>(format.baseColorTextures.size());
        stream.write(reinterpret_cast<const char*>(&numTextures), sizeof(std::uint64_t));
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

    deserialize(stream, format.vertexPositions);
    deserialize(stream, format.vertexNormals);
    deserialize(stream, format.vertexTexCoords);
    deserialize(stream, format.vertexIndices);

    deserialize(stream, format.vertexPositions, format.modelVertexPositions);
    deserialize(stream, format.vertexNormals, format.modelVertexNormals);
    deserialize(stream, format.vertexTexCoords, format.modelVertexTexCoords);
    deserialize(stream, format.vertexIndices, format.modelVertexIndices);
    deserialize(stream, format.modelBaseColorTextureIndices);

    {
        std::uint64_t numTextures;
        NLRS_ASSERT(
            stream.read(reinterpret_cast<char*>(&numTextures), sizeof(std::uint64_t)) ==
            sizeof(std::uint64_t));
        format.baseColorTextures.resize(numTextures);
        std::for_each(
            format.baseColorTextures.begin(), format.baseColorTextures.end(), [&](auto& texture) {
                deserialize(stream, texture);
            });
    }
}
} // namespace nlrs

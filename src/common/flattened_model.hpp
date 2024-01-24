#pragma once

#include "triangle_attributes.hpp"

#include <span>
#include <vector>

namespace nlrs
{
class GltfModel;

// A FlattenedModel unrolls the triangle attributes based on the model's index buffer. The texture
// indices refer to the texture indices in the original model and are not copied over.
class FlattenedModel
{
public:
    FlattenedModel(const GltfModel&);

    std::span<const Positions>     positions() const { return mPositions; }
    std::span<const Normals>       normals() const { return mNormals; }
    std::span<const TexCoords>     texCoords() const { return mTexCoords; }
    std::span<const std::uint32_t> baseColorTextureIndices() const
    {
        return mBaseColorTextureIndices;
    }

private:
    std::vector<Positions>     mPositions;
    std::vector<Normals>       mNormals;
    std::vector<TexCoords>     mTexCoords;
    std::vector<std::uint32_t> mBaseColorTextureIndices;
};
} // namespace nlrs
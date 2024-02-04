#pragma once

#include "triangle_attributes.hpp"

#include <span>
#include <vector>

namespace nlrs
{
struct GltfModel;

// A FlattenedModel unrolls the triangle attributes based on the model's index buffer. The texture
// indices refer to the texture indices in the original model and are not copied over.
struct FlattenedModel
{
    FlattenedModel(const GltfModel&);

    std::vector<Positions>     positions;
    std::vector<Normals>       normals;
    std::vector<TexCoords>     texCoords;
    std::vector<std::uint32_t> baseColorTextureIndices;
};
} // namespace nlrs
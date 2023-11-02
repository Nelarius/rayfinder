#pragma once

#include "geometry.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace pt
{
class GltfModel
{
public:
    GltfModel(std::string_view gltfPath);

    std::span<const Triangle> triangles() const { return mTriangles; }

private:
    std::vector<Triangle> mTriangles;
};
} // namespace pt

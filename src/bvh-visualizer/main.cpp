#include <common/aabb.hpp>
#include <common/bvh.hpp>
#include <common/camera.hpp>
#include <common/extent.hpp>
#include <common/gltf_model.hpp>
#include <common/ray_intersection.hpp>
#include <common/units/angle.hpp>

#include <stb_image_write.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>

using namespace nlrs;

inline constexpr Extent2i imageSize = Extent2i{1280, 720};

void printHelp() { std::printf("Usage: bvh-visualizer <input_gltf_file>\n"); }

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printHelp();
        return 0;
    }

    const nlrs::GltfModel model(argv[1]);
    const nlrs::Bvh       bvh = nlrs::buildBvh(model.positions());
    const auto            triangles = reorderAttributes(model.positions(), bvh.triangleIndices);

    const Camera camera = [&bvh]() -> Camera {
        const BvhNode&  rootNode = bvh.nodes[0];
        const Aabb      rootAabb = Aabb(rootNode.aabb.min, rootNode.aabb.max);
        const glm::vec3 rootDiagonal = diagonal(rootAabb);
        const glm::vec3 rootCentroid = centroid(rootAabb);
        const int       maxDim = maxDimension(rootAabb);

        const float aperture = 0.0f;
        const float focusDistance = 1.0f;
        const Angle vfov = Angle::degrees(70.0f);

        return createCamera(
            rootCentroid -
                glm::vec3(-0.8 * rootDiagonal[maxDim], 0.0f, 0.8f * rootDiagonal[maxDim]),
            rootCentroid,
            aperture,
            focusDistance,
            vfov,
            aspectRatio(imageSize));
    }();

    std::vector<std::uint32_t> pixelData;
    pixelData.reserve(imageSize.x * imageSize.y);

    for (int i = 0; i < imageSize.y; ++i)
    {
        for (int j = 0; j < imageSize.x; ++j)
        {
            const float u = static_cast<float>(j) / static_cast<float>(imageSize.x);
            const float v = 1.0f - static_cast<float>(i + 1) / static_cast<float>(imageSize.y);

            const Ray ray = generateCameraRay(camera, u, v);

            Intersection intersect;
            BvhStats     bvhStats;
            rayIntersectBvh(ray, bvh.nodes, triangles, FLT_MAX, intersect, &bvhStats);

            const float         x = 0.01f * static_cast<float>(bvhStats.nodesVisited);
            const auto          p = static_cast<std::uint32_t>(std::min(x, 1.0f) * 255.0f);
            const std::uint32_t pixel = (255u << 24) | (p << 16) | (p << 8) | p;
            pixelData.push_back(pixel);
        }
    }

    const int numChannels = 4;
    const int strideBytes = imageSize.x * numChannels;

    [[maybe_unused]] const int result = stbi_write_png(
        "bvh-visualizer.png", imageSize.x, imageSize.y, numChannels, pixelData.data(), strideBytes);
    assert(result != 0);

    return 0;
}

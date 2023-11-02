#include <common/bvh.hpp>
#include <common/camera.hpp>
#include <common/geometry.hpp>
#include <common/gltf_model.hpp>
#include <common/ray_intersection.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits.h>

using namespace pt;

inline constexpr int imageWidth = 1280;
inline constexpr int imageHeight = 720;

int main()
{
    const pt::GltfModel model("Duck.glb");
    const pt::Bvh       bvh = pt::buildBvh(model.triangles());

    const Camera camera = [&bvh]() -> Camera {
        const BvhNode&  rootNode = bvh.nodes[0];
        const Aabb      rootAabb = rootNode.aabb;
        const glm::vec3 rootDiagonal = diagonal(rootAabb);
        const glm::vec3 rootCentroid = centroid(rootAabb);
        const int       maxDim = maxDimension(rootAabb);

        return createCamera(
            imageWidth,
            imageHeight,
            70.0f,
            rootCentroid -
                glm::vec3(-0.8 * rootDiagonal[maxDim], 0.0f, 0.8f * rootDiagonal[maxDim]),
            rootCentroid);
    }();

    // Outputs binary PPM image format, https://netpbm.sourceforge.net/doc/ppm.html
    std::cout << "P6\n";
    std::cout << imageWidth << ' ' << imageHeight << '\n';
    std::cout << "255\n";
    for (int i = 0; i < imageHeight; ++i)
    {
        for (int j = 0; j < imageWidth; ++j)
        {
            const float u = static_cast<float>(j) / static_cast<float>(imageWidth);
            // Binary PPM is stored from top to bottom. Invert v to flip the image vertically.
            const float v = 1.0f - static_cast<float>(i + 1) / static_cast<float>(imageHeight);

            const Ray ray = generateCameraRay(camera, u, v);

            Intersection intersect;
            BvhStats     bvhStats;
            rayIntersectBvh(ray, bvh, FLT_MAX, intersect, &bvhStats);

            const float x = 0.01f * static_cast<float>(bvhStats.nodesVisited);
            const auto  p = static_cast<std::uint8_t>(std::min(x, 1.0f) * 255.0f);

            std::cout << p << p << p;
        }
    }

    return 0;
}

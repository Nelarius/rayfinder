#include <common/bvh.hpp>
#include <common/camera.hpp>
#include <common/extent.hpp>
#include <common/geometry.hpp>
#include <common/gltf_model.hpp>
#include <common/ray_intersection.hpp>
#include <common/units/angle.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>

using namespace nlrs;

inline constexpr Extent2i imageSize = Extent2i{1280, 720};

int main()
{
    const nlrs::GltfModel model("Duck.glb");
    const nlrs::Bvh bvh = nlrs::buildBvh(model.positions(), model.normals(), model.texCoords());

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

    // Outputs binary PPM image format, https://netpbm.sourceforge.net/doc/ppm.html
    std::cout << "P6\n";
    std::cout << imageSize.x << ' ' << imageSize.y << '\n';
    std::cout << "255\n";
    for (int i = 0; i < imageSize.y; ++i)
    {
        for (int j = 0; j < imageSize.x; ++j)
        {
            const float u = static_cast<float>(j) / static_cast<float>(imageSize.x);
            // Binary PPM is stored from top to bottom. Invert v to flip the image vertically.
            const float v = 1.0f - static_cast<float>(i + 1) / static_cast<float>(imageSize.y);

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

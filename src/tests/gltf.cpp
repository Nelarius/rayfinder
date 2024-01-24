#include <common/gltf_model.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Loading Gltf model produces triangle output", "[gltf]")
{
    nlrs::GltfModel model("Duck.glb");
    REQUIRE_FALSE(model.meshes().empty());
    REQUIRE_FALSE(model.baseColorTextures().empty());
    for (const auto& mesh : model.meshes())
    {
        REQUIRE_FALSE(mesh.positions().empty());
        REQUIRE(mesh.positions().size() == mesh.normals().size());
        REQUIRE(mesh.positions().size() == mesh.texCoords().size());
        REQUIRE_FALSE(mesh.indices().empty());
        REQUIRE(mesh.baseColorTextureIndex() < model.baseColorTextures().size());
    }
}

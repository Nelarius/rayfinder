#include <common/gltf_model.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Loading Gltf model produces triangle output", "[gltf]")
{
    pt::GltfModel model("Duck.glb");

    REQUIRE_FALSE(model.positions().empty());
    REQUIRE(model.positions().size() == model.normals().size());
    REQUIRE(model.positions().size() == model.texCoords().size());
}

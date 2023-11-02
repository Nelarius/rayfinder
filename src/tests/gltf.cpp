#include <common/gltf_model.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Loading Gltf model produces triangle output", "[gltf]")
{
    pt::GltfModel model("Duck.glb");

    REQUIRE_FALSE(model.triangles().empty());
}

include(FetchContent)

# Declare
FetchContent_Declare(glfw
    GIT_REPOSITORY "https://github.com/glfw/glfw"
    GIT_TAG 7482de6) # 3.3.8

# Fetch
message(STATUS "Fetching glfw...")
FetchContent_MakeAvailable(glfw)

# Configure

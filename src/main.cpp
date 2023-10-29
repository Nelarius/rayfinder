#include <chrono>
#include <cstdio>

#include <thread>

#include <GLFW/glfw3.h>

inline constexpr int defaultWindowWidth = 640;
inline constexpr int defaultWindowHeight = 480;

int main()
{
    if (!glfwInit())
    {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    GLFWwindow* const window = glfwCreateWindow(
        defaultWindowWidth, defaultWindowHeight, "pt-playground 🛝", nullptr, nullptr);
    if (!window)
    {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    {
        glfwMakeContextCurrent(window);

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            glfwSwapBuffers(window);

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

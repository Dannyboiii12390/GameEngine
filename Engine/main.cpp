
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>

#include <chrono>
#include <omp.h>

#include "Renderer/Window.h"
#include "Core/Managers/SceneManager.h"
#include "Core/Scenes/TemplateScene.h"

#ifdef _DEBUG
#define LOG_DEBUG(msg) std::cout << msg << std::endl;
#else
#define LOG_DEBUG(msg)
#endif

#include "../Physics Engine/Shapes/Sphere.h"

/*
- Render Passes
- Render Graph
*/

#include <iostream>
#include <thread>
using namespace std;

class SharedObject {
    int _objectCounter;
    static int _classCounter;
    thread_local static int _threadLocalCounter;

public:
    SharedObject() : _objectCounter(0) {}

    void update() {
        _objectCounter++;
        _classCounter++;
        _threadLocalCounter++;

        output();
    }

    void output() const {
        cout << "objectCounter= " << _objectCounter <<
            "\nclassCounter= " << _classCounter <<
            "\nthreadLocalCounter= " << _threadLocalCounter << endl;
    }
};

int SharedObject::_classCounter = 0;
thread_local int SharedObject::_threadLocalCounter = 0;

auto threadMain = [](SharedObject& shareObject) {
    for (auto i = 0; i < 2; i++)
        shareObject.update();
    return 0;
    };

int main(int, char**) {
    cout << "Start program" << endl;

    {
        SharedObject shareObject;
        shareObject.output();

        thread t1(threadMain, ref(shareObject));
        this_thread::sleep_for(chrono::milliseconds(500));
        thread t2(threadMain, ref(shareObject));
        t1.join();
        t2.join();

        shareObject.output();
    }

    cout << "Part 2" << endl;

    {
        SharedObject shareObject;
        shareObject.output();

        thread t1(threadMain, ref(shareObject));
        this_thread::sleep_for(chrono::milliseconds(500));
        thread t2(threadMain, ref(shareObject));
        t1.join();
        t2.join();

        shareObject.output();
    }

    cout << "End program" << endl;
    return 0;
}

int GameMain()
{
    try
    {

        #ifdef _OPENMP
        std::cout << "OpenMP is enabled! Max threads: " << omp_get_max_threads() << std::endl;
        #endif

        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        auto getDeltaTime = []()
            {
                static auto timeLastFrame = std::chrono::high_resolution_clock::now();
                auto timeNow = std::chrono::high_resolution_clock::now();
                float deltaTime = std::chrono::duration<float>(timeNow - timeLastFrame).count();
                timeLastFrame = timeNow;
                return deltaTime;
            };

        const int width = 1280;
        const int height = 720;
        Window window(width, height, "Vulkan Engine");

        SceneManager sceneManager;

        VulkanRHI vulkanRHI;
		vulkanRHI.Initialise(&window);
        vulkanRHI.ToggleVSync(true);    

		sceneManager.AddScene(std::make_unique<TemplateScene>(window, &vulkanRHI));

		Physics::Sphere testSphere(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f);
		Physics::Sphere testSphere2(glm::vec3(0.5f, 0.0f, 0.0f), 1.0f);

		std::cout << "Colliding? " << (testSphere.isCollidingWith(testSphere2) ? "Yes" : "No") << std::endl;


        


        while (window.getGLFWwindow() && !glfwWindowShouldClose(window.getGLFWwindow()))
        {
            float deltaTime = getDeltaTime();

			IScene* scene = sceneManager.GetCurrentScene();
            scene->Update(deltaTime);
            scene->HandleInput(deltaTime);
            scene->Draw();

			static float timeAccumulator = 0.0f;
			//after 5 seconds, add a new scene on top of the current one to test scene management
			timeAccumulator += deltaTime;
            if (timeAccumulator > 5.0f)
            {
                sceneManager.AddScene(std::make_unique<TemplateScene>(window, &vulkanRHI));
                timeAccumulator = 0.0f;
            }
        }

        sceneManager.Shutdown();
        vulkanRHI.Shutdown();
        window.Shutdown();
    }


  
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}


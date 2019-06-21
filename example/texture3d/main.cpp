//
// Created by wws on 2019/6/21.
//

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>
#include <VulkanBuffer.hpp>
#include <VulkanDevice.hpp>
#include <comm/CommTool.hpp>
#include <comm/dbg.hpp>
#include <VulkanTexture.hpp>

class Example : public VulkanExampleBase {
private:
public:
    void render() override
    {
        if (!prepared)
            return;
        //draw();
    }

    Example() : VulkanExampleBase(true)
    {
        zoom = -2.5f;
        title = "3d texture";
        rotation = { 0.0f, 15.0f, 0.0f };
        settings.overlay = true;
    }

    ~Example(){

    }
};

#if defined(_WIN32)

Example *example;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (example != NULL)
    {
        example->handleMessages(hWnd, uMsg, wParam, lParam);
    }
    return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
//    for (size_t i = 0; i < __argc; i++) { Example::args.push_back(__argv[i]); };
//    example = new Example();
//    example->initVulkan();
//    example->setupWindow(hInstance, WndProc);
//    example->prepare();
//    example->renderLoop();
//    delete example;
    MessageBoxA(0,"hhh","tip",0);
    return 0;
}

#elif defined(__linux__)

// Linux entry point
Example *example;
static void handleEvent(const xcb_generic_event_t *event)
{
	if (example != NULL)
	{
		example->handleEvent(event);
	}
}
int main(const int argc, const char *argv[])
{
	for (size_t i = 0; i < argc; i++) { Example::args.push_back(argv[i]); };
	example = new Example();
	example->initVulkan();
	example->setupWindow();
	example->prepare();
	example->renderLoop();
	delete example;
	return 0;
}
#endif

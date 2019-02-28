#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>

#if defined(_WIN32)

//Triangle *triangle;
//LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
//{
//	if (triangle != NULL)
//	{
//		triangle->handleMessages(hWnd, uMsg, wParam, lParam);
//	}
//	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
//}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	/*for (size_t i = 0; i < __argc; i++) { Triangle::args.push_back(__argv[i]); };
	triangle = new Triangle();
	triangle->initVulkan();
	triangle->setupWindow(hInstance, WndProc);
	triangle->prepare();
	triangle->renderLoop();
	delete triangle;*/
	return 0;
}

#elif defined(__linux__)

// Linux entry point
//Triangle *triangle;
//static void handleEvent(const xcb_generic_event_t *event)
//{
//	if (triangle != NULL)
//	{
//		triangle->handleEvent(event);
//	}
//}
int main(const int argc, const char *argv[])
{
	/*for (size_t i = 0; i < argc; i++) { Triangle::args.push_back(argv[i]); };
	triangle = new Triangle();
	triangle->initVulkan();
	triangle->setupWindow();
	triangle->prepare();
	triangle->renderLoop();
	delete triangle;*/
	return 0;
}
#endif
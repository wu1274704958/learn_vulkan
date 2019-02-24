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
	MessageBox(NULL, "hello", "tip", MB_OK);
	return 0;
}

#else

int main()
{
	return 0;
}

#endif // 
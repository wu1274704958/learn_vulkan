#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>

class Triangle : public VulkanExampleBase
{
public:
	virtual void render() override {

	}

	Triangle() : VulkanExampleBase(false)
	{

	}
private:

};

#if defined(_WIN32)

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	Triangle tri;
	MessageBoxA(0, tri.getAssetPath().data(), "Tip", MB_OK);
	return 0;
}

#else

int main()
{

	return 0;
}

#endif // 


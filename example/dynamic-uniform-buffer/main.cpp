#include<cstdlib>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>
#include <VulkanBuffer.hpp>
#include <VulkanDevice.hpp>

#define OBJECT_INSTANCES 125

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
};

class Example : public VulkanExampleBase {
public:
	virtual void render() override {

	}
	Example() : VulkanExampleBase(true)
	{
		title = "dynamic-uniform-buffer";
		camera.type = Camera::CameraType::lookat;
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 512.0f);
		camera.setTranslation(glm::vec3(0.0f, 0.0f, -5.0f));
		settings.overlay = true;
	}
	~Example()
	{
		if (dynamicData)
			std::free(dynamicData);

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		vertexBuffer.destroy();
		indexBuffer.destroy();

		uniformBuffers.dynamic.destroy();
		uniformBuffers.view.destroy();
	}

	virtual void buildCommandBuffers() override
	{
		using namespace vks::initializers;

		auto cmdBeginI = commandBufferBeginInfo();

		auto renderPassBeginI = renderPassBeginInfo();

		VkClearValue clearVal[2];
		clearVal[0].color = defaultClearColor;
		clearVal[1].depthStencil = { 1.0f,0 };
	}
private:

	vks::Buffer vertexBuffer;
	vks::Buffer indexBuffer;
	uint32_t indexCount;

	struct 
	{
		vks::Buffer view;
		vks::Buffer dynamic;

	} uniformBuffers;

	struct {
		glm::mat4 projection;
		glm::mat4 view;
	} uboVS;

	glm::vec3 rotations[OBJECT_INSTANCES];
	glm::vec3 rotationSpeeds[OBJECT_INSTANCES];

	glm::mat4 *dynamicData = nullptr;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;

	float animationTimer = 0.0f;

	size_t dynamicAlignment = 0;
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
	/*for (size_t i = 0; i < __argc; i++) { Example::args.push_back(__argv[i]); };
	example = new Example();
	example->initVulkan();
	example->setupWindow(hInstance, WndProc);
	example->prepare();
	example->renderLoop();
	delete example;*/
	MessageBox(NULL, "sss", "tip", MB_OK);
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
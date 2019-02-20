#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>

struct Vertex
{
	float position[3];
	float color[3];
};

class Triangle : public VulkanExampleBase
{
public:
	virtual void render() override {

	}

	Triangle() : VulkanExampleBase(false)
	{
		zoom = -2.5f;
		title = "triangle";
	}
private:

	struct {
		VkDeviceMemory memory;
		VkBuffer buffer;
	} vertices;

	struct {
		VkDeviceMemory memory;
		VkBuffer buffer;
		uint32_t count;
	} indeices;

	struct {
		VkDeviceMemory memory;
		VkBuffer buffer;
		VkDescriptorBufferInfo descriptor;
	} uniformBufferVS;

	struct {
		glm::mat4x4 projectionMatrix;
		glm::mat4x4 modelMatrix;
		glm::mat4x4 viewMatrix;
	} uboVS;

	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	
	//用于协调图形队列中的操作并确保正确的命令顺序
	VkSemaphore presentSemaphore;
	VkSemaphore renderSemaphore;

	//用于检查队列操作的完成情况（例如命令缓冲区执行）
	std::vector<VkFence> waitFences;

	~Triangle()
	{
		vkDestroyPipeline(device, pipeline, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		vkDestroyBuffer(device, vertices.buffer, nullptr);
		vkFreeMemory(device, vertices.memory, nullptr);

		vkDestroyBuffer(device, indeices.buffer, nullptr);
		vkFreeMemory(device, indeices.memory, nullptr);

		vkDestroyBuffer(device, uniformBufferVS.buffer, nullptr);
		vkFreeMemory(device, uniformBufferVS.memory, nullptr);

		vkDestroySemaphore(device, presentSemaphore, nullptr);
		vkDestroySemaphore(device, renderSemaphore, nullptr);

		for (auto fence : waitFences)
		{
			vkDestroyFence(device, fence, nullptr);
		}

	}
};

#if defined(_WIN32)

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	MessageBoxA(0, VK_EXAMPLE_DATA_DIR, "Tip", MB_OK);
	return 0;
}

#else

int main()
{

	return 0;
}

#endif // 


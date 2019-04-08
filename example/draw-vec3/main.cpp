#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>
#include <VulkanBuffer.hpp>
#include <VulkanSwapChain.hpp>
#include "vector3.hpp"
#include <comm/CommTool.hpp>

class Example : public VulkanExampleBase
{
public:
	Example() : VulkanExampleBase(true)
	{

	}
	~Example()
	{
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

		indexBuffer.destroy();
		for (auto& vs : vertexBuffers)
			vs.destroy();
		uniformBuffer.destroy();

		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	}

	void render() override
	{

	}

	void buildCommandBuffers()
	{
		using namespace vks::initializers;

		VkClearValue clearVals[2];
		clearVals[0].color = defaultClearColor;
		clearVals[1].depthStencil = { 1.0f,0 };

		auto cmdBeginI = commandBufferBeginInfo();
		auto renderPassBeginI = renderPassBeginInfo();

		renderPassBeginI.clearValueCount = wws::arrLen<uint32_t>(clearVals);
		renderPassBeginI.pClearValues = clearVals;
		renderPassBeginI.renderPass = renderPass;
		renderPassBeginI.renderArea = { {0,0},{width,height} };

		for (int i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBeginI.framebuffer = frameBuffers[i];
			vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBeginI);
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginI, VK_SUBPASS_CONTENTS_INLINE);

			auto vp = viewport(width, height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &vp);
			auto scissor = rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			
			for (int j = 0; j < vertexBuffers.size(); ++j)
			{
				VkDeviceSize offset[1] = { 0 };
				vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertexBuffers[j].buffer, offset);
				vkCmdBindIndexBuffer(drawCmdBuffers[i], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(drawCmdBuffers[i], indices.size(), 1, 0, 0, 0);
			}

			vkCmdEndRenderPass(drawCmdBuffers[i]);
			vkEndCommandBuffer(drawCmdBuffers[i]);
		}
	}

	void prepareVertices()
	{
		dvs.push_back(DrawVec3(glm::vec3(1.0f, 0.0f, 0.0f)));
		indices = dvs[0].build_indices();

		for (int i = 0; i < dvs.size(); ++i)
		{
			updateVertexBuffer(i);
		}
	}

	void updateVertexBuffer(int index)
	{	
		vks::Buffer stageBuffer;
		std::vector<DrawVec3::Vertex> vertices = dvs[index].build_vertices;

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&stageBuffer, sizeof(DrawVec3::Vertex) * static_cast<VkDeviceSize>(vertices.size()), vertices.data());

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&vertexBuffers[index], sizeof(DrawVec3::Vertex) * static_cast<VkDeviceSize>(vertices.size()));

		vulkanDevice->copyBuffer(&stageBuffer, &vertexBuffers[index],queue);
	}

	void setupVertexDescription()
	{
		using namespace vks::initializers;

		vertexInputState.bindings = {
			vertexInputBindingDescription(0,sizeof(DrawVec3::Vertex),VK_VERTEX_INPUT_RATE_VERTEX)
		};

		vertexInputState.attrs = {
			vertexInputAttributeDescription(0,0,VK_FORMAT_R32G32B32_SFLOAT,0),
			vertexInputAttributeDescription(0,1,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 3)
		};

		vertexInputState.state = pipelineVertexInputStateCreateInfo();
		vertexInputState.state.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputState.bindings.size());
		vertexInputState.state.pVertexBindingDescriptions = vertexInputState.bindings.data();

		vertexInputState.state.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputState.attrs.size());
		vertexInputState.state.pVertexAttributeDescriptions = vertexInputState.attrs.data();

	}

	void setupDescriptorPool()
	{
		auto size = vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
		auto descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(1, &size, 1);
		vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorPool);
	}

	void setupDescriptorLayout()
	{
		VkDescriptorSetLayoutBinding binds[] = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT,0)
		};

		auto descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(binds, wws::arrLen<uint32_t>(binds));
		vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout);

		auto pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout);
	}

	void setupDescriptorSet()
	{
		using namespace vks::initializers;
		
		auto setAllocI = descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		vkAllocateDescriptorSets(device, &setAllocI, &descriptorSet);

		auto writeSetI = writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeSetI, 0, nullptr);
	}

private:
	VkPipeline pipeline;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	VkPipelineLayout pipelineLayout;

	struct {
		VkPipelineVertexInputStateCreateInfo state;
		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attrs;
	} vertexInputState;

	std::vector<uint32_t> indices;
	std::vector<DrawVec3> dvs;

	std::vector<vks::Buffer> vertexBuffers;
	
	vks::Buffer indexBuffer;
	vks::Buffer uniformBuffer;
	struct {
		glm::mat4 projectionMatrix;
		glm::mat4 modelMatrix;
		glm::mat4 viewMatrix;
	} uboVS;
};


#if defined(_WIN32)

Example* example;
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
	DrawVec3 dv(glm::vec3(1.0f, 1.0f, 0.0f));

	std::vector<DrawVec3::Vertex> res = dv.build_vertices();
	std::string str;
	for (auto& a : res)
	{
		char buf[100] = { 0 };
		sprintf(buf,"%f %f %f\n", a.pos.x, a.pos.y, a.pos.z);
		str += buf;
	}

	MessageBox(NULL, str.data(), "tip", MB_OK);
	return 0;
}

#elif defined(__linux__)

// Linux entry point
Example* example;
static void handleEvent(const xcb_generic_event_t* event)
{
	if (example != NULL)
	{
		example->handleEvent(event);
	}
}
int main(const int argc, const char* argv[])
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
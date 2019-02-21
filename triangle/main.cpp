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

	uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags property)
	{
		for (int i = 0; i < deviceMemoryProperties.memoryTypeCount; ++i)
		{
			if ((typeBits & 1) == 1)
			{
				if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & property) == property)
				{
					return i;
				}
			}
			typeBits >>= 1;
		}
		throw "Could not find a suitable memory type";
	}

	void prepareSynchronizationPrimitives()
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.pNext = nullptr;

		vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &presentSemaphore);
		vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderSemaphore);

		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		waitFences.resize(drawCmdBuffers.size());
		for (int i = 0; i < waitFences.size(); ++i)
		{
			vkCreateFence(device, &fenceCreateInfo, nullptr, &waitFences[i]);
		}
	}

	VkCommandBuffer getCommandBuffer(bool begin)
	{
		VkCommandBuffer commandBuffer;

		VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = cmdPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = 1;
		
		vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);

		if (begin) {
			VkCommandBufferBeginInfo cmdBeginInfo = vks::initializers::commandBufferBeginInfo();
			vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo);
		}
		return commandBuffer;
	}

	void flushCommandBuffer(VkCommandBuffer cmdBuffer)
	{
		assert(cmdBuffer != VK_NULL_HANDLE);

		vkEndCommandBuffer(cmdBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pCommandBuffers = &cmdBuffer;
		submitInfo.commandBufferCount = 1;

		VkFence fence;
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = 0;

		vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);

		vkQueueSubmit(queue, 1, &submitInfo, fence);
		vkWaitForFences(device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT);

		vkDestroyFence(device, fence, nullptr);
		vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBeginInfo = {};
		cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		VkClearValue clearValue[2];
		clearValue[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };
		clearValue[1].depthStencil = { 1.0f,0 };

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pClearValues = clearValue;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.renderPass = renderPass;

		for (int i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBeginInfo);

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport;
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)width;
			viewport.height = (float)height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor;
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;

			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertices.buffer, nullptr);

			vkCmdBindIndexBuffer(drawCmdBuffers[i], indeices.buffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdDrawIndexed(drawCmdBuffers[i], indeices.count, 1, 0, 0, 0);

			vkCmdEndRenderPass(drawCmdBuffers[i]);
			vkEndCommandBuffer(drawCmdBuffers[i]);
		}
	}

	void draw()
	{
		swapChain.acquireNextImage(presentSemaphore, &currentBuffer);

		vkWaitForFences(device, 1, &waitFences[currentBuffer], VK_TRUE, DEFAULT_FENCE_TIMEOUT);
		vkResetFences(device, 1, &waitFences[currentBuffer]);

		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pWaitDstStageMask = &waitStageMask;
		submitInfo.pWaitSemaphores = &presentSemaphore;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderSemaphore;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		submitInfo.commandBufferCount = 1;

		vkQueueSubmit(queue, 1, &submitInfo, waitFences[currentBuffer]);
		swapChain.queuePresent(queue, currentBuffer, renderSemaphore);
	}

	void prepareVertices(bool useStageBuffers)
	{
		std::vector<Vertex> vertexBuffer =
		{
			{ {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
			{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
			{ {  0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
		};

		uint32_t vertexBufferSize = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(Vertex);

		std::vector<uint32_t> indexBuffer = { 0,1,2 };
		indeices.count = static_cast<uint32_t>(indexBuffer.size());
		uint32_t indexBufferSize = static_cast<uint32_t>(indexBuffer.size()) * sizeof(uint32_t);

	}
};
#if defined(_WIN32)

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	char arr[4] = { 'a','b','c','d' };
	char str[200] = { 0 };
	sprintf(str, "%s %c", VK_EXAMPLE_DATA_DIR, *((char *)&arr));
	MessageBoxA(0, str, "Tip", MB_OK);
	return 0;
}

#else

int main()
{

	return 0;
}

#endif // 


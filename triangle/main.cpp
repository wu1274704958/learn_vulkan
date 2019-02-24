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
	Triangle() : VulkanExampleBase(true)
	{
		zoom = -2.5f;
		title = "triangle";
	}

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

		VK_CHECK_RESULT( vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &presentSemaphore) );
		VK_CHECK_RESULT( vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderSemaphore) );

		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		waitFences.resize(drawCmdBuffers.size());
		for (int i = 0; i < waitFences.size(); ++i)
		{
			VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &waitFences[i]) );
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
		
		VK_CHECK_RESULT( vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer) );

		if (begin) {
			VkCommandBufferBeginInfo cmdBeginInfo = vks::initializers::commandBufferBeginInfo();
			VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));
		}
		return commandBuffer;
	}

	void flushCommandBuffer(VkCommandBuffer cmdBuffer)
	{
		assert(cmdBuffer != VK_NULL_HANDLE);

		VK_CHECK_RESULT( vkEndCommandBuffer(cmdBuffer) );

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pCommandBuffers = &cmdBuffer;
		submitInfo.commandBufferCount = 1;

		VkFence fence;
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = 0;

		VK_CHECK_RESULT( vkCreateFence(device, &fenceCreateInfo, nullptr, &fence) );

		VK_CHECK_RESULT( vkQueueSubmit(queue, 1, &submitInfo, fence) );
		VK_CHECK_RESULT( vkWaitForFences(device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT) );

		vkDestroyFence(device, fence, nullptr);
		vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer) ;
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

			VK_CHECK_RESULT( vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBeginInfo));

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
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertices.buffer, offsets);

			vkCmdBindIndexBuffer(drawCmdBuffers[i], indeices.buffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdDrawIndexed(drawCmdBuffers[i], indeices.count, 1, 0, 0, 1);

			vkCmdEndRenderPass(drawCmdBuffers[i]);
			VK_CHECK_RESULT( vkEndCommandBuffer(drawCmdBuffers[i]) );
		}
	}

	void draw()
	{
		VK_CHECK_RESULT( swapChain.acquireNextImage(presentSemaphore, &currentBuffer) );
		VK_CHECK_RESULT(vkWaitForFences(device, 1, &waitFences[currentBuffer], VK_TRUE, UINT64_MAX) );
		VK_CHECK_RESULT(vkResetFences(device, 1, &waitFences[currentBuffer]) );

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

		VK_CHECK_RESULT( vkQueueSubmit(queue, 1, &submitInfo, waitFences[currentBuffer]) );
		VK_CHECK_RESULT( swapChain.queuePresent(queue, currentBuffer, renderSemaphore) );
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

		VkMemoryAllocateInfo memAllocateInfo = {};
		memAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

		VkMemoryRequirements memoryRequirement;

		void *data = nullptr;
		if (useStageBuffers)
		{
			struct StagingBuffer
			{
				VkBuffer buffer;
				VkDeviceMemory memory;
			};

			struct {
				StagingBuffer vertices;
				StagingBuffer indices;
			} stagingBuffers;

			VkBufferCreateInfo bufferCreateInfo = {};
			bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferCreateInfo.size = vertexBufferSize;

			bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			VK_CHECK_RESULT( vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffers.vertices.buffer) );

			vkGetBufferMemoryRequirements(device, stagingBuffers.vertices.buffer, &memoryRequirement);
			memAllocateInfo.allocationSize = memoryRequirement.size;

			memAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryRequirement.memoryTypeBits,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			VK_CHECK_RESULT( vkAllocateMemory(device, &memAllocateInfo, nullptr, &stagingBuffers.vertices.memory) );

			VK_CHECK_RESULT( vkMapMemory(device, stagingBuffers.vertices.memory, 0, vertexBufferSize, 0, &data) );
			memcpy(data, vertexBuffer.data(), vertexBufferSize);
			vkUnmapMemory(device, stagingBuffers.vertices.memory);
			VK_CHECK_RESULT( vkBindBufferMemory(device,stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0) );

			//create device local memory.
			bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			VK_CHECK_RESULT( vkCreateBuffer(device, &bufferCreateInfo, nullptr, &vertices.buffer) );

			vkGetBufferMemoryRequirements(device, vertices.buffer, &memoryRequirement);
			memAllocateInfo.allocationSize = memoryRequirement.size;
			memAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryRequirement.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			VK_CHECK_RESULT( vkAllocateMemory(device, &memAllocateInfo, nullptr, &vertices.memory));
			VK_CHECK_RESULT( vkBindBufferMemory(device, vertices.buffer, vertices.memory,0) );

			//indices buffer
			bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferCreateInfo.size = indexBufferSize;
			
			VK_CHECK_RESULT( vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffers.indices.buffer) );

			vkGetBufferMemoryRequirements(device, stagingBuffers.indices.buffer, &memoryRequirement);

			memAllocateInfo.allocationSize = memoryRequirement.size;
			memAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryRequirement.memoryTypeBits,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			VK_CHECK_RESULT( vkAllocateMemory(device, &memAllocateInfo, nullptr, &stagingBuffers.indices.memory) );
			
			VK_CHECK_RESULT( vkMapMemory(device, stagingBuffers.indices.memory, 0, indexBufferSize, 0, &data));
			memcpy(data, indexBuffer.data(), indexBufferSize);
			vkUnmapMemory(device, stagingBuffers.indices.memory);
			VK_CHECK_RESULT( vkBindBufferMemory(device, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0) );

			//create device local memory.
			bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			bufferCreateInfo.size = indexBufferSize;
			VK_CHECK_RESULT( vkCreateBuffer(device, &bufferCreateInfo, nullptr, &indeices.buffer) );

			vkGetBufferMemoryRequirements(device, indeices.buffer, &memoryRequirement);
			memAllocateInfo.allocationSize = memoryRequirement.size;
			memAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryRequirement.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT( vkAllocateMemory(device, &memAllocateInfo, nullptr, &indeices.memory) );
			VK_CHECK_RESULT( vkBindBufferMemory(device, indeices.buffer, indeices.memory, 0) );

			VkCommandBuffer copycmd = getCommandBuffer(true);

			VkBufferCopy copyRegion = {};
			copyRegion.size = vertexBufferSize;
			vkCmdCopyBuffer(copycmd, stagingBuffers.vertices.buffer, vertices.buffer, 1, &copyRegion);

			copyRegion.size = indexBufferSize;
			vkCmdCopyBuffer(copycmd, stagingBuffers.indices.buffer, indeices.buffer, 1, &copyRegion);

			flushCommandBuffer(copycmd);
		}
		else {
			VkBufferCreateInfo bufferCreateInfo = {};
			bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

			bufferCreateInfo.size = vertexBufferSize;
			bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			
			VK_CHECK_RESULT( vkCreateBuffer(device, &bufferCreateInfo, nullptr, &vertices.buffer) );
			vkGetBufferMemoryRequirements(device, vertices.buffer, &memoryRequirement);

			memAllocateInfo.allocationSize = memoryRequirement.size;
			memAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryRequirement.memoryTypeBits,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			VK_CHECK_RESULT( vkAllocateMemory(device, &memAllocateInfo, nullptr, &vertices.memory) );
			VK_CHECK_RESULT( vkMapMemory(device, vertices.memory, 0, vertexBufferSize, 0, &data) );
			memcpy(data, vertexBuffer.data(), vertexBufferSize);
			vkUnmapMemory(device, vertices.memory);
			VK_CHECK_RESULT( vkBindBufferMemory(device, vertices.buffer, vertices.memory, 0) );

			bufferCreateInfo.size = indexBufferSize;
			bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

			VK_CHECK_RESULT( vkCreateBuffer(device, &bufferCreateInfo, nullptr, &indeices.buffer) );
			vkGetBufferMemoryRequirements(device, indeices.buffer, &memoryRequirement);
			memAllocateInfo.allocationSize = memoryRequirement.size;
			memAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryRequirement.memoryTypeBits,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			VK_CHECK_RESULT( vkAllocateMemory(device, &memAllocateInfo, nullptr, &indeices.memory) );
			VK_CHECK_RESULT( vkMapMemory(device, indeices.memory, 0, indexBufferSize, 0, &data) );
			memcpy(data, indexBuffer.data(), indexBufferSize);
			vkUnmapMemory(device, indeices.memory);
			VK_CHECK_RESULT( vkBindBufferMemory(device, indeices.buffer, indeices.memory, 0) );
		}
	}

	void setupDescriptorPool()
	{
		VkDescriptorPoolSize typeCount[1];
		typeCount[0].descriptorCount = 1;
		typeCount[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.poolSizeCount = 1;
		descriptorPoolCreateInfo.pPoolSizes = typeCount;
		descriptorPoolCreateInfo.maxSets = 1;

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
		descriptorSetLayoutBinding.descriptorCount = 1;
		descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		descriptorSetLayoutBinding.pImmutableSamplers = VK_NULL_HANDLE;

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.bindingCount = 1;
		descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout));

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
		
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo descriptorAllocateInfo = {};
		descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorAllocateInfo.descriptorSetCount = 1;
		descriptorAllocateInfo.pSetLayouts = &descriptorSetLayout;
		descriptorAllocateInfo.descriptorPool = descriptorPool;

		VK_CHECK_RESULT( vkAllocateDescriptorSets(device, &descriptorAllocateInfo, &descriptorSet) );
		//uniform binding 0
		VkWriteDescriptorSet writeDescriptSet = {};
		writeDescriptSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptSet.dstBinding = 0;
		writeDescriptSet.dstSet = descriptorSet;
		writeDescriptSet.descriptorCount = 1;
		writeDescriptSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptSet.pBufferInfo = &uniformBufferVS.descriptor;

		vkUpdateDescriptorSets(device, 1, &writeDescriptSet, 0, nullptr);
	}

	virtual void setupDepthStencil() override
	{
		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = depthFormat;
		imageCreateInfo.extent = { width,height,1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &depthStencil.image));

		VkMemoryAllocateInfo memAllocateInfo = {};
		memAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

		VkMemoryRequirements memRequirement;
		vkGetImageMemoryRequirements(device, depthStencil.image, &memRequirement);
		
		memAllocateInfo.allocationSize = memRequirement.size;
		memAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memRequirement.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocateInfo, nullptr, &depthStencil.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.image = depthStencil.image;
		imageViewCreateInfo.format = depthFormat;
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &depthStencil.view));
	}

	virtual void setupFrameBuffer() override
	{
		frameBuffers.resize(swapChain.imageCount);
		for (int i = 0; i < frameBuffers.size(); i++)
		{
			VkImageView attachments[2];
			attachments[0] = swapChain.buffers[i].view;
			attachments[1] = depthStencil.view;

			VkFramebufferCreateInfo frameBufferCreateInfo = {};
			frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferCreateInfo.width = width;
			frameBufferCreateInfo.height = height;
			frameBufferCreateInfo.attachmentCount = 2;
			frameBufferCreateInfo.pAttachments = attachments;
			frameBufferCreateInfo.layers = 1;
			frameBufferCreateInfo.renderPass = renderPass;
			
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
		}
	}

	virtual void setupRenderPass() override
	{
		std::array<VkAttachmentDescription, 2> attachments = {};
		//color attachment
		attachments[0].format = swapChain.colorFormat;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		//depth attachment
		attachments[1].format = depthFormat;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;
		subpassDescription.pDepthStencilAttachment = &depthReference;
		subpassDescription.inputAttachmentCount = 0;
		subpassDescription.pInputAttachments = nullptr;
		subpassDescription.preserveAttachmentCount = 0;
		subpassDescription.pPreserveAttachments = nullptr;
		subpassDescription.pResolveAttachments = nullptr;

		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0] = {};
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1] = {};
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassCreateInfo = {};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderPassCreateInfo.pDependencies = dependencies.data();
		renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassCreateInfo.pAttachments = attachments.data();
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpassDescription;

		VK_CHECK_RESULT( vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass) );
	}

	VkShaderModule loadSPIRVShader(const std::string& path)
	{
		size_t shaderSize;
		char *shaderCode = nullptr;

		std::ifstream fs(path, std::ios::in | std::ios::binary | std::ios::ate);
		if (fs.is_open())
		{
			shaderSize = fs.tellg();
			fs.seekg(0, std::ios::beg);
			shaderCode = new char[shaderSize];
			
			fs.read(shaderCode, shaderSize);
			fs.close();
			assert(shaderSize > 0);
		}

		if (shaderCode) {
			VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
			shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			shaderModuleCreateInfo.codeSize = shaderSize;
			shaderModuleCreateInfo.pCode = reinterpret_cast<uint32_t*>(shaderCode);

			VkShaderModule shaderModule;

			VK_CHECK_RESULT(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule));
			delete[] shaderCode;
			return shaderModule;
		}
		else
		{
			std::cerr << "Error: Could not open shader file \"" << path << "\"" << std::endl;
			return VK_NULL_HANDLE;
		}
	}

	void preparePipelines()
	{
		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.layout = pipelineLayout;

		VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {};
		pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
		pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
		pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

		VkPipelineColorBlendAttachmentState blendAttachmentState = {};
		blendAttachmentState.blendEnable = VK_FALSE;
		blendAttachmentState.colorWriteMask = 0xf;

		VkPipelineColorBlendStateCreateInfo blendStateCreateInfo = {};
		blendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blendStateCreateInfo.attachmentCount = 1;
		blendStateCreateInfo.pAttachments = &blendAttachmentState;

		VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
		viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateCreateInfo.viewportCount = 1;
		viewportStateCreateInfo.scissorCount = 1;

		VkDynamicState dynamicState[2] = { VK_DYNAMIC_STATE_VIEWPORT , VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
		dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCreateInfo.dynamicStateCount = 2;
		dynamicStateCreateInfo.pDynamicStates = dynamicState;

		VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
		depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
		depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
		depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
		depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilStateCreateInfo.front.failOp = VK_STENCIL_OP_KEEP;
		depthStencilStateCreateInfo.front.passOp = VK_STENCIL_OP_KEEP;
		depthStencilStateCreateInfo.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depthStencilStateCreateInfo.back = depthStencilStateCreateInfo.front;

		VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
		multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleStateCreateInfo.pSampleMask = VK_NULL_HANDLE;

		VkVertexInputBindingDescription vertexInputBindDescription = {};
		vertexInputBindDescription.binding = 0;
		vertexInputBindDescription.stride = sizeof(Vertex);
		vertexInputBindDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription  vertexInputAttributeDescription[2];

		vertexInputAttributeDescription[0].binding = 0;
		vertexInputAttributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexInputAttributeDescription[0].location = 0;
		vertexInputAttributeDescription[0].offset = offsetof(Vertex, position);

		vertexInputAttributeDescription[1].binding = 0;
		vertexInputAttributeDescription[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexInputAttributeDescription[1].location = 1;
		vertexInputAttributeDescription[1].offset = offsetof(Vertex, color);

		VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
		vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindDescription;
		vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 2;
		vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescription;

		VkPipelineShaderStageCreateInfo shaderStageCreateInfos[2];

		shaderStageCreateInfos[0] = {};
		shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfos[0].pName = "main";
		shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStageCreateInfos[0].module = loadSPIRVShader(getAssetPath() + "shaders/triangle/triangle.vert.spv");

		shaderStageCreateInfos[1] = {};
		shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfos[1].pName = "main";
		shaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStageCreateInfos[1].module = loadSPIRVShader(getAssetPath() + "shaders/triangle/triangle.frag.spv");

		assert(shaderStageCreateInfos[0].module != VK_NULL_HANDLE);
		assert(shaderStageCreateInfos[1].module != VK_NULL_HANDLE);

		pipelineCreateInfo.stageCount = 2;
		pipelineCreateInfo.pStages = shaderStageCreateInfos;
		pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
		pipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
		pipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
		pipelineCreateInfo.pColorBlendState = &blendStateCreateInfo;
		pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
		pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
		pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));

		vkDestroyShaderModule(device, shaderStageCreateInfos[0].module, nullptr);
		vkDestroyShaderModule(device, shaderStageCreateInfos[1].module, nullptr);
	}

	void prepareUniformBuffers()
	{
		VkMemoryRequirements memRequirement;

		VkBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

		VkMemoryAllocateInfo memoryAllocateInfo = {};
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

		bufferCreateInfo.size = sizeof(uboVS);
		bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &uniformBufferVS.buffer));

		vkGetBufferMemoryRequirements(device, uniformBufferVS.buffer, &memRequirement);

		memoryAllocateInfo.allocationSize = memRequirement.size;
		memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memRequirement.memoryTypeBits, 
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &uniformBufferVS.memory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, uniformBufferVS.buffer, uniformBufferVS.memory,0));

		uniformBufferVS.descriptor.buffer = uniformBufferVS.buffer;
		uniformBufferVS.descriptor.offset = 0;
		uniformBufferVS.descriptor.range = sizeof(uboVS);

		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		uboVS.projectionMatrix = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f);

		uboVS.viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, zoom));
		
		glm::mat4 model(1.0f);

		model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		uboVS.modelMatrix = model;
		void* data = nullptr;

		VK_CHECK_RESULT(vkMapMemory(device, uniformBufferVS.memory, 0, sizeof(uboVS), 0, &data));

		memcpy(data, &uboVS, sizeof(uboVS));

		vkUnmapMemory(device, uniformBufferVS.memory);
	}

	virtual void render() override {
		if (!prepared)
			return;
		draw();
	}

	virtual void viewChanged() override {
		updateUniformBuffers();
	}
public:
	virtual void prepare() override
	{
		VulkanExampleBase::prepare();
		prepareSynchronizationPrimitives();
		prepareVertices(true);
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared = true;
	}
};
#if defined(_WIN32)

Triangle *triangle;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (triangle != NULL)
	{
		triangle->handleMessages(hWnd, uMsg, wParam, lParam);
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	for (size_t i = 0; i < __argc; i++) { Triangle::args.push_back(__argv[i]); };
	triangle = new Triangle();
	triangle->initVulkan();
	triangle->setupWindow(hInstance, WndProc);
	triangle->prepare(); 
	triangle->renderLoop();
	delete triangle;
	return 0;
}

#else

int main()
{

	return 0;
}

#endif // 


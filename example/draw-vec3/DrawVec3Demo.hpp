#pragma once

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
#include <comm/dbg.hpp>

class DrawVec3Demo : public VulkanExampleBase
{
public:
	DrawVec3Demo() : VulkanExampleBase(true)
	{
		zoom = -10.5f;
		title = "draw vec";
	}
	~DrawVec3Demo()
	{
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

		indexBuffer.destroy();
		for (auto& vs : vertexBuffers)
			vs.destroy();
		uniformBuffer.destroy();

		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	}
	float rotatex = 0.0f;
	void render() override
	{
		if (!prepared)
			return;
		draw();
		update_vec();
	}

	void buildCommandBuffers() override
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
			vkCmdSetLineWidth(drawCmdBuffers[i], 2.0);

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

			for (int j = 0; j < vertexBuffers.size(); ++j)
			{
				vkCmdSetLineWidth(drawCmdBuffers[i], dvs[j].get_line_width());
				VkDeviceSize offset[1] = { 0 };
				vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertexBuffers[j].buffer, offset);
				vkCmdBindIndexBuffer(drawCmdBuffers[i], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(drawCmdBuffers[i], indices.size(), 1, 0, 0, 0);
			}

			vkCmdEndRenderPass(drawCmdBuffers[i]);
			vkEndCommandBuffer(drawCmdBuffers[i]);
		}
	}

	virtual void init_vec()
	{

	}

	virtual void update_vec()
	{

	}

	void prepareVertices()
	{
		auto x = DrawVec3(glm::vec3(1.0f, 0.0f, 0.0f));
		x.set_color(x.get_vec());
		
		auto z = DrawVec3(glm::vec3(0.0f, 0.0f, 1.0f));
		z.set_color(z.get_vec());

		auto y = DrawVec3(glm::vec3(0.0f, 1.0f, 0.0f));
		y.set_color(y.get_vec());

		dvs.push_back(x);
		dvs.push_back(y);
		dvs.push_back(z);

		init_vec();
		
		indices = dvs[0].build_indices();
		setupIndexBuffer();
		setupVertexBuffer(dbg(dvs[0].vertices_count() * sizeof(DrawVec3::Vertex)));
		for (int i = 0; i < dvs.size(); ++i)
		{
			updateVertexBuffer(i);
		}
	}

	void setupIndexBuffer()
	{
		vks::Buffer stageBuffer;

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&stageBuffer, static_cast<VkDeviceSize>(sizeof(uint32_t)) * indices.size(), indices.data());

		stageBuffer.flush();

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&indexBuffer, static_cast<VkDeviceSize>(sizeof(uint32_t)) * indices.size());


		vulkanDevice->copyBuffer(&stageBuffer, &indexBuffer, queue);

		stageBuffer.destroy();
	}

	void setupVertexBuffer(size_t buffer_size)
	{
		vertexBuffers.resize(dvs.size());
		for (int i = 0; i < dvs.size(); ++i)
		{
			vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&vertexBuffers[i], static_cast<VkDeviceSize>(buffer_size));
		}
	}

	void updateVertexBuffer(int index)
	{
		vks::Buffer stageBuffer;
		std::vector<DrawVec3::Vertex> vertices = dvs[index].build_vertices();

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&stageBuffer, sizeof(DrawVec3::Vertex) * static_cast<VkDeviceSize>(vertices.size()), vertices.data());
		stageBuffer.flush();
		vulkanDevice->copyBuffer(&stageBuffer, &vertexBuffers[index], queue);
		stageBuffer.destroy();
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

	void preparePipelines()
	{
		using namespace vks::initializers;

		auto inputAssemblySCI = pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_LINE_LIST, 0, VK_FALSE);
		auto rasterizationSCI = pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
		auto colorBlendAttachment = pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		auto colorBlendSCI = pipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
		auto depthStencilSCI = pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		auto vpSCI = pipelineViewportStateCreateInfo(1, 1);
		auto multsampleSCI = pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

		VkDynamicState dynamicSs[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		auto dynamicSCI = pipelineDynamicStateCreateInfo(dynamicSs, wws::arrLen<uint32_t>(dynamicSs));
		VkPipelineShaderStageCreateInfo shaderStages[] = {
			loadShader(getAssetPath() + "shaders/triangle/triangle.vert.spv",VK_SHADER_STAGE_VERTEX_BIT),
			loadShader(getAssetPath() + "shaders/triangle/triangle.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		auto pipelineCI = pipelineCreateInfo(pipelineLayout, renderPass);
		pipelineCI.stageCount = wws::arrLen<uint32_t>(shaderStages);
		pipelineCI.pStages = shaderStages;

		pipelineCI.pColorBlendState = &colorBlendSCI;
		pipelineCI.pDepthStencilState = &depthStencilSCI;
		pipelineCI.pDynamicState = &dynamicSCI;
		pipelineCI.pInputAssemblyState = &inputAssemblySCI;
		pipelineCI.pMultisampleState = &multsampleSCI;
		pipelineCI.pRasterizationState = &rasterizationSCI;
		pipelineCI.pVertexInputState = &vertexInputState.state;
		pipelineCI.pViewportState = &vpSCI;

		vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline);
	}

	void prepareUniformBuffer()
	{
		vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffer, sizeof(uboVS));

		updateUniformBuffer();
	}

	void updateUniformBuffer()
	{
		VK_CHECK_RESULT(uniformBuffer.map());

		uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 512.0f);
		uboVS.view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, zoom));
		glm::mat4 model = glm::mat4(1.0f);

		model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		uboVS.model = model;

		memcpy(uniformBuffer.mapped, &uboVS, sizeof(uboVS));

		uniformBuffer.unmap();
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		submitInfo.commandBufferCount = 1;
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();

		prepareVertices();
		setupVertexDescription();
		prepareUniformBuffer();
		setupDescriptorPool();
		setupDescriptorLayout();
		setupDescriptorSet();
		preparePipelines();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void viewChanged() override
	{
		updateUniformBuffer();
	}

protected:

	virtual void getEnabledFeatures() override
	{
		if (deviceFeatures.fillModeNonSolid)
		{
			enabledFeatures.fillModeNonSolid = VK_TRUE;
			if (deviceFeatures.wideLines)
			{
				enabledFeatures.wideLines = VK_TRUE;
			}
		}
	}

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
		glm::mat4 projection;
		glm::mat4 model;
		glm::mat4 view;
	} uboVS;
};
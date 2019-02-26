#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>
#include <VulkanModel.hpp>
#include <VulkanBuffer.hpp>

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION true

class Example : public VulkanExampleBase {
	vks::VertexLayout vertexLayout = vks::VertexLayout({
		vks::VERTEX_COMPONENT_POSITION,
		vks::VERTEX_COMPONENT_NORMAL,
		vks::VERTEX_COMPONENT_UV,
		vks::VERTEX_COMPONENT_COLOR
	});

	struct 
	{
		vks::Model cube;
	} models;

	vks::Buffer uniformBuffer;

	struct {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 lightPos = glm::vec4(0.0f, 2.0f, 1.0f, 0.0f);
	} uboVS;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;

	struct {
		VkPipeline phong;
		VkPipeline wireframe;
		VkPipeline toon;
	} pipelines;
public:
	Example() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		zoom = -10.5f;
		rotation = glm::vec3(-25.0f, 15.0f, 0.0f);
		title = "Pipeline state object";
		settings.overlay = true;
//		width = 400;
//		height = 130;
	}

	~Example()
	{
		vkDestroyPipeline(device, pipelines.phong, nullptr);
		if (deviceFeatures.fillModeNonSolid)
		{
			vkDestroyPipeline(device, pipelines.wireframe, nullptr);
		}
		vkDestroyPipeline(device, pipelines.toon, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		models.cube.destroy();
		uniformBuffer.destroy();
	}
private:
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

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBeginInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearVal[2];

		clearVal[0].color = { defaultClearColor };
		clearVal[1].depthStencil = { 1.0f,0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearVal;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea = { {0,0} , {width,height} };

		float width_1_3 = static_cast<float>(width) / 3.0f;

		for (int i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT( vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBeginInfo) );

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

			VkDeviceSize offset[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &models.cube.vertices.buffer, offset);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], models.cube.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

			viewport.width = width_1_3;
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.phong);
			vkCmdDrawIndexed(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);

			viewport.x = width_1_3;
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.toon);
			if (deviceFeatures.wideLines)
				vkCmdSetLineWidth(drawCmdBuffers[i], 2.0f);
			vkCmdDrawIndexed(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);

			if (deviceFeatures.fillModeNonSolid)
			{
				viewport.x = width_1_3 * 2.0f;
				vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.wireframe);
				vkCmdDrawIndexed(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);
			}

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);
			VK_CHECK_RESULT( vkEndCommandBuffer(drawCmdBuffers[i]) ); 
		}
	}

	void loadAssets()
	{
		models.cube.loadFromFile(getAssetPath() + "models/treasure_smooth.dae", vertexLayout, 1.0f, vulkanDevice, queue);
	}

	void setupDescriptorPool()
	{
		std::vector<VkDescriptorPoolSize> poolSize = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1) };

		VkDescriptorPoolCreateInfo poolCreateInfo = vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSize.size()), poolSize.data(), 2);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT,0)
		};

		VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);

		VK_CHECK_RESULT(  vkCreateDescriptorSetLayout( device, &setLayoutCreateInfo, nullptr, &descriptorSetLayout) );

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout);

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	}

	void setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor);
		vkUpdateDescriptorSets(device,1,&writeDescriptorSet,0, nullptr);
	}

	void preparePipelines()
	{
		using namespace vks::initializers;
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo = pipelineInputAssemblyStateCreateInfo(
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			0,
			VK_FALSE
		);

		VkPipelineRasterizationStateCreateInfo rasterizationStateInfo = pipelineRasterizationStateCreateInfo(
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_CLOCKWISE
		);

		VkPipelineColorBlendAttachmentState blendAttachmentState = pipelineColorBlendAttachmentState(
			0xf,
			VK_FALSE
		);

		VkPipelineColorBlendStateCreateInfo blendStateInfo = pipelineColorBlendStateCreateInfo(
			1, &blendAttachmentState
		);

		VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo = pipelineDepthStencilStateCreateInfo(
			VK_TRUE, VK_TRUE,
			VK_COMPARE_OP_LESS_OR_EQUAL
		);

		VkPipelineViewportStateCreateInfo viewPortStateInfo = pipelineViewportStateCreateInfo(
			1, 1
		);

		VkPipelineMultisampleStateCreateInfo multisampleStateInfo = pipelineMultisampleStateCreateInfo(
			VK_SAMPLE_COUNT_1_BIT
		);

		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo dynamicStateInfo = pipelineDynamicStateCreateInfo(
			dynamicStates, 3
		);

		VkGraphicsPipelineCreateInfo pipelineInfo = pipelineCreateInfo(pipelineLayout, renderPass);

		std::array<VkPipelineShaderStageCreateInfo,2> shaderStageInfos;

		pipelineInfo.pInputAssemblyState = &inputAssemblyStateInfo;
		pipelineInfo.pRasterizationState = &rasterizationStateInfo;
		pipelineInfo.pColorBlendState = &blendStateInfo;
		pipelineInfo.pMultisampleState = &multisampleStateInfo;
		pipelineInfo.pViewportState = &viewPortStateInfo;
		pipelineInfo.pDepthStencilState = &depthStencilStateInfo;
		pipelineInfo.pDynamicState = &dynamicStateInfo;
		pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfos.size());
		pipelineInfo.pStages = shaderStageInfos.data();


		std::vector<VkVertexInputBindingDescription> vertexInputBindingDesc = {
			vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID,vertexLayout.stride(),VK_VERTEX_INPUT_RATE_VERTEX)
		};

		std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDesc = {
			vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID,0,VK_FORMAT_R32G32B32_SFLOAT,0),
			vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID,1,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 3),
			vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID,2,VK_FORMAT_R32G32_SFLOAT,sizeof(float) * 6),
			vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID,3,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 8)
		};

		VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = pipelineVertexInputStateCreateInfo();
		vertexInputStateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindingDesc.size());
		vertexInputStateInfo.pVertexBindingDescriptions = vertexInputBindingDesc.data();
		vertexInputStateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributeDesc.size());
		vertexInputStateInfo.pVertexAttributeDescriptions = vertexInputAttributeDesc.data();

		pipelineInfo.pVertexInputState = &vertexInputStateInfo;

		// We are using this pipeline as the base for the other pipelines (derivatives)
		// Pipeline derivatives can be used for pipelines that share most of their state
		// Depending on the implementation this may result in better performance for pipeline 
		// switchting and faster creation time
		pipelineInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;

		shaderStageInfos[0] = loadShader(getAssetPath() + "shaders/pipelines/phong.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStageInfos[1] = loadShader(getAssetPath() + "shaders/pipelines/phong.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.phong));
		// All pipelines created after the base pipeline will be derivatives
		pipelineInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
		// Base pipeline will be our first created pipeline
		pipelineInfo.basePipelineHandle = pipelines.phong;
		// It's only allowed to either use a handle or index for the base pipeline
		// As we use the handle, we must set the index to -1 (see section 9.5 of the specification)
		pipelineInfo.basePipelineIndex = -1;

		//toon pipeline
		shaderStageInfos[0] = loadShader(getAssetPath() + "shaders/pipelines/toon.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStageInfos[1] = loadShader(getAssetPath() + "shaders/pipelines/toon.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.toon));
		// Pipeline for wire frame rendering
		// Non solid rendering is not a mandatory Vulkan feature
		if (deviceFeatures.fillModeNonSolid)
		{
			rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_LINE;
			shaderStageInfos[0] = loadShader(getAssetPath() + "shaders/pipelines/wireframe.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			shaderStageInfos[1] = loadShader(getAssetPath() + "shaders/pipelines/wireframe.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.wireframe));
		}
	}

	void prepareUniformBuffers()
	{
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&uniformBuffer, sizeof(uboVS)
		));

		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		uboVS.projection = glm::perspective(glm::radians(60.0f), (float)(width / 3.0f) / (float)height, 0.1f, 256.0f);

		glm::mat4 viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, zoom));

		auto model = viewMatrix * glm::translate(glm::mat4(1.0f), cameraPos);
		model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		uboVS.modelView = model;
		VK_CHECK_RESULT(uniformBuffer.map());
		memcpy(uniformBuffer.mapped, &uboVS, sizeof(uboVS));
		uniformBuffer.unmap();
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}
public:
	virtual void prepare() override
	{
		VulkanExampleBase::prepare();
		loadAssets();
		prepareUniformBuffers(); 
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render() override
	{
		if (!prepared)
			return;
		draw();
	}

	virtual void viewChanged() override
	{
		updateUniformBuffers();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
	{
		if (!deviceFeatures.fillModeNonSolid)
		{
			if (overlay->header("Info"))
			{
				overlay->text("Non solid fill modes not supported!");
			}
		}
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
	for (size_t i = 0; i < __argc; i++) { Example::args.push_back(__argv[i]); };
	example = new Example();
	example->initVulkan();
	example->setupWindow(hInstance, WndProc);
	example->prepare();
	example->renderLoop();
	delete example;
	return 0;
}

#else

int main()
{
	return 0;
}

#endif // 
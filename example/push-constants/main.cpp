#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>
#include <VulkanBuffer.hpp>
#include <VulkanDevice.hpp>
#include <comm/CommTool.hpp>
#include <VulkanModel.hpp>


class Example : public VulkanExampleBase {
public:
	virtual void render() override {
		if (!prepared)
			return;
		draw();
		if (!paused)
		{
			rebuildCommandBuffers();
		}
	}
	Example() : VulkanExampleBase(true)
	{
		zoom = -30.0f;
		zoomSpeed = 2.5f;
		rotationSpeed = 0.5f;
		timerSpeed *= 0.5f;
		title = "push-constants";
		rotation = { -32.5, 45.0, 0.0 };
		settings.overlay = true;
	}
	~Example()
	{
		vkDestroyPipeline(device, pipelines.solid, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		uniformBuffer.destroy();
		models.sence.destroy();
	}

	void rebuildCommandBuffers()
	{
		if (!checkCommandBuffers())
		{
			destroyCommandBuffers();
			createCommandBuffers();
		}
		buildCommandBuffers();
	}

	virtual void buildCommandBuffers() override {
		using namespace vks::initializers;

		VkClearValue clearVal[2];
		clearVal[0].color = defaultClearColor;
		clearVal[1].depthStencil = { 1.0f,0 };

		auto cmdBeginI = commandBufferBeginInfo();
		auto renderPassBeginI = renderPassBeginInfo();
		renderPassBeginI.clearValueCount = wws::arr_len_v<decltype(clearVal)>;
		renderPassBeginI.pClearValues = clearVal;
		renderPassBeginI.renderPass = renderPass;
		renderPassBeginI.renderArea = { {0,0},{width,height} };

		for (int i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBeginI.framebuffer = frameBuffers[i];
			VK_CHECK_RESULT( vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBeginI));
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginI,VK_SUBPASS_CONTENTS_INLINE);

			VkViewport vp = viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &vp);

			VkRect2D scissor = rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
#define r 7.5f
#define sin_t sin(timer * 2.0 * 3.14159265359f)
#define cos_t cos(timer * 2.0 * 3.14159265359f)
#define y -4.0f
			pushConstants[0] = glm::vec4(r * 1.1 * sin_t, y, r * 1.1 * cos_t, 1.0f);
			pushConstants[1] = glm::vec4(-r * sin_t, y, -r * cos_t, 1.0f);
			pushConstants[2] = glm::vec4(r * 0.85f * sin_t, y, -sin_t * 2.5f, 1.5f);
			pushConstants[3] = glm::vec4(0.0f, y, r * 1.25f * cos_t, 1.5f);
			pushConstants[4] = glm::vec4(r * 2.25f * cos_t, y, 0.0f, 1.25f);
			pushConstants[5] = glm::vec4(r * 2.5f * cos_t, y, r * 2.5f * sin_t, 1.25f);
#undef r
#undef y
#undef sin_t
#undef cos_t

			vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants) , pushConstants.data());
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.solid);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

			VkDeviceSize offset[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &models.sence.vertices.buffer, offset);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], models.sence.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(drawCmdBuffers[i], models.sence.indexCount, 1, 0, 0, 0);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);
		 	VK_CHECK_RESULT( vkEndCommandBuffer(drawCmdBuffers[i]));
		}

	}

	void loadAssets() 
	{
		models.sence.loadFromFile(getAssetPath() + "models/samplescene.dae", vertexLayout, 0.35f, vulkanDevice, queue);
	}

	void setupVertexDescription()
	{
		using namespace vks::initializers;

		vertexInputState.bindings.resize(1);
		vertexInputState.bindings[0] = vertexInputBindingDescription(0, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX);

		vertexInputState.attrs = {
			vertexInputAttributeDescription(0,0,VK_FORMAT_R32G32B32_SFLOAT,0),
			vertexInputAttributeDescription(0,1,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 3),
			vertexInputAttributeDescription(0,2,VK_FORMAT_R32G32_SFLOAT,sizeof(float) * 6),
			vertexInputAttributeDescription(0,3,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 8)
		};

		vertexInputState.state = pipelineVertexInputStateCreateInfo();
		vertexInputState.state.vertexBindingDescriptionCount = 1;
		vertexInputState.state.pVertexBindingDescriptions = vertexInputState.bindings.data();
		vertexInputState.state.vertexAttributeDescriptionCount = 4;
		vertexInputState.state.pVertexAttributeDescriptions = vertexInputState.attrs.data();
	}

	void setupDescriptorPool()
	{
		VkDescriptorPoolSize poolSize = vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
		VkDescriptorPoolCreateInfo poolCI = vks::initializers::descriptorPoolCreateInfo(1, &poolSize, 1);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolCI, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		auto setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		auto setLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(&setLayoutBinding, 1);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setLayoutCI, nullptr, &descriptorSetLayout));
		
		VkPushConstantRange pushConstantsRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(pushConstants), 0);

		auto pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout);
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = &pushConstantsRange;

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));
	}

	void setupDescriptorSet()
	{
		using namespace vks::initializers;

		auto setAllocI = descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &setAllocI, &descriptorSet));

		auto writeSetI = writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeSetI, 0, nullptr);
	}

	void preparePipelines()
	{
		using namespace vks::initializers;

		auto inputAssemblySCI = pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		auto rasterizationSCI = pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
		auto colorBlendAttachment = pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		auto colorBlendSCI = pipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
		auto depthStencilSCI = pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		auto vpSCI = pipelineViewportStateCreateInfo(1, 1);
		auto multisampleSCI = pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		VkDynamicState dynamicSs[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		auto dynamicSCI = pipelineDynamicStateCreateInfo(dynamicSs, wws::arr_len_v<decltype(dynamicSs)>);

		VkPipelineShaderStageCreateInfo shaderStage[2];
		shaderStage[0] = loadShader(getAssetPath() + "shaders/pushconstants/lights.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStage[1] = loadShader(getAssetPath() + "shaders/pushconstants/lights.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		auto pipelineCI = pipelineCreateInfo(pipelineLayout, renderPass);

		pipelineCI.pVertexInputState = &vertexInputState.state;
		pipelineCI.pInputAssemblyState = &inputAssemblySCI;
		pipelineCI.pRasterizationState = &rasterizationSCI;
		pipelineCI.pColorBlendState = &colorBlendSCI;
		pipelineCI.pMultisampleState = &multisampleSCI;
		pipelineCI.pViewportState = &vpSCI;
		pipelineCI.pDepthStencilState = &depthStencilSCI;
		pipelineCI.pDynamicState = &dynamicSCI;
		pipelineCI.stageCount = wws::arr_len_v<decltype(shaderStage)>;
		pipelineCI.pStages = shaderStage;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.solid));
	}

	void prepareUniformBuffers()
	{
		vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&uniformBuffer, sizeof(uboVS));

		updateUniformBuffer();
	}

	void updateUniformBuffer()
	{
		VK_CHECK_RESULT(uniformBuffer.map());

		uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 512.0f);
		glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, zoom));
		glm::mat4 model = view * glm::translate(glm::mat4(1.0f),glm::vec3(0.0f,0.0f,0.0f));

		model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		uboVS.modle = model;

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

		// Check requested push constant size against hardware limit
		// Specs require 128 bytes, so if the device complies our push constant buffer should always fit into memory		
		assert(sizeof(pushConstants) <= vulkanDevice->properties.limits.maxPushConstantsSize);

		loadAssets();
		setupVertexDescription();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void viewChanged() override
	{
		updateUniformBuffer();
	}

	struct {
		VkPipelineVertexInputStateCreateInfo state;
		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attrs;
	} vertexInputState;

	vks::VertexLayout vertexLayout = vks::VertexLayout({
		vks::VERTEX_COMPONENT_POSITION,
		vks::VERTEX_COMPONENT_NORMAL,
		vks::VERTEX_COMPONENT_UV,
		vks::VERTEX_COMPONENT_COLOR
	});
	struct {
		vks::Model sence;
	} models;

	vks::Buffer uniformBuffer;
	struct {
		glm::mat4 projection;
		glm::mat4 modle;
		glm::vec4 lightPos = glm::vec4(0.0f, 0.0f, -2.0f, 1.0f);
	} uboVS;

	struct {
		VkPipeline solid;
	} pipelines;

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	VkPipelineLayout pipelineLayout;

	std::array<glm::vec4, 6> pushConstants;
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


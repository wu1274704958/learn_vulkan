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
#include <comm/dbg.hpp>
#include <VulkanTexture.hpp>

class Example : public VulkanExampleBase {
public:
	void render() override
	{
		if (!prepared)
			return;
		draw();
		if (camera.updated)
		{
			updateUniformBuffers();
		}
	}

	Example() : VulkanExampleBase(true)
	{
		title = "specialization constants";
		camera.type = Camera::lookat;
		camera.setPerspective(60.0f, ((float)width / 3.0f) / (float)height, 0.1f, 512.0f);
		camera.setRotation(glm::vec3(-40.0f, -90.0f, 0.0f));
		camera.setPosition(glm::vec3(0.0f, 0.0f, -2.0f));
		settings.overlay = true;
	}

	~Example() {
		models.cube.destroy();
		textures.colormap.destroy();
		uniformBuffer.destroy();

		vkDestroyPipeline(device, pipelines.phong, nullptr);
		vkDestroyPipeline(device, pipelines.tong, nullptr);
		vkDestroyPipeline(device, pipelines.textured, nullptr);
		
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	}

	void buildCommandBuffers() override
	{
		using namespace vks::initializers;

		dbg("buildCommandBuffers()");

		auto cmdBufferBeginI = commandBufferBeginInfo();
		auto renderPassBeginI = renderPassBeginInfo();

		VkClearValue clearVal[2];
		clearVal[0].color = defaultClearColor;
		clearVal[1].depthStencil = { 1.0f,0 };

		renderPassBeginI.clearValueCount = wws::arrLen<uint32_t >(clearVal);
		renderPassBeginI.pClearValues = clearVal;
		renderPassBeginI.renderPass = renderPass;
		renderPassBeginI.renderArea = { {0,0},{width,height} };

		auto width_1_3 = (float)width / 3.0f;

		for (uint32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBeginI.framebuffer = frameBuffers[i];
			vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufferBeginI);

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginI, VK_SUBPASS_CONTENTS_INLINE);
			

			auto vp = viewport(width_1_3, (float)height, 0.0f, 1.0f);

			auto scissor = rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			VkDeviceSize offsets[] = { 0 };

			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &models.cube.vertices.buffer, offsets);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], models.cube.indices.buffer, 0 ,VK_INDEX_TYPE_UINT32);

			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &vp);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.phong);
			vkCmdDrawIndexed(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);

			vp.x += width_1_3;
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &vp);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.tong);
			vkCmdDrawIndexed(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);

			vp.x += width_1_3;
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &vp);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.textured);
			vkCmdDrawIndexed(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);
			vkEndCommandBuffer(drawCmdBuffers[i]);
		}
	}

	void loadAssets()
	{
		models.cube.loadFromFile(getAssetPath() + "models/color_teapot_spheres.dae", vertexLayout,0.1f, vulkanDevice,queue);
		textures.colormap.loadFromFile(getAssetPath() + "textures/metalplate_nomips_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	}

	void setupVertexDescriptions()
	{
		using namespace vks::initializers;

		vertices.bindings = {
			vertexInputBindingDescription(0, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX)
		};

		vertices.attrs = {
			vertexInputAttributeDescription(0,0,VK_FORMAT_R32G32B32_SFLOAT,0),
			vertexInputAttributeDescription(0,1,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 3),
			vertexInputAttributeDescription(0,2,VK_FORMAT_R32G32_SFLOAT,sizeof(float) * 6),
			vertexInputAttributeDescription(0,3,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 8)
		};

		vertices.inputState = pipelineVertexInputStateCreateInfo();
		vertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindings.size());
		vertices.inputState.pVertexBindingDescriptions = vertices.bindings.data();
		vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attrs.size());
		vertices.inputState.pVertexAttributeDescriptions = vertices.attrs.data();
	}

	void setupDescriptorPool()
	{
		VkDescriptorPoolSize sizes[] = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1)
		};
		auto poolCI = vks::initializers::descriptorPoolCreateInfo(wws::arrLen<uint32_t>(sizes), sizes, 2);
		vkCreateDescriptorPool(device, &poolCI, nullptr, &descriptorPool);
	}

	void setupDescriptorLayout()
	{
		using namespace vks::initializers;
		VkDescriptorSetLayoutBinding setLayoutBindings[] = {
			descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT,0),
			descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
		};

		auto setLayoutCI = descriptorSetLayoutCreateInfo(setLayoutBindings, wws::arrLen<uint32_t>(setLayoutBindings));
		vkCreateDescriptorSetLayout(device, &setLayoutCI, nullptr, &descriptorSetLayout);

		auto pipelineLayoutCI = pipelineLayoutCreateInfo(&descriptorSetLayout);
		vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout);
	}

	void setupDescriptorSet()
	{
		auto setAllocI = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		vkAllocateDescriptorSets(device, &setAllocI, &descriptorSet);

		VkWriteDescriptorSet writeDescSets[] = {
			vks::initializers::writeDescriptorSet(descriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0,&uniformBuffer.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSet,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,&textures.colormap.descriptor)
		};
		vkUpdateDescriptorSets(device, wws::arrLen<uint32_t>(writeDescSets), writeDescSets, 0, nullptr);
	}

	void preparePipelines()
	{
		using namespace vks::initializers;

		auto inputAssemblySCI = pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		auto rasterizationSCI = pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
		auto colorBlendAS = pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		auto colorBlendSCI = pipelineColorBlendStateCreateInfo(1, &colorBlendAS);
		auto depthStencilSCI = pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		auto vpSCI = pipelineViewportStateCreateInfo(1, 1);
		auto multisampleSCI = pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		VkDynamicState dynamicSs[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		auto dynamicSCI = pipelineDynamicStateCreateInfo(dynamicSs, wws::arrLen<uint32_t>(dynamicSs));
		VkPipelineShaderStageCreateInfo shaderStages[2] = {};

		auto pipelineCI = pipelineCreateInfo(pipelineLayout, renderPass);

		pipelineCI.pInputAssemblyState = &inputAssemblySCI;
		pipelineCI.pRasterizationState = &rasterizationSCI;
		pipelineCI.pColorBlendState = &colorBlendSCI;
		pipelineCI.pDepthStencilState = &depthStencilSCI;
		pipelineCI.pDynamicState = &dynamicSCI;
		pipelineCI.pMultisampleState = &multisampleSCI;
		pipelineCI.pStages = shaderStages;
		pipelineCI.pViewportState = &vpSCI;
		pipelineCI.stageCount = wws::arrLen<uint32_t>(shaderStages);
		pipelineCI.pVertexInputState = &vertices.inputState;

		struct {
			uint32_t lightingMode;
			float toonDesaturationFactor = 0.5f;
		} specializationData;

		VkSpecializationMapEntry specializationMapEntrys[] = {
			specializationMapEntry(0,0,sizeof(specializationData.lightingMode)),
			specializationMapEntry(1,sizeof(specializationData.lightingMode),sizeof(specializationData.toonDesaturationFactor))
		};

		auto specializationI = specializationInfo(wws::arrLen<uint32_t>(specializationMapEntrys), specializationMapEntrys, sizeof(specializationData), &specializationData);

		shaderStages[0] = loadShader(getAssetPath() + "shaders/specializationconstants/uber.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/specializationconstants/uber.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		shaderStages[1].pSpecializationInfo = &specializationI;

		specializationData.lightingMode = 0;
		vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.phong);

		specializationData.lightingMode = 1;
		vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.tong);

		specializationData.lightingMode = 2;
		vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.textured);
	}

	void prepareUniformBuffers()
	{
		vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(uboVS));

		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		uniformBuffer.map();
		camera.setPerspective(60.0f, ((float)width / 3.0f) / (float)height, 0.1f, 512.0f);
		uboVS.projection = camera.matrices.perspective;
		uboVS.model = camera.matrices.view;

		memcpy(uniformBuffer.mapped, &uboVS, sizeof(uboVS));

		uniformBuffer.unmap();
	}

	void draw() 
	{
		VulkanExampleBase::prepareFrame();

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		vkQueueSubmit(queue, 1, &submitInfo,VK_NULL_HANDLE);

		VulkanExampleBase::submitFrame();
	}

	virtual void prepare() override 
	{
		VulkanExampleBase::prepare();
		loadAssets();
		setupVertexDescriptions();
		prepareUniformBuffers();
		setupDescriptorPool();
		setupDescriptorLayout();
		setupDescriptorSet();
		preparePipelines();
		buildCommandBuffers();
		prepared = true;
	}

	void windowResized() override
	{
		updateUniformBuffers();
	}
private:
	struct 
	{
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attrs;
	} vertices;

	vks::VertexLayout vertexLayout = {
		{
			vks::VERTEX_COMPONENT_POSITION,
			vks::VERTEX_COMPONENT_NORMAL,
			vks::VERTEX_COMPONENT_UV,
			vks::VERTEX_COMPONENT_COLOR
		}
	};

	struct { 
		vks::Model cube; 
	} models;
	struct {
		vks::Texture2D colormap;
	} textures;

	struct 
	{
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 lightPos = { 0.0f,-2.0f,1.0f,0.0f }; 
	} uboVS;

	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;

	struct {
		VkPipeline phong;
		VkPipeline tong;
		VkPipeline textured;
	} pipelines;
	
	vks::Buffer uniformBuffer;
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
	/*char buf[100] = { 0 };
	sprintf(buf, "%d", wws::arrLen(buf));
	MessageBoxA(NULL, buf, "tip", MB_OK);*/
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
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>
#include <VulkanModel.hpp>
#include <VulkanBuffer.hpp>
#include <VulkanSwapChain.hpp>
#include <VulkanTexture.hpp>

class Example : public VulkanExampleBase {
public:
	Example() : VulkanExampleBase(true)
	{
		title = "use descriptor set";
		settings.overlay = true;
		camera.type = Camera::CameraType::lookat;
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 512.0f);
		camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
		camera.setTranslation(glm::vec3(0.0f, 0.0f, -5.0f));
	}
	~Example() 
	{
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		models.cube.destroy();
		for (auto &cube : cubes)
		{
			cube.texture.destroy();
			cube.uniformBuffer.destroy();
		}
	}

	virtual void getEnabledFeatures() override
	{
		if (deviceFeatures.samplerAnisotropy)
		{
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		}
	}

	void buildCommandBuffers()
	{
		using namespace vks::initializers;

		VkCommandBufferBeginInfo commandBufferBegin = commandBufferBeginInfo();

		VkClearValue clearVal[2];
		clearVal[0].color = defaultClearColor;
		clearVal[1].depthStencil = { 1.0f,0 };

		VkRenderPassBeginInfo renderPassBegin = renderPassBeginInfo();
		renderPassBegin.clearValueCount = 2;
		renderPassBegin.pClearValues = clearVal;
		renderPassBegin.renderPass = renderPass;
		renderPassBegin.renderArea = { {0,0} , {width,height} };

		for (int i = 0; i < frameBuffers.size(); ++i)
		{
			renderPassBegin.framebuffer = frameBuffers[i];

			vkBeginCommandBuffer(drawCmdBuffers[i], &commandBufferBegin);

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			VkViewport vp = viewport((float)width, (float)height, 0.0f, 1.0f);
			VkRect2D scissor = rect2D(width, height, 0, 0);

			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &vp);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &models.cube.vertices.buffer, &offset);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], models.cube.indices.buffer, 0,VK_INDEX_TYPE_UINT32);

			for (auto &cube : cubes)
			{
				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &cube.descriptorSet, 0, NULL);
				vkCmdDrawIndexed(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);
			}

			drawUI(drawCmdBuffers[i]);

			vkEndCommandBuffer(drawCmdBuffers[i]);
			vkCmdEndRenderPass(drawCmdBuffers[i]);
		}
	}

	void loadAssets()
	{
		models.cube.loadFromFile(getAssetPath() + "models/cube.dae", vertexLayout, 1.0f, vulkanDevice, queue);
		cubes[0].texture.loadFromFile(getAssetPath() + "textures/crate01_color_height_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		cubes[1].texture.loadFromFile(getAssetPath() + "textures/crate02_color_height_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
	}

	void setupDescriptors()
	{
		using namespace vks::initializers;

		std::array<VkDescriptorSetLayoutBinding, 2> setLayoutBinding;
		setLayoutBinding[0] = {};
		setLayoutBinding[0].binding = 0;
		setLayoutBinding[0].descriptorCount = 1;
		setLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		setLayoutBinding[0].pImmutableSamplers = VK_NULL_HANDLE;
		setLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		
		setLayoutBinding[1] = {};
		setLayoutBinding[1].binding = 1;
		setLayoutBinding[1].descriptorCount = 1;
		setLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		setLayoutBinding[1].pImmutableSamplers = VK_NULL_HANDLE;
		setLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = descriptorSetLayoutCreateInfo(setLayoutBinding.data(), static_cast<uint32_t>(setLayoutBinding.size()));

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

		std::array<VkDescriptorPoolSize,2> poolSizes = {};
		poolSizes[0] = descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(cubes.size()));
		poolSizes[1] = descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(cubes.size()));

		VkDescriptorPoolCreateInfo poolCreateInfo = descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), static_cast<uint32_t>(poolSizes.size()));
		VK_CHECK_RESULT( vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool) );

		for (auto &cube : cubes)
		{
			VkDescriptorSetAllocateInfo setAllocateInfo = descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &setAllocateInfo, &cube.descriptorSet));

			std::array<VkWriteDescriptorSet,2> writeDescriptorSets = {
				writeDescriptorSet(cube.descriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0,&cube.uniformBuffer.descriptor),
				writeDescriptorSet(cube.descriptorSet,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,&cube.texture.descriptor)
			};

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
		}
	}

	void preparePipelines()
	{
		using namespace vks::initializers;

		VkPipelineLayoutCreateInfo pipelineLayoutCI = pipelineLayoutCreateInfo(&descriptorSetLayout);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

		VkGraphicsPipelineCreateInfo pipelineCI = pipelineCreateInfo(pipelineLayout, renderPass);
		
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicStateCI = pipelineDynamicStateCreateInfo(dynamicStates);

		pipelineCI.pDynamicState = &dynamicStateCI;

		auto inputAssemblyCI = pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		auto rasterizationCI = pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
		auto colorBlendAttachment = pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		auto colorBlendCI = pipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
		auto depthStencilCI = pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		auto viewportCI = pipelineViewportStateCreateInfo(1, 1);
		auto multisampleCI = pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

		auto vertexInputBindingDesc = vertexInputBindingDescription(0, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX);

		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vertexInputAttributeDescription(0,0,VK_FORMAT_R32G32B32_SFLOAT,0),
			vertexInputAttributeDescription(0,1,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 3),
			vertexInputAttributeDescription(0,2,VK_FORMAT_R32G32_SFLOAT,sizeof(float) * 6),
			vertexInputAttributeDescription(0,3,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 8)
		};

		auto vertexInputStateCI = pipelineVertexInputStateCreateInfo();
		vertexInputStateCI.vertexBindingDescriptionCount = 1;
		vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBindingDesc;
		vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

		pipelineCI.pInputAssemblyState = &inputAssemblyCI;
		pipelineCI.pRasterizationState = &rasterizationCI;
		pipelineCI.pColorBlendState = &colorBlendCI;
		pipelineCI.pDepthStencilState = &depthStencilCI;
		pipelineCI.pViewportState = &viewportCI;
		pipelineCI.pMultisampleState = &multisampleCI;
		pipelineCI.pVertexInputState = &vertexInputStateCI;
	

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderModules = {
			loadShader(getAssetPath() + "shaders/descriptorsets/cube.vert.spv",VK_SHADER_STAGE_VERTEX_BIT),
			loadShader(getAssetPath() + "shaders/descriptorsets/cube.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		pipelineCI.stageCount = static_cast<uint32_t>(shaderModules.size());
		pipelineCI.pStages = shaderModules.data();

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
	}

	void prepareUniformBuffers()
	{
		for (auto &cube : cubes)
		{
			vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				&cube.uniformBuffer, sizeof(Cube::matrices));
		}

		updateUniformBuffers();
	}

	void updateUniformBuffers()
	{
		cubes[0].matrices.model = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, 0.0f));
		cubes[1].matrices.model = glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 0.5f, 0.0f));

		for (auto &cube : cubes)
		{
			VK_CHECK_RESULT(cube.uniformBuffer.map());

			cube.matrices.projection = camera.matrices.perspective;
			cube.matrices.view = camera.matrices.view;
			cube.matrices.model = glm::rotate(cube.matrices.model, cube.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
			cube.matrices.model = glm::rotate(cube.matrices.model, cube.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
			cube.matrices.model = glm::rotate(cube.matrices.model, cube.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
			memcpy(cube.uniformBuffer.mapped, &cube.matrices, sizeof(Cube::matrices));
			cube.uniformBuffer.unmap();
		}
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();
		VkSubmitInfo submitI = vks::initializers::submitInfo();
		submitI.commandBufferCount = 1;
		submitI.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT( vkQueueSubmit(queue, 1, &submitI, VK_NULL_HANDLE) );
		VulkanExampleBase::submitFrame();
	}

	virtual void prepare() override
	{
		VulkanExampleBase::prepare();
		loadAssets();
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelines();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render() override 
	{
		if (!prepared)
			return;
		draw();

		if (animate)
		{
			cubes[0].rotation.x += 2.5f * frameTimer;
			if (cubes[0].rotation.x > 360.0f)
				cubes[0].rotation.x -= 360.0f;
			cubes[1].rotation.y += 2.0f * frameTimer;
			if (cubes[1].rotation.y > 360.0f)
				cubes[1].rotation.y -= 360.0f;

			//std::cout << cubes[0].rotation.y << "  " << frameTimer << std::endl;
		}

		if (animate || camera.updated)
		{
			updateUniformBuffers();
		}
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
	{
		if (overlay->header("Settings"))
		{
			overlay->checkBox("Animate", &animate);
		}
	}
private:
	bool animate = true;
	vks::VertexLayout vertexLayout = vks::VertexLayout({ 
		vks::VERTEX_COMPONENT_POSITION,
		vks::VERTEX_COMPONENT_NORMAL,
		vks::VERTEX_COMPONENT_UV,
		vks::VERTEX_COMPONENT_COLOR
	});

	struct Cube {
		struct {
			glm::mat4 projection;
			glm::mat4 view;
			glm::mat4 model;
		} matrices;
		VkDescriptorSet descriptorSet;
		vks::Texture2D texture;
		vks::Buffer uniformBuffer;
		glm::vec3 rotation = glm::vec3(0.0f,0.0f,0.0f);
	};

	std::array<Cube, 2> cubes;
	struct 
	{
		vks::Model cube;
	} models;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	VkDescriptorSetLayout descriptorSetLayout;


	
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
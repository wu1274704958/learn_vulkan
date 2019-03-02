
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>
#include <VulkanBuffer.hpp>
#include <VulkanDevice.hpp>
#include <comm/CommTool.hpp>
#include <random>

#define OBJECT_INSTANCES 125


struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
};

class Example : public VulkanExampleBase {
public:
	virtual void render() override {
		if (!prepared)
			return;
		draw();
		if (!paused)
			updateDynamicUniformBuffers();
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
			wws::aligned_free(dynamicData);

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

		renderPassBeginI.clearValueCount = wws::arr_len_v<decltype(clearVal)>;
		renderPassBeginI.pClearValues = clearVal;
		renderPassBeginI.renderArea = { {0,0},{width,height} };
		renderPassBeginI.renderPass = renderPass;

		for (int i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBeginI.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBeginI));

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginI, VK_SUBPASS_CONTENTS_INLINE);

			auto vp = viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &vp);

			auto scissor = rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
			VkDeviceSize offset[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertexBuffer.buffer, offset);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			for (int j = 0; j < OBJECT_INSTANCES; ++j)
			{
				uint32_t dynamicOffset = static_cast<uint32_t>(dynamicAlignment) * j;
				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 1, &dynamicOffset);
				vkCmdDrawIndexed(drawCmdBuffers[i], indexCount, 1, 0, 0, 0);
			}

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);
			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		auto submitI = vks::initializers::submitInfo();
		submitI.commandBufferCount = 1;
		submitI.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitI, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void generateCube()
	{
		std::vector<Vertex> vertices = {
			{ { -1.0f, -1.0f,  1.0f },{ 1.0f, 0.0f, 0.0f } },
			{ {  1.0f, -1.0f,  1.0f },{ 0.0f, 1.0f, 0.0f } },
			{ {  1.0f,  1.0f,  1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -1.0f,  1.0f,  1.0f },{ 0.0f, 0.0f, 0.0f } },
			{ { -1.0f, -1.0f, -1.0f },{ 1.0f, 0.0f, 0.0f } },
			{ {  1.0f, -1.0f, -1.0f },{ 0.0f, 1.0f, 0.0f } },
			{ {  1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 0.0f } },
		};

		std::vector<uint32_t> indices = {
			0,1,2, 2,3,0, 1,5,6, 6,2,1, 7,6,5, 5,4,7, 4,0,3, 3,7,4, 4,5,1, 1,0,4, 3,2,6, 6,7,3,
		};

		indexCount = static_cast<uint32_t>(indices.size());

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &vertexBuffer,
			sizeof(Vertex) * static_cast<uint32_t>(vertices.size()), vertices.data());

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &indexBuffer,
			sizeof(uint32_t) * static_cast<uint32_t>(indices.size()), indices.data());
	}

	void setupDescriptorPool()
	{
		VkDescriptorPoolSize poolSizes[] = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1)
		};

		auto poolI = vks::initializers::descriptorPoolCreateInfo(
			wws::arr_len_v<decltype(poolSizes)>,poolSizes, wws::arr_len_v<decltype(poolSizes)>
		);

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolI, nullptr, &descriptorPool));
	}

	void setupDescriptorLayout()
	{
		VkDescriptorSetLayoutBinding setLayoutBings[] = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT,0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT,1)
		};

		auto descriptorLayoutI = vks::initializers::descriptorSetLayoutCreateInfo(
			setLayoutBings,
			wws::arr_len_v<decltype(setLayoutBings)>
		);

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutI, nullptr, &descriptorSetLayout));

		auto pipelineLayoutI = vks::initializers::pipelineLayoutCreateInfo(
			&descriptorSetLayout
		);

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutI, nullptr, &pipelineLayout));
	}

	void setupDescriptorSet()
	{
		auto descriptorSetAllocI = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocI, &descriptorSet));

		VkWriteDescriptorSet writeDescriptorSets[] = {
			vks::initializers::writeDescriptorSet(descriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0,&uniformBuffers.view.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1,&uniformBuffers.dynamic.descriptor)
		};

		vkUpdateDescriptorSets(device, wws::arr_len_v<decltype(writeDescriptorSets)>, writeDescriptorSets, 0, nullptr);
	}

	void preparePipelines()
	{
		using namespace vks::initializers;

		auto inputAssemblySCI = pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		auto rasterizationSCI = pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		auto colorBlendAttachment = pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		auto colorBlendSCI = pipelineColorBlendStateCreateInfo(1, &colorBlendAttachment);
		auto depthStencilSCI = pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		auto vpSCI = pipelineViewportStateCreateInfo(1, 1);
		auto multisampleSCI = pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		VkDynamicState dynamicSs[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		auto dynamicSCI = pipelineDynamicStateCreateInfo(dynamicSs,wws::arr_len_v<decltype(dynamicSs)>);
		VkPipelineShaderStageCreateInfo shaderStages[2];

		shaderStages[0] = loadShader(getAssetPath() + "shaders/dynamicuniformbuffer/base.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/dynamicuniformbuffer/base.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		auto pipelineCI = pipelineCreateInfo(pipelineLayout, renderPass);

		VkVertexInputBindingDescription vertexInputBindings = 
			vertexInputBindingDescription(0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX);
		VkVertexInputAttributeDescription vertexInputAttrs[] = {
			vertexInputAttributeDescription(0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,pos)),
			vertexInputAttributeDescription(0,1,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,color))
		};
		auto vertexInputSCI = pipelineVertexInputStateCreateInfo();
		vertexInputSCI.vertexBindingDescriptionCount = 1;
		vertexInputSCI.pVertexBindingDescriptions = &vertexInputBindings;
		vertexInputSCI.vertexAttributeDescriptionCount = wws::arr_len_v<decltype(vertexInputAttrs)>;
		vertexInputSCI.pVertexAttributeDescriptions = vertexInputAttrs;

		pipelineCI.stageCount = wws::arr_len_v<decltype(shaderStages)>;
		pipelineCI.pStages = shaderStages;
		pipelineCI.pColorBlendState = &colorBlendSCI;
		pipelineCI.pDepthStencilState = &depthStencilSCI;
		pipelineCI.pDynamicState = &dynamicSCI;
		pipelineCI.pInputAssemblyState = &inputAssemblySCI;
		pipelineCI.pMultisampleState = &multisampleSCI;
		pipelineCI.pRasterizationState = &rasterizationSCI;
		pipelineCI.pViewportState = &vpSCI;
		pipelineCI.pVertexInputState = &vertexInputSCI;

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
	}
	//min 4  need 8  4+8-1 = 11  4-1 = 3	|  min 4  need 3  4+3-1 = 6  4-1 = 3
	// 11			0000 1011				|   6			0000 0110
	//  3						0000 0011	|   3						0000 0011
	// ~3			1111 1100	1111 1100	|  ~3			1111 1100	1111 1100
	// 11 & ~3 = 8	0000 1000				|   6 & ~3 = 4	0000 0100
	void prepareUniforms()
	{
		size_t minUBOAlignement = vulkanDevice->properties.limits.minUniformBufferOffsetAlignment;
		dynamicAlignment = sizeof(glm::mat4);
		if (minUBOAlignement > 0)
		{
			dynamicAlignment = (dynamicAlignment + minUBOAlignement - 1) & ~(minUBOAlignement - 1);
		}

		size_t bufferSize = OBJECT_INSTANCES * dynamicAlignment;

		dynamicData = wws::aligned_alloc<glm::mat4>(dynamicAlignment, bufferSize);
		assert(dynamicData);
		std::cout << "minUniformBufferOffsetAlignment = " << minUBOAlignement << std::endl;
		std::cout << "dynamicAlignment = " << dynamicAlignment << std::endl;

		VK_CHECK_RESULT( vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &uniformBuffers.view, sizeof(uboVS)));


		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &uniformBuffers.dynamic, bufferSize));

		std::default_random_engine rndEngine(benchmark.active ? 0 : (unsigned)time(nullptr));
		std::normal_distribution<float> rndDist(-1.0f, 1.0f);
		for (uint32_t i = 0; i < OBJECT_INSTANCES; i++) {
			rotations[i] = glm::vec3(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine)) * 2.0f * 3.14159265359f;
			rotationSpeeds[i] = glm::vec3(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine));
		}

		updateUniformBuffers();
		updateDynamicUniformBuffers();
	}

	void updateUniformBuffers()
	{
		VK_CHECK_RESULT(uniformBuffers.view.map());

		uboVS.projection = camera.matrices.perspective;
		uboVS.view = camera.matrices.view;

		memcpy(uniformBuffers.view.mapped, &uboVS, sizeof(uboVS));

		uniformBuffers.view.unmap();
	}

	void updateDynamicUniformBuffers(bool force = false)
	{
		animationTimer += frameTimer;
		if ((animationTimer <= 1.0f / 60.0f) && (!force)) {
			return;
		}

		// Dynamic ubo with per-object model matrices indexed by offsets in the command buffer
		uint32_t dim = static_cast<uint32_t>(pow(OBJECT_INSTANCES, (1.0f / 3.0f)));
		std::cout << "dim = "<< dim << std::endl;
		glm::vec3 offset(5.0f);
		auto offset_1_2 = 0.5f * offset;
		auto first_pos = -(static_cast<float>(dim) * offset_1_2);

		for (uint32_t x = 0; x < dim; x++)
		{
			for (uint32_t y = 0; y < dim; y++)
			{
				for (uint32_t z = 0; z < dim; z++)
				{
					uint32_t index = x * dim * dim + y * dim + z;

					// Aligned offset
					glm::mat4* modelMat = (glm::mat4*)(((uint64_t)dynamicData + (index * dynamicAlignment)));

					// Update rotations
					rotations[index] += animationTimer * rotationSpeeds[index];

					// Update matrices
					glm::vec3 pos = glm::vec3(	first_pos.x + offset_1_2.x + x * offset.x, 
												first_pos.y + offset_1_2.y + y * offset.y,
												first_pos.z + offset_1_2.z + z * offset.z
					);
					*modelMat = glm::translate(glm::mat4(1.0f), pos);
					*modelMat = glm::rotate(*modelMat, rotations[index].x, glm::vec3(1.0f, 1.0f, 0.0f));
					*modelMat = glm::rotate(*modelMat, rotations[index].y, glm::vec3(0.0f, 1.0f, 0.0f));
					*modelMat = glm::rotate(*modelMat, rotations[index].z, glm::vec3(0.0f, 0.0f, 1.0f));
				}
			}
		}

		animationTimer = 0.0f;
		VK_CHECK_RESULT(uniformBuffers.dynamic.map());

		memcpy(uniformBuffers.dynamic.mapped, dynamicData, uniformBuffers.dynamic.size);
		// Flush to make changes visible to the host 
		VkMappedMemoryRange memoryRange = vks::initializers::mappedMemoryRange();
		memoryRange.memory = uniformBuffers.dynamic.memory;
		memoryRange.size = uniformBuffers.dynamic.size;
		vkFlushMappedMemoryRanges(device, 1, &memoryRange);

		uniformBuffers.dynamic.unmap();

	}
	void prepare()
	{
		VulkanExampleBase::prepare();
		generateCube();
		prepareUniforms();
		setupDescriptorLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void viewChanged() override
	{
		updateUniformBuffers();
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
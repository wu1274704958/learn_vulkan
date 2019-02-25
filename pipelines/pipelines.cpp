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
	} vboVS;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;

	struct {
		VkPipeline phong;
		VkPipeline wireframe;
		VkPipeline toon;
	} pipelines;

	Example() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		zoom = -10.5f;
		rotation = glm::vec3(-25.0f, 15.0f, 0.0f);
		title = "Pipeline state object";
		settings.overlay = true;
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

	virtual void getEnabledFeatures() override
	{
		if (deviceFeatures.fillModeNonSolid)
		{
			enabledFeatures.fillModeNonSolid = true;
			if (deviceFeatures.wideLines)
			{
				enabledFeatures.wideLines = true;
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

		VkDescriptorPoolCreateInfo poolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSize.size(), poolSize.data(), 2);
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
			VK_FRONT_FACE_COUNTER_CLOCKWISE
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
	MessageBox(NULL, "hello", "tip", MB_OK);
	return 0;
}

#else

int main()
{
	return 0;
}

#endif // 
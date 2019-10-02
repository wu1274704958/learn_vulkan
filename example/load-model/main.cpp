#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>
#include <VulkanBuffer.hpp>
#include <VulkanDevice.hpp>
#include <comm/CommTool.hpp>
#include <comm/dbg.hpp>
#include "comm/macro.h"

#include <assimp/cimport.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <VulkanTexture.hpp>

class Example : public VulkanExampleBase
{
private:
	bool wireframe = false;

	struct {
		vks::Texture2D colorMap;
	} textures;

	struct 
	{
		VkPipelineVertexInputStateCreateInfo info;
		std::vector < VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attrs;
	} inputState;

	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 color;
	};

	struct Model
	{
		struct {
			VkBuffer buf;
			VkDeviceMemory mem;
		}vertices;

		struct {
			int count;
			VkBuffer buf;
			VkDeviceMemory mem;
		}indeices;

		void destroy(VkDevice device)
		{
			vkDestroyBuffer(device, vertices.buf, nullptr);
			vkFreeMemory(device, vertices.mem,nullptr);

			vkDestroyBuffer(device, indeices.buf, nullptr);
			vkFreeMemory(device, indeices.mem, nullptr);
		}
	} model;

	struct {
		vks::Buffer scene;
	}uniformBufs;

	struct 
	{
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 lightPos = glm::vec4(25.0f, 5.0f, 5.0f, 1.0f);
	} uboVS;

	struct {
		VkPipeline solid;
		VkPipeline wireframe = VK_NULL_HANDLE;
	} pipelines;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;


public:
	Example() : VulkanExampleBase(true)
	{
		zoom = -5.5f;
		zoomSpeed = 2.5f;
		rotationSpeed = 0.5f;
		rotation = { -0.5f, -112.75f, 0.0f };
		cameraPos = { 0.1f, 1.1f, 0.0f };
		title = "Model rendering";
		settings.overlay = true;
	}
	~Example()
	{
		vkDestroyPipeline(device, pipelines.solid, nullptr);
		if (pipelines.wireframe != VK_NULL_HANDLE)
			vkDestroyPipeline(device, pipelines.wireframe, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		model.destroy(device);
		textures.colorMap.destroy();
		uniformBufs.scene.destroy();
	}

	void getEnabledFeatures() override
	{
		if (deviceFeatures.fillModeNonSolid)
			enabledFeatures.fillModeNonSolid = VK_TRUE;
	}

	void buildCommandBuffers() override
	{
		using namespace vks::initializers;
		VkCommandBufferBeginInfo cmdBeginInfo = commandBufferBeginInfo();

		VkClearValue clearVal[2];
		clearVal[0].color = defaultClearColor;
		clearVal[1].depthStencil = { 1.0f,0 };

		VkRenderPassBeginInfo renderPassBI = renderPassBeginInfo();
		renderPassBI.renderPass = renderPass;
		renderPassBI.renderArea = { {0,0},{width,height} };
		renderPassBI.clearValueCount = wws::arrLen(clearVal);
		renderPassBI.pClearValues = clearVal;

		for (int i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBI.framebuffer = frameBuffers[i];

			vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBeginInfo);

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport vp = viewport(static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &vp);

			VkRect2D scissor = rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.wireframe : pipelines.solid);

			VkDeviceSize offset[1] = { 0 };

			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &model.vertices.buf, offset);

			vkCmdBindIndexBuffer(drawCmdBuffers[i], model.indeices.buf, 0, VK_INDEX_TYPE_UINT32);

			vkCmdDrawIndexed(drawCmdBuffers[i], model.indeices.count, 1, 0, 0, 0);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			vkEndCommandBuffer(drawCmdBuffers[i]);
		}
	}

	void loadModel(std::string&& path)
	{
		const aiScene* scene;
		Assimp::Importer importer;

		constexpr  uint32_t load_flags = aiProcess_FlipWindingOrder |
			aiProcess_Triangulate |
			aiProcess_PreTransformVertices;

		scene = importer.ReadFile(path.c_str(), load_flags);

		float scale = 1.f;
		std::vector<Vertex> vertices;

		for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
		{
			auto curr_mesh = scene->mMeshes[m];
			for (uint32_t i = 0; i < curr_mesh->mNumVertices; ++i)
			{
				Vertex v;
				v.pos = glm::make_vec3(&curr_mesh->mVertices[i].x) * scale;
				v.normal = glm::make_vec3(&curr_mesh->mNormals[i].x);

				v.uv = glm::make_vec2(&curr_mesh->mTextureCoords[0][i].x);
				v.color = curr_mesh->HasVertexColors(0) ? glm::make_vec3(&curr_mesh->mColors[0]->r) : glm::vec3(1.f);

				// Vulkan uses a right-handed NDC (contrary to OpenGL), so simply flip Y-Axis
				v.pos.y *= -1.0f;
				vertices.push_back(v);
			}
		}

		size_t vertexBufferSize = sizeof(Vertex) * vertices.size();

		std::vector<uint32_t> indexBuffer;
		for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
		{
			auto curr_mesh = scene->mMeshes[m];
			uint32_t indexBase = static_cast<uint32_t>(indexBuffer.size());
			for (uint32_t f = 0; f < curr_mesh->mNumFaces; ++f)
			{
				//We assume that all faces are triangulated
				for (uint32_t i = 0; i < 3; ++i)
				{
					indexBuffer.push_back(curr_mesh->mFaces[f].mIndices[i] + indexBase);
				}
			}
		}

		size_t indexBufferSize = sizeof(uint32_t) * static_cast<uint32_t>(indexBuffer.size());

		model.indeices.count = static_cast<uint32_t>(indexBuffer.size());

		struct {
			VkBuffer buf;
			VkDeviceMemory mem;
		} vbuf,ibuf;

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBufferSize, &vbuf.buf,&vbuf.mem,vertices.data());

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBufferSize, &ibuf.buf, &ibuf.mem, indexBuffer.data());

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		, vertexBufferSize, &model.vertices.buf, &model.vertices.mem);
		
		vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			indexBufferSize, &model.indeices.buf, &model.indeices.mem);

		
		VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = {};
		
		copyRegion.size = vertexBufferSize;

		vkCmdCopyBuffer(copyCmd, vbuf.buf, model.vertices.buf, 1, &copyRegion);

		copyRegion.size = indexBufferSize;

		vkCmdCopyBuffer(copyCmd, ibuf.buf, model.indeices.buf, 1, &copyRegion);

		flushCommandBuffer(copyCmd, queue, true);

		vkDestroyBuffer(device,vbuf.buf,nullptr);
		vkFreeMemory(device, vbuf.mem, nullptr);

		vkDestroyBuffer(device, ibuf.buf, nullptr);
		vkFreeMemory(device, ibuf.mem, nullptr);
	}

	void loadAssets()
	{
		loadModel(getAssetPath() + "models/voyager/voyager.dae");
		textures.colorMap.loadFromFile(getAssetPath() + "models/voyager/voyager_rgba_unorm.ktx", VK_FORMAT_R8G8B8A8_UNORM,
			vulkanDevice, queue);
	}

	void setupVertexDescriptions()
	{
		using namespace vks::initializers;

		std::vector<VkVertexInputBindingDescription> binds = {
			vertexInputBindingDescription(0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX)
		};

		std::vector<VkVertexInputAttributeDescription> attrs = {
			vertexInputAttributeDescription(0,0,VK_FORMAT_R32G32B32_SFLOAT,0),
			vertexInputAttributeDescription(0,1,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 3),
			vertexInputAttributeDescription(0,2,VK_FORMAT_R32G32_SFLOAT,sizeof(float) * 6),
			vertexInputAttributeDescription(0,3,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 8)
		};

		inputState.info = {};

		inputState.bindings = binds;
		inputState.attrs = attrs;
		inputState.info.pVertexAttributeDescriptions = inputState.attrs.data();
		inputState.info.pVertexBindingDescriptions = inputState.bindings.data();

		inputState.info.vertexAttributeDescriptionCount = inputState.attrs.size();
		inputState.info.vertexBindingDescriptionCount = inputState.bindings.size();

	}

	void setupDescriptorPool()
	{
		VkDescriptorPoolSize sizes[] = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1)
		};

		VkDescriptorPoolCreateInfo poolCI = vks::initializers::descriptorPoolCreateInfo(wws::arrLen(sizes), sizes, 1);

		vkCreateDescriptorPool(device, &poolCI, nullptr, &descriptorPool);
	}

	void setupDescriptorSetLayout()
	{
		using namespace vks::initializers;
		VkDescriptorSetLayoutBinding binds[] = {
			descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT,0),
			descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT,1),
		};

		VkDescriptorSetLayoutCreateInfo layoutCI = descriptorSetLayoutCreateInfo(binds, wws::arrLen(binds));

		vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &descriptorSetLayout);

		VkPipelineLayoutCreateInfo pipelineCI = pipelineLayoutCreateInfo(&descriptorSetLayout);

		vkCreatePipelineLayout(device, &pipelineCI, nullptr, &pipelineLayout);
	}

	void setupDescriptorSet()
	{
		using namespace vks::initializers;

		VkDescriptorSetAllocateInfo setAI = descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		vkAllocateDescriptorSets(device, &setAI, &descriptorSet);

		VkWriteDescriptorSet ws[] = {
			writeDescriptorSet(descriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0,&uniformBufs.scene.descriptor),
			writeDescriptorSet(descriptorSet,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,&textures.colorMap.descriptor)
		};

		vkUpdateDescriptorSets(device, wws::arrLen(ws), ws, 0, nullptr);
	}

	void preparePipelines()
	{
		using namespace vks::initializers;
		
		VkPipelineInputAssemblyStateCreateInfo inputAssemblySCI = pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

		VkPipelineColorBlendAttachmentState colorAttachment = pipelineColorBlendAttachmentState(0xf,VK_FALSE);

		VkPipelineColorBlendStateCreateInfo colorBlendSCI = pipelineColorBlendStateCreateInfo(1, &colorAttachment);

		VkPipelineDepthStencilStateCreateInfo depthStencilSCI = pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

		VkDynamicState dss[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};

		VkPipelineDynamicStateCreateInfo dynamicSCI = pipelineDynamicStateCreateInfo(dss, wws::arrLen(dss));

		VkPipelineMultisampleStateCreateInfo multisampleSCI = pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

		VkPipelineRasterizationStateCreateInfo rasterizationSCI = pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_CLOCKWISE);

		VkPipelineViewportStateCreateInfo vpSCI = pipelineViewportStateCreateInfo(1, 1);

		VkGraphicsPipelineCreateInfo pipelineCI = pipelineCreateInfo(pipelineLayout, renderPass);

		
		VkPipelineShaderStageCreateInfo stages[] = {
			loadShader(getAssetPath() + "shaders/mesh/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			loadShader(getAssetPath() + "shaders/mesh/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		pipelineCI.pInputAssemblyState = &inputAssemblySCI;
		pipelineCI.pColorBlendState = &colorBlendSCI;
		pipelineCI.pDepthStencilState = &depthStencilSCI;
		pipelineCI.pDynamicState = &dynamicSCI;
		pipelineCI.pInputAssemblyState = &inputAssemblySCI;
		pipelineCI.pMultisampleState = &multisampleSCI;
		pipelineCI.pRasterizationState = &rasterizationSCI;
		pipelineCI.pViewportState = &vpSCI;
		pipelineCI.pVertexInputState = &inputState.info;
		pipelineCI.pStages = stages;
		pipelineCI.stageCount = wws::arrLen(stages);

		vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.solid);

		if (deviceFeatures.fillModeNonSolid)
		{
			rasterizationSCI.polygonMode = VK_POLYGON_MODE_LINE;
			rasterizationSCI.lineWidth = 1.0f;
			vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.wireframe);
		}
	}

	void prepareUniformBuffer()
	{
		vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBufs.scene, sizeof(uboVS));

		updateUniformBuffer();
	}

	void updateUniformBuffer()
	{
		uniformBufs.scene.map();

		uboVS.projection = glm::perspective(glm::radians(60.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 256.0f);
		auto viewMatrix = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, zoom));

		uboVS.model = viewMatrix * glm::translate(glm::mat4(1.0f), cameraPos);

		uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		memcpy(uniformBufs.scene.mapped, &uboVS, sizeof(uboVS));

		uniformBufs.scene.unmap();
	}

	void prepare() override
	{
		VulkanExampleBase::prepare();
		loadAssets();
		setupVertexDescriptions();
		prepareUniformBuffer();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared = true;
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

		vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

		VulkanExampleBase::submitFrame();
	}

	void render() override
	{
		if (!prepared)  return;
		draw();
		if (camera.updated)
			updateUniformBuffer();
	}

	void OnUpdateUIOverlay(vks::UIOverlay* overlay) override
	{
		if (overlay->header("Settings"))
		{
			if (overlay->checkBox("Wireframe", &wireframe))
			{
				buildCommandBuffers();
			}
		}
	}

};

EDF_EXAMPLE_MAIN_FUNC(Example, false, {
	int a = 1;
	char buf[9] = {0};
	sprintf(buf,"hhh%d",a);
	MessageBoxA(NULL, buf, "Tip", MB_OK);
})
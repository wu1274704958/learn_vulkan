#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>
#include <VulkanBuffer.hpp>
#include <VulkanDevice.hpp>
#include <comm/CommTool.hpp>
#include <comm/dbg.hpp>
#include <VulkanTexture.hpp>

struct Vertex {
	glm::vec3 pos;
	glm::vec2 uv;
	glm::vec3 normal;
};

class Example : public VulkanExampleBase {
private:

	struct Texture {
		VkSampler sampler;
		VkImage image;
		VkImageLayout imageLayout;
		VkDeviceMemory memory;
		VkImageView imageView;
		uint32_t width, height, mipLevels;
	} texture;

	struct {
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 viewPos;
		float lodBias = 0.0f;
	} uboVS;
	
	vks::Buffer vertexBuffer;
	vks::Buffer indexBuffer;
	vks::Buffer uniformBuffer;

	uint32_t indexCount;

	struct {
		VkPipeline soild;
	} pipelines;

	VkDescriptorSetLayout  descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;

	struct {
		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attrs;
		VkPipelineVertexInputStateCreateInfo info;
	} vertexInputState;

public:
	void render() override
	{
		if (!prepared)
			return
		draw();
	}

	Example() : VulkanExampleBase(true)
	{
		zoom = -2.5f;
		title = "load texture";
		rotation = { 0.0f, 15.0f, 0.0f };
		settings.overlay = true;
	}

	~Example()
	{
		uniformBuffer.destroy();
		vertexBuffer.destroy();
		indexBuffer.destroy();

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyPipeline(device, pipelines.soild, nullptr);
		
		destroyTextureImage(texture);
	}

	void destroyTextureImage(Texture &texture)
	{
		vkDestroyImageView(device, texture.imageView,nullptr);
		vkDestroyImage(device,texture.image,nullptr);
		vkDestroySampler(device, texture.sampler, nullptr);
		vkFreeMemory(device, texture.memory, nullptr);
	}

	void getEnabledFeatures() override
	{
		if (deviceFeatures.samplerAnisotropy)
		{
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		}
	}

	void loadTexture()
	{
		std::string path = getAssetPath() + "textures/metalplate01_rgba.ktx";
		VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

#if defined(__ANDROID__)
		// Textures are stored inside the apk on Android (compressed)
		// So they need to be loaded via the asset manager
		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, path.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t size = AAsset_getLength(asset);
		assert(size > 0);

		void *textureData = malloc(size);
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);

		gli::texture2d tex2D(gli::load((const char*)textureData, size));
#else
		gli::texture2d tex2D(gli::load(path));
#endif
		assert(!tex2D.empty());

		texture.mipLevels = static_cast<uint32_t>(tex2D.levels());
		texture.width = static_cast<uint32_t>(tex2D[0].extent().x);
		texture.height = static_cast<uint32_t>(tex2D[0].extent().y);

		bool useStaging = true;

		bool forceLinearTiling = false;
		if (forceLinearTiling)//如果强制启用线性平铺 如果设备支持就不用 device local memory
		{
			VkFormatProperties properties;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
			useStaging = !(properties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
		}

		using namespace vks::initializers;

		VkMemoryAllocateInfo memAllocInfo = memoryAllocateInfo();
		VkMemoryRequirements memRequirments;

		if (useStaging)
		{
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingMem;

			VkBufferCreateInfo bufferCI = bufferCreateInfo(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, tex2D.size());
			bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			vkCreateBuffer(device, &bufferCI, nullptr, &stagingBuffer);

			vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirments);

			memAllocInfo.allocationSize = memRequirments.size;
			memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memRequirments.memoryTypeBits,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			vkAllocateMemory(device, &memAllocInfo, nullptr, &stagingMem);
			vkBindBufferMemory(device, stagingBuffer, stagingMem, 0);

			void* data;
			vkMapMemory(device, stagingMem, 0, memRequirments.size, 0, &data);
			memcpy(data, tex2D.data(), tex2D.size());
			vkUnmapMemory(device, stagingMem);

			std::vector<VkBufferImageCopy> bufferCopyRegions;

			uint32_t offset = 0;
			for (int i = 0; i < texture.mipLevels; ++i)
			{
				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageSubresource.mipLevel = i;

				bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(tex2D[i].extent().x);
				bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(tex2D[i].extent().y);
				bufferCopyRegion.imageExtent.depth = 1;
				
				bufferCopyRegion.bufferOffset = offset;
				
				bufferCopyRegions.push_back(bufferCopyRegion);

				offset += tex2D[i].size();
			}

			VkImageCreateInfo imageCI = imageCreateInfo();
			imageCI.imageType = VK_IMAGE_TYPE_2D;
			imageCI.format = format;
			imageCI.mipLevels = texture.mipLevels;
			imageCI.arrayLayers = 1;
			imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCI.extent = { texture.width,texture.height,1 };
			imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			vkCreateImage(device, &imageCI, nullptr, &texture.image);

			vkGetImageMemoryRequirements(device, texture.image, &memRequirments);

			memAllocInfo.allocationSize = memRequirments.size;
			memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memRequirments.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			vkAllocateMemory(device, &memAllocInfo, nullptr, &texture.memory);

			vkBindImageMemory(device, texture.image, texture.memory,0);

			VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			VkImageSubresourceRange subResourceRange = {};
			subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subResourceRange.baseArrayLayer = 0;
			subResourceRange.baseMipLevel = 0;
			subResourceRange.layerCount = 1;
			subResourceRange.levelCount = texture.mipLevels;

			VkImageMemoryBarrier imageMemoryBar = imageMemoryBarrier();
			imageMemoryBar.image = texture.image;
			imageMemoryBar.subresourceRange = subResourceRange;
			imageMemoryBar.srcAccessMask = 0;
			imageMemoryBar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageMemoryBar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			vkCmdPipelineBarrier(copyCmd,
				VK_PIPELINE_STAGE_HOST_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0,nullptr,
				0,nullptr,
				1, &imageMemoryBar);

			vkCmdCopyBufferToImage(
				copyCmd,
				stagingBuffer,
				texture.image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data()
			);

			imageMemoryBar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemoryBar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageMemoryBar.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			vkCmdPipelineBarrier(
				copyCmd,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBar
			);

			texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);

			vkFreeMemory(device, stagingMem, nullptr);
			vkDestroyBuffer(device, stagingBuffer, nullptr);
		}
		else {
			VkImage mappableImage;
			VkDeviceMemory mappableMem;

			VkImageCreateInfo imageCI = imageCreateInfo();
			imageCI.imageType = VK_IMAGE_TYPE_2D;
			imageCI.format = format;
			imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCI.arrayLayers = 1;
			imageCI.extent = { texture.width , texture.height,1 };
			imageCI.mipLevels = 1;
			imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
			imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCI.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
			imageCI.tiling = VK_IMAGE_TILING_LINEAR;

			vkCreateImage(device, &imageCI, nullptr,&mappableImage);

			vkGetImageMemoryRequirements(device, mappableImage, &memRequirments);

			memAllocInfo.allocationSize = memRequirments.size;
			memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memRequirments.memoryTypeBits,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			
			vkAllocateMemory(device, &memAllocInfo, nullptr, &mappableMem);
			vkBindImageMemory(device, mappableImage, mappableMem,0);

			void *data;

			vkMapMemory(device, mappableMem, 0, memRequirments.size, 0,&data);

			memcpy(data, tex2D[0].data(), tex2D[0].size());

			vkUnmapMemory(device, mappableMem);

			texture.image = mappableImage;
			texture.memory = mappableMem;
			texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			VkImageSubresourceRange subResourceRange = {};
			subResourceRange.baseArrayLayer = 0;
			subResourceRange.baseMipLevel = 0;
			subResourceRange.layerCount = 1;
			subResourceRange.levelCount = 1;
			subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

			VkImageMemoryBarrier imageMemBarrier = imageMemoryBarrier();
			imageMemBarrier.image = texture.image;
			imageMemBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			vkCmdPipelineBarrier(copyCmd,
				VK_PIPELINE_STAGE_HOST_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemBarrier);

			VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);
		}

		VkSamplerCreateInfo sampleCI = samplerCreateInfo();
		sampleCI.minFilter = VK_FILTER_LINEAR;
		sampleCI.magFilter = VK_FILTER_LINEAR;
		sampleCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampleCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampleCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampleCI.mipLodBias = 0.0f;
		sampleCI.compareOp = VK_COMPARE_OP_NEVER;
		sampleCI.minLod = 0.0f;
		sampleCI.maxLod = useStaging ? (float)texture.mipLevels : 0.0f;

		if (vulkanDevice->enabledFeatures.samplerAnisotropy)
		{
			sampleCI.maxAnisotropy = vulkanDevice->properties.limits.maxSamplerAnisotropy;
			sampleCI.anisotropyEnable = VK_TRUE;
		}
		else {
			sampleCI.maxAnisotropy = 1.0f;
			sampleCI.anisotropyEnable = VK_FALSE;
		}

		sampleCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		vkCreateSampler(device, &sampleCI, nullptr, &texture.sampler);

		VkImageViewCreateInfo ivCI = imageViewCreateInfo();
		ivCI.format = format;
		ivCI.components = { VK_COMPONENT_SWIZZLE_R ,VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B,VK_COMPONENT_SWIZZLE_A };
		ivCI.image = texture.image;
		ivCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ivCI.subresourceRange.baseArrayLayer = 0;
		ivCI.subresourceRange.baseMipLevel = 0;
		ivCI.subresourceRange.layerCount = 1;
		ivCI.subresourceRange.levelCount = useStaging ? texture.mipLevels : 1;
		ivCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		vkCreateImageView(device, &ivCI, nullptr, &texture.imageView);
	}

	void buildCommandBuffers() override
	{
		VkCommandBufferBeginInfo cmdBeginI = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearVal[2];
		clearVal[0].color = defaultClearColor;
		clearVal[1].depthStencil = { 1.0f,0 };

		VkRenderPassBeginInfo renderPassBeginI = vks::initializers::renderPassBeginInfo();
		renderPassBeginI.renderPass = renderPass;
		renderPassBeginI.clearValueCount = wws::arrLen<uint32_t>(clearVal);
		renderPassBeginI.pClearValues = clearVal;
		renderPassBeginI.renderArea = { {0, 0} , {width,height} };

		for (int i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBeginI.framebuffer = frameBuffers[i];
			vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBeginI);
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginI, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport vp = vks::initializers::viewport(width, height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &vp);
			
			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.soild);

			VkDeviceSize offset[1] = { 0 };

			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertexBuffer.buffer, offset);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdDrawIndexed(drawCmdBuffers[i], indexCount, 1, 0, 0, 0);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]) ;
			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

		vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

		VulkanExampleBase::submitFrame();
	}

	void generateQuad()
	{
		std::vector<Vertex> vertices = {
			{{1.0f,1.0f,0.0f},	{1.0f,1.0f},{0.0f,0.0f,1.0f}},
			{{-1.0f,1.0f,0.0f},	{0.0f,1.0f},{0.0f,0.0f,1.0f}},
			{{-1.0f,-1.0f,0.0f},{0.0f,0.0f},{0.0f,0.0f,1.0f}},
			{{1.0f,-1.0f,0.0f},	{1.0f,0.0f},{0.0f,0.0f,1.0f}}
		};

		std::vector<uint32_t> indices = { 0,1,2,  2,3,0 };
		indexCount = static_cast<uint32_t>(indices.size());

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertexBuffer,
			sizeof(Vertex) * vertices.size(), vertices.data());

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &indexBuffer,
			sizeof(uint32_t) * indices.size(), indices.data());
	}

	void setupVertexDescriptions()
	{
		using namespace vks::initializers;

		vertexInputState.bindings = {
			vertexInputBindingDescription(0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX)
		};

		vertexInputState.attrs = {
			vertexInputAttributeDescription(0,0,VK_FORMAT_R32G32B32_SFLOAT,0),
			vertexInputAttributeDescription(0,1,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,uv)),
			vertexInputAttributeDescription(0,2,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,normal))
		};

		vertexInputState.info = pipelineVertexInputStateCreateInfo();
		vertexInputState.info.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputState.bindings.size());
		vertexInputState.info.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputState.attrs.size());
		vertexInputState.info.pVertexBindingDescriptions = vertexInputState.bindings.data();
		vertexInputState.info.pVertexAttributeDescriptions = vertexInputState.attrs.data();
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

	void setupDescriptorSetLayout()
	{
		using namespace vks::initializers;
		VkDescriptorSetLayoutBinding bindings[] = {
			descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT,0),
			descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT,1)
		};

		auto descriptorSetLayoutCI = descriptorSetLayoutCreateInfo(bindings, wws::arrLen<uint32_t>(bindings));
		vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout);

		auto pipelineLayoutCI = pipelineLayoutCreateInfo(&descriptorSetLayout);
		vkCreatePipelineLayout(device, &pipelineLayoutCI,nullptr, &pipelineLayout);
	}

	void setupDescriptorSet()
	{
		auto allocI = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		vkAllocateDescriptorSets(device, &allocI, &descriptorSet);

		auto descriptorImage = vks::initializers::descriptorImageInfo(texture.sampler, texture.imageView, texture.imageLayout);

		VkWriteDescriptorSet writeSets[] = {
			vks::initializers::writeDescriptorSet(descriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0,&uniformBuffer.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSet,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,&descriptorImage)
		};

		vkUpdateDescriptorSets(device, wws::arrLen<uint32_t>(writeSets), writeSets, 0, nullptr);
	}

	void preparePipeline()
	{
		using namespace vks::initializers;

		auto inputAssemblySCI = pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		auto rasterizationSCI = pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
		auto blendAttachment = pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		auto blendSCI = pipelineColorBlendStateCreateInfo(1, &blendAttachment);
		auto depthStencilSCI = pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		auto vpSCI = pipelineViewportStateCreateInfo(1, 1);
		auto multsampleSCI = pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		VkDynamicState dynameicSs[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		auto dynamicSCI = pipelineDynamicStateCreateInfo(dynameicSs, wws::arrLen<uint32_t>(dynameicSs));
		VkPipelineShaderStageCreateInfo shaderStages[] = {
			loadShader(getAssetPath() + "shaders/texture/texture.vert.spv",VK_SHADER_STAGE_VERTEX_BIT),
			loadShader(getAssetPath() + "shaders/texture/texture.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		auto pipelineCI = pipelineCreateInfo(pipelineLayout, renderPass);
		pipelineCI.stageCount = wws::arrLen<uint32_t>(shaderStages);
		pipelineCI.pInputAssemblyState = &inputAssemblySCI;
		pipelineCI.pRasterizationState = &rasterizationSCI;
		pipelineCI.pColorBlendState = &blendSCI;
		pipelineCI.pDepthStencilState = &depthStencilSCI;
		pipelineCI.pViewportState = &vpSCI;
		pipelineCI.pMultisampleState = &multsampleSCI;
		pipelineCI.pDynamicState = &dynamicSCI;
		pipelineCI.pStages = shaderStages;
		pipelineCI.pVertexInputState = &vertexInputState.info;
		VK_CHECK_RESULT( vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.soild) );
	}

	void prepareUniformBuffers()
	{
		vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer,sizeof(uboVS));

		updateUniformBuffer();
	}

	void updateUniformBuffer()
	{

		uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 512.0f);
		glm::mat4 viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, zoom));

		uboVS.model = viewMatrix;
		uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboVS.model = glm::rotate(uboVS.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		uboVS.viewPos = glm::vec4(0.0f, 0.0f, -zoom, 0.0f);

		uniformBuffer.map();

		memcpy(uniformBuffer.mapped, &uboVS, sizeof(uboVS));

		uniformBuffer.unmap();
	}

	void prepare() override
	{
		VulkanExampleBase::prepare();
		loadTexture();
		generateQuad();
		setupVertexDescriptions();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipeline();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared = true;
	}

	void viewChanged() override
	{
		updateUniformBuffer();
	}

	void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
	{
		if (overlay->header("Settings"))
		{
			if (overlay->sliderFloat("LOD bias", &uboVS.lodBias, 0.0f, (float)texture.mipLevels)) {
				updateUniformBuffer();
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

	system("pause");
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
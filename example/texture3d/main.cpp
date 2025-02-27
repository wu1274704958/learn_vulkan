//
// Created by wws on 2019/6/21.
//

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>
#include <VulkanBuffer.hpp>
#include <VulkanDevice.hpp>
#include <comm/CommTool.hpp>
#include <comm/dbg.hpp>
#include <random>
#include <VulkanModel.hpp>
#include "comm/macro.h"


//#define FRACTAL

template <typename T>
class PerlinNoise
{
private:
	uint32_t permutations[512];
	T fade(T t)
	{
		return t * t * t * (t * (t * (T)6 - (T)15) + (T)10);
	}
	T lerp(T t, T a, T b)
	{
		return a + t * (b - a);
	}
	T grad(int hash, T x, T y, T z)
	{
		// Convert LO 4 bits of hash code into 12 gradient directions
		int h = hash & 15;
		T u = h < 8 ? x : y;
		T v = h < 4 ? y : h == 12 || h == 14 ? x : z;
		return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
	}
public:
	PerlinNoise()
	{
		// Generate random lookup for permutations containing all numbers from 0..255
		std::vector<uint8_t> plookup;
		plookup.resize(256);
		std::iota(plookup.begin(), plookup.end(), 0);
		std::default_random_engine rndEngine(std::random_device{}());
		std::shuffle(plookup.begin(), plookup.end(), rndEngine);

		for (uint32_t i = 0; i < 256; i++)
		{
			permutations[i] = permutations[256 + i] = plookup[i];
		}
	}
	T noise(T x, T y, T z)
	{
		// Find unit cube that contains point
		int32_t X = (int32_t)floor(x) & 255;
		int32_t Y = (int32_t)floor(y) & 255;
		int32_t Z = (int32_t)floor(z) & 255;
		// Find relative x,y,z of point in cube
		x -= floor(x);
		y -= floor(y);
		z -= floor(z);

		// Compute fade curves for each of x,y,z
		T u = fade(x);
		T v = fade(y);
		T w = fade(z);

		// Hash coordinates of the 8 cube corners
		uint32_t A = permutations[X] + Y;
		uint32_t AA = permutations[A] + Z;
		uint32_t AB = permutations[A + 1] + Z;
		uint32_t B = permutations[X + 1] + Y;
		uint32_t BA = permutations[B] + Z;
		uint32_t BB = permutations[B + 1] + Z;

		// And add blended results for 8 corners of the cube;
		T res = lerp(w, lerp(v,
			lerp(u, grad(permutations[AA], x, y, z), grad(permutations[BA], x - 1, y, z)), lerp(u, grad(permutations[AB], x, y - 1, z), grad(permutations[BB], x - 1, y - 1, z))),
			lerp(v, lerp(u, grad(permutations[AA + 1], x, y, z - 1), grad(permutations[BA + 1], x - 1, y, z - 1)), lerp(u, grad(permutations[AB + 1], x, y - 1, z - 1), grad(permutations[BB + 1], x - 1, y - 1, z - 1))));
		return res;
	}
};

template <typename T>
class FractalNoise
{
private:
	PerlinNoise<T> perlinNoise;
	uint32_t octaves;
	T frequency;
	T amplitude;
	T persistence;
public:

	FractalNoise(const PerlinNoise<T>& perlinNoise)
	{
		this->perlinNoise = perlinNoise;
		octaves = 6;
		persistence = (T)0.5;
	}

	T noise(T x, T y, T z)
	{
		T sum = 0;
		T frequency = (T)1;
		T amplitude = (T)1;
		T max = (T)0;
		for (int32_t i = 0; i < octaves; i++)
		{
			sum += perlinNoise.noise(x * frequency, y * frequency, z * frequency) * amplitude;
			max += amplitude;
			amplitude *= persistence;
			frequency *= (T)2;
		}

		sum = sum / max;
		return (sum + (T)1.0) / (T)2.0;
	}
};

struct Vertex
{
	glm::vec3 pos;
	glm::vec2 uv;
	glm::vec3 normal;
};

class Example : public VulkanExampleBase {
private:

	struct Texture
	{
		VkSampler sampler = VK_NULL_HANDLE;
		VkImage image = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkDeviceMemory mem = VK_NULL_HANDLE;
		VkDescriptorImageInfo descriptor;
		VkFormat format;
		VkImageLayout layout;
		uint32_t width, height, depth;
		uint32_t mipLevels;
	} texture;

	struct {
		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attrs;
		VkPipelineVertexInputStateCreateInfo sci;
	} visci;

	vks::Buffer vertexBuffer;
	vks::Buffer indexBuffer;
	uint32_t indexCount;
	
	vks::Buffer uniformBuffer;

	struct {
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 viewPos;
		float depth = 0.0f;
	} uboVS;


	struct 
	{
		VkPipeline soild;
	} pipelines;

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet desscriptorSet;
	VkPipelineLayout pipelineLayout;

public:
    void render() override
    {
        if (!prepared)
            return;
        draw();
		if(!paused)
			updateUniformBuffers(false);
		
    }

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

		vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

		VulkanExampleBase::submitFrame();
	}

    Example() : VulkanExampleBase(true)
    {
        zoom = -2.5f;
        title = "3d texture";
        rotation = { 0.0f, 15.0f, 0.0f };
        settings.overlay = true;
		srand(static_cast<uint32_t>(time(0)));
    }

	~Example() {

		destroyTexture(texture);

		vkDestroyPipeline(device, pipelines.soild, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		uniformBuffer.destroy();
		vertexBuffer.destroy();
		indexBuffer.destroy();
	}

	void destroyTexture(Texture tex)
	{
		if(tex.view != VK_NULL_HANDLE)
			vkDestroyImageView(device, tex.view, nullptr);
		if (tex.image != VK_NULL_HANDLE)
			vkDestroyImage(device, tex.image, nullptr);
		if (tex.sampler != VK_NULL_HANDLE)
			vkDestroySampler(device, tex.sampler,nullptr);
		if (tex.mem != VK_NULL_HANDLE)
			vkFreeMemory(device, tex.mem, nullptr);
	}

	void prepareNoiseTexture(uint32_t width, uint32_t height, uint32_t depth)
	{
		using namespace vks::initializers;

		texture.width = width;
		texture.height = height;
		texture.depth = depth;

		texture.mipLevels = 1;

		texture.format = VK_FORMAT_R8_UNORM;

		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, texture.format, &properties);

		if (!properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
		{
			std::cout << "Error : Device not support flag ��Transfer_Dst�� for select format! ";
			return;
		}
		uint32_t maxImageDimension = vulkanDevice->properties.limits.maxImageDimension3D;
		if (width > maxImageDimension || height > maxImageDimension || depth > maxImageDimension)
		{
			std::cout << "Error : Max image dimension is greater Device supported 3d texture dimension!";
			return;
		}

		VkImageCreateInfo imageCI = imageCreateInfo();
		imageCI.arrayLayers = 1;
		imageCI.extent = { width,height,depth };
		imageCI.imageType = VK_IMAGE_TYPE_3D;
		imageCI.mipLevels = 1;
		imageCI.format = texture.format;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		vkCreateImage(device, &imageCI, nullptr, &texture.image);

		VkMemoryAllocateInfo allocateI = memoryAllocateInfo();
		VkMemoryRequirements memReq = {};

		vkGetImageMemoryRequirements(device, texture.image, &memReq);

		allocateI.allocationSize = memReq.size;
		allocateI.memoryTypeIndex = vulkanDevice->getMemoryType(memReq.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		
		vkAllocateMemory(device, &allocateI, nullptr, &texture.mem);

		vkBindImageMemory(device, texture.image, texture.mem,0);

		VkSamplerCreateInfo samplerCI = samplerCreateInfo();
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		samplerCI.anisotropyEnable = VK_FALSE;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerCI.compareEnable = VK_FALSE;
		samplerCI.compareOp = VK_COMPARE_OP_NEVER;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.maxAnisotropy = 1.0f;
		samplerCI.maxLod = 0.0f;
		samplerCI.minLod = 0.0f;
		samplerCI.mipLodBias = 0.0f;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		vkCreateSampler(device, &samplerCI, nullptr, &texture.sampler);

		VkImageViewCreateInfo ivCI = imageViewCreateInfo();
		ivCI.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A,
		};

		ivCI.format = texture.format;
		ivCI.image = texture.image;
		ivCI.viewType = VK_IMAGE_VIEW_TYPE_3D;
		ivCI.subresourceRange = {
			VK_IMAGE_ASPECT_COLOR_BIT,
			0,
			1,
			0,
			1
		};

		vkCreateImageView(device, &ivCI, nullptr, &texture.view);

		texture.descriptor = {
			texture.sampler,
			texture.view,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		updateNoiseTexture();
	}
	

	void updateNoiseTexture()
	{
		using namespace vks::initializers;

		const uint32_t texMemSize = texture.width * texture.height * texture.depth;

		uint8_t* data = new uint8_t[texMemSize];
		memset(data, 0, texMemSize);

		std::cout << "Generating " << texture.width << '*' << texture.height << '*' << texture.depth << " texture ....\n";
		auto tStart = std::chrono::high_resolution_clock::now();
		const float noiseScale = static_cast<float>(rand() % 10) + 4.0f;

		PerlinNoise<float> perlinNoise;
		FractalNoise<float> fractalNoise(perlinNoise);

#pragma omp parallel for
		for (uint32_t z = 0; z < texture.depth; ++z)
		{
			for (uint32_t y = 0; y < texture.height; ++y)
			{
				for (uint32_t x = 0; x < texture.width; ++x)
				{
					float nx = (float)x / (float)texture.width;
					float ny = (float)y / (float)texture.height;
					float nz = (float)z / (float)texture.depth;
#ifdef FRACTAL
					float n = fractalNoise.noise(nx * noiseScale, ny * noiseScale, nz * noiseScale);
#else
					float n = 20.0f  * perlinNoise.noise(nx , ny , nz );
#endif 
					n = n - floor(n);
					data[x + y * texture.width + z * texture.width * texture.height] = static_cast<uint8_t>(floor(255 * n));
				}
			}
		}

		auto tend = std::chrono::high_resolution_clock::now();

		std::cout << "used " << std::chrono::duration_cast<std::chrono::milliseconds>(tend - tStart).count() << " ms\n";

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMem;
		
		VkBufferCreateInfo bufferCI = bufferCreateInfo(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, texMemSize);
		bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		vkCreateBuffer(device, &bufferCI, nullptr, &stagingBuffer);
		VkMemoryAllocateInfo allocI = memoryAllocateInfo();
		VkMemoryRequirements memReq;
		vkGetBufferMemoryRequirements(device, stagingBuffer, &memReq);

		allocI.allocationSize = memReq.size;
		allocI.memoryTypeIndex = vulkanDevice->getMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		vkAllocateMemory(device, &allocI, nullptr, &stagingMem);
		vkBindBufferMemory(device, stagingBuffer, stagingMem, 0);

		void* dst;
		vkMapMemory(device, stagingMem, 0, texMemSize, 0, &dst);
		memcpy(dst, data, texMemSize);
		vkUnmapMemory(device, stagingMem);

		auto copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkImageSubresourceRange subresourceRange = {
			VK_IMAGE_ASPECT_COLOR_BIT,
			0,1,0,1
		};

		vks::tools::setImageLayout(copyCmd, texture.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		VkBufferImageCopy copyi = {};
		copyi.imageExtent = {
			texture.width,
			texture.height,
			texture.depth
		};
		copyi.imageSubresource = {
			VK_IMAGE_ASPECT_COLOR_BIT,
			0,0,1
		};

		vkCmdCopyBufferToImage(copyCmd, stagingBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyi);

		vks::tools::setImageLayout(copyCmd, texture.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

		delete[] data;
		vkFreeMemory(device, stagingMem, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
	}

	void buildCommandBuffers() override
	{
		using namespace vks::initializers;

		VkCommandBufferBeginInfo cmdBI = commandBufferBeginInfo();
		VkClearValue clearVal[2];
		clearVal[0].color = defaultClearColor;
		clearVal[1].depthStencil = { 1.0f,0 };

		VkRenderPassBeginInfo renderPassBI = renderPassBeginInfo();
		renderPassBI.clearValueCount = wws::arrLen(clearVal);
		renderPassBI.pClearValues = clearVal;
		renderPassBI.renderPass = renderPass;
		renderPassBI.renderArea = {
			0,0,width,height
		};

		for (uint32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBI.framebuffer = frameBuffers[i];
			vkBeginCommandBuffer(drawCmdBuffers[i],&cmdBI);
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport vp = viewport((float)width, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &vp);

			VkRect2D scissor = rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &desscriptorSet, 0, nullptr);

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.soild);

			VkDeviceSize offset[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertexBuffer.buffer, offset);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdDrawIndexed(drawCmdBuffers[i], indexCount, 1, 0, 0, 0);

			drawUI(drawCmdBuffers[i]);
			vkCmdEndRenderPass(drawCmdBuffers[i]);
			vkEndCommandBuffer(drawCmdBuffers[i]);
		}
	}

	void generateQuad()
	{
		// Setup vertices for a single uv-mapped quad made from two triangles
		std::vector<Vertex> vertices =
		{
			{ {  1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } },
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } },
			{ {  1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }
		};

		// Setup indices
		std::vector<uint32_t> indices = { 0,1,2, 2,3,0 };
		indexCount = static_cast<uint32_t>(indices.size());

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vertexBuffer, sizeof(Vertex) * static_cast<uint32_t>(vertices.size()), vertices.data());

		vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&indexBuffer, sizeof(uint32_t) * static_cast<uint32_t>(indices.size()),indices.data());
	}

	void setupVertexDescriptions()
	{
		using namespace vks::initializers;

		visci.bindings.resize(1);
		visci.bindings[0] = vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);

		visci.attrs = {
			vertexInputAttributeDescription(0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,pos)),
			vertexInputAttributeDescription(0,1,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,uv)),
			vertexInputAttributeDescription(0,2,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,normal))
		};

		visci.sci = pipelineVertexInputStateCreateInfo();
		visci.sci.vertexBindingDescriptionCount = 1;
		visci.sci.pVertexBindingDescriptions = visci.bindings.data();
		visci.sci.vertexAttributeDescriptionCount = static_cast<uint32_t>(visci.attrs.size());
		visci.sci.pVertexAttributeDescriptions = visci.attrs.data();
	}

	void setupDescriptorPool()
	{
		VkDescriptorPoolSize ss[] = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1)
		};

		auto poolCI = vks::initializers::descriptorPoolCreateInfo(wws::arrLen(ss), ss,2);

		vkCreateDescriptorPool(device, &poolCI, nullptr, &descriptorPool);
	}

	void setupDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding bindings[] = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT,0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT,1)
		};

		VkDescriptorSetLayoutCreateInfo setLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(bindings, wws::arrLen(bindings));

		vkCreateDescriptorSetLayout(device, &setLayoutCI, nullptr, &descriptorSetLayout);

		auto pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

		vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout);
	}

	void setupDescriptorSet()
	{
		using namespace vks::initializers;

		VkDescriptorSetAllocateInfo allocI = descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

		vkAllocateDescriptorSets(device, &allocI, &desscriptorSet);

		VkWriteDescriptorSet ss[] = {
			writeDescriptorSet(desscriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0,&uniformBuffer.descriptor),
			writeDescriptorSet(desscriptorSet,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,&texture.descriptor),
		};

		vkUpdateDescriptorSets(device, wws::arrLen(ss), ss, 0, nullptr);
	}

	void preparePipelines()
	{
		using namespace vks::initializers;

		auto inputAssemblySCI = pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		auto rasterizationSCI = pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		auto colorAttachment = pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		auto colorSCI = pipelineColorBlendStateCreateInfo(1, &colorAttachment);
		auto depthStencilSCI = pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		auto vpSCI = pipelineViewportStateCreateInfo(1, 1);
		VkDynamicState dss[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		auto dynamicSCI = pipelineDynamicStateCreateInfo(dss, wws::arrLen(dss));

		auto multisamplerSCI = pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		
		VkPipelineShaderStageCreateInfo stages[] = {
			loadShader(getAssetPath() + "shaders/texture3d/texture3d.vert.spv" ,VK_SHADER_STAGE_VERTEX_BIT),
			loadShader(getAssetPath() + "shaders/texture3d/texture3d.frag.spv" ,VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		auto pipelineCI = pipelineCreateInfo(pipelineLayout, renderPass);

		pipelineCI.pStages = stages;
		pipelineCI.stageCount = wws::arrLen(stages);
		pipelineCI.pInputAssemblyState = &inputAssemblySCI;
		pipelineCI.pRasterizationState = &rasterizationSCI;
		pipelineCI.pColorBlendState = &colorSCI;
		pipelineCI.pDepthStencilState = &depthStencilSCI;
		pipelineCI.pViewportState = &vpSCI;
		pipelineCI.pDynamicState = &dynamicSCI;
		pipelineCI.pMultisampleState = &multisamplerSCI;
		pipelineCI.pVertexInputState = &visci.sci;

		vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.soild);
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		prepareNoiseTexture(512, 512, 512);
		generateQuad();
		setupVertexDescriptions();
		preparUniformBuffers();
		setupDescriptorPool();
		setupDescriptorSetLayout();
		setupDescriptorSet();
		preparePipelines();
		buildCommandBuffers();
		prepared = true;
	}

	void preparUniformBuffers()
	{
		vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &uniformBuffer, sizeof(uboVS));
		updateUniformBuffers();
	}

	void updateUniformBuffers(bool viewChange = true )
	{
		if (viewChange)
		{
			uboVS.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 512.0f);
			auto model = glm::mat4(1.0f);

			model = glm::translate(model, glm::vec3(0.0f, 0.0f, zoom));
			model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
			model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
			model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
															  
			uboVS.model = model;
			uboVS.viewPos = glm::vec4(0.0f, 0.0f, -zoom, 0.0f);
		}else{
			uboVS.depth += frameTimer * 0.15f;
			if (uboVS.depth > 1.0f)
				uboVS.depth -= 1.0f;
		}

		uniformBuffer.map();
		memcpy(uniformBuffer.mapped, &uboVS, sizeof(uboVS));
		uniformBuffer.unmap();
	}

	void OnUpdateUIOverlay(vks::UIOverlay* overlay) override
	{
		if (overlay->header("Settings"))
		{
			if (overlay->button("generate new"))
			{
				updateNoiseTexture();
			}
		}
	}

	void viewChanged() override
	{
		updateUniformBuffers();
	}
};

EDF_EXAMPLE_MAIN_FUNC(Example, false, {
	int a = 1;
	char buf[9] = {0};
	sprintf(buf,"hhh%d",a);
	MessageBoxA(NULL, buf, "Tip", MB_OK);
})

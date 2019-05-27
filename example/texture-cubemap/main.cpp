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
#include <VulkanModel.hpp>



class Example : public VulkanExampleBase{
public:
	vks::VertexLayout vertexLayout = vks::VertexLayout({
		vks::VERTEX_COMPONENT_POSITION,
		vks::VERTEX_COMPONENT_NORMAL,
		vks::VERTEX_COMPONENT_UV
	});


	void render() override
	{

	}

	Example() : VulkanExampleBase(true)
	{
        zoom = -4.f;
        rotationSpeed = 0.25f;
        rotation = {-7.25f,-120.f,0.f};
        title = "Cube map textures";
        settings.overlay = true;
	}

	~Example()
	{
        vkDestroyImage(device,cubeMap.image, nullptr);
        vkDestroyImageView(device,cubeMap.view, nullptr);
        vkDestroySampler(device,cubeMap.sampler, nullptr);
        vkFreeMemory(device,cubeMap.deviceMemory, nullptr);

        vkDestroyPipeline(device,pipelines.skybox, nullptr);
        vkDestroyPipeline(device,pipelines.reflect, nullptr);

        vkDestroyPipelineLayout(device,pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device,descriptorSetLayout, nullptr);

        for(auto& obj : models.objects)
        {
            obj.destroy();
        }

        models.skybox.destroy();

        uniformBuffer.skybox.destroy();
        uniformBuffer.object.destroy();
	}

    void getEnabledFeatures() override {
        if(deviceFeatures.samplerAnisotropy)
        {
            enabledFeatures.samplerAnisotropy = VK_TRUE;
        }
        if(deviceFeatures.textureCompressionBC)
        {
            enabledFeatures.textureCompressionBC = VK_TRUE;
        }else if(deviceFeatures.textureCompressionASTC_LDR)
        {
            enabledFeatures.textureCompressionASTC_LDR = VK_TRUE;
        }else if(deviceFeatures.textureCompressionETC2)
        {
            enabledFeatures.textureCompressionETC2 = VK_TRUE;
        }
    }

    void loadCubeMap(std::string path,VkFormat format,bool forceLinearTiling = false)
    {
#if defined(__ANDROID__)
        // Textures are stored inside the apk on Android (compressed)
		// So they need to be loaded via the asset manager
		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t size = AAsset_getLength(asset);
		assert(size > 0);

		void *textureData = malloc(size);
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);

		gli::texture_cube texCube(gli::load((const char*)textureData, size));
#else
        gli::texture_cube texCube(gli::load(path));
#endif
        assert(!texCube.empty());
        cubeMap.width = texCube.extent().x;
        cubeMap.height = texCube.extent().y;
        cubeMap.mipLevels = texCube.levels();

        using namespace vks::initializers;

        VkMemoryAllocateInfo memAllocateInfo = memoryAllocateInfo();
        VkMemoryRequirements requirements;

        VkBuffer stagingBuffer;
        VkDeviceMemory  stagingMem;

        VkBufferCreateInfo bufferCI = bufferCreateInfo();
        bufferCI.size = texCube.size();
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        vkCreateBuffer(device,&bufferCI,nullptr,&stagingBuffer);

        vkGetBufferMemoryRequirements(device,stagingBuffer,&requirements);
        memAllocateInfo.allocationSize = requirements.size;
        memAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(requirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device,&memAllocateInfo, nullptr,&stagingMem);
        vkBindBufferMemory(device,stagingBuffer,stagingMem,0);

        //copy to staging buffer
        void *data;
        vkMapMemory(device,stagingMem,0,requirements.size,0,&data);
        memcpy(data,texCube.data(),texCube.size());
        vkUnmapMemory(device,stagingMem);

		VkImageCreateInfo imageCI = imageCreateInfo();
		imageCI.format = format;
		imageCI.mipLevels = cubeMap.mipLevels;
		imageCI.extent = { cubeMap.width,cubeMap.height,1 };
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.arrayLayers = 6;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		//this flag reauired for cube map images;
		imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		vkCreateImage(device, &imageCI, nullptr, &cubeMap.image);

		vkGetImageMemoryRequirements(device, cubeMap.image, &requirements);
		memAllocateInfo.allocationSize = requirements.size;
		memAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkAllocateMemory(device, &memAllocateInfo, nullptr, &cubeMap.deviceMemory);

		vkBindImageMemory(device, cubeMap.image, cubeMap.deviceMemory,0);

		VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		std::vector<VkBufferImageCopy> copyRegions;
		uint32_t offset = 0;
		for (int i = 0; i < 6; ++i)
		{
			for (int l = 0; l < cubeMap.mipLevels; ++l)
			{
				VkBufferImageCopy ic = {};
				ic.imageSubresource.mipLevel = l;
				ic.imageSubresource.baseArrayLayer = i;
				ic.imageSubresource.layerCount = 1;
				ic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				ic.imageExtent.width = texCube[i][l].extent().x;
				ic.imageExtent.height = texCube[i][l].extent().y;
				ic.imageExtent.depth = 1;
				ic.bufferOffset = offset;

				copyRegions.push_back(ic);

				ic.bufferOffset += texCube[i][l].size();
			}
		}
		VkImageSubresourceRange subsourceRange = {};
		subsourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subsourceRange.baseArrayLayer = 0;
		subsourceRange.layerCount = 6;
		subsourceRange.baseMipLevel = 0;
		subsourceRange.levelCount = cubeMap.mipLevels;

		vks::tools::setImageLayout(copyCmd,
			cubeMap.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subsourceRange);

		vkCmdCopyBufferToImage(copyCmd,
			stagingBuffer,
			cubeMap.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(copyRegions.size()),
			copyRegions.data());

		cubeMap.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vks::tools::setImageLayout(copyCmd,
			cubeMap.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			cubeMap.imageLayout,
			subsourceRange
		);

		VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);

		//create sampler
		VkSamplerCreateInfo samplerCI = samplerCreateInfo();
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = samplerCI.addressModeU;
		samplerCI.addressModeW = samplerCI.addressModeU;
		samplerCI.mipLodBias = 0.0f;
		samplerCI.compareOp = VK_COMPARE_OP_NEVER;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = cubeMap.mipLevels;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerCI.maxAnisotropy = 1.0f;
		if (vulkanDevice->enabledFeatures.samplerAnisotropy)
		{
			samplerCI.maxAnisotropy = vulkanDevice->properties.limits.maxSamplerAnisotropy;
			samplerCI.anisotropyEnable = VK_TRUE;
		}

		vkCreateSampler(device, &samplerCI, nullptr, &cubeMap.sampler);

		VkImageViewCreateInfo ivCI = imageViewCreateInfo();
		ivCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		ivCI.format = format;
		ivCI.components = { VK_COMPONENT_SWIZZLE_R,VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B,VK_COMPONENT_SWIZZLE_A };
		ivCI.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT,0,cubeMap.mipLevels,0,6 };
		ivCI.image = cubeMap.image;

		vkCreateImageView(device, &ivCI, nullptr, &cubeMap.view);
    }

	void loadTextures()
	{
		std::string path;
		VkFormat format;
		
		if (deviceFeatures.textureCompressionBC)
		{
			path = "cubemap_yokohama_bc3_unorm.ktx";
			format = VK_FORMAT_BC2_UNORM_BLOCK;
		}
		else if (deviceFeatures.textureCompressionASTC_LDR)
		{
			path = "cubemap_yokohama_astc_8x8_unorm.ktx";
			format = VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
		}
		else if (deviceFeatures.textureCompressionETC2)
		{
			path = "cubemap_yokohama_etc2_unorm.ktx";
			format = VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
		}
		else {
			vks::tools::exitFatal("Device not support any compression texture format!", VK_ERROR_FEATURE_NOT_PRESENT);
		}
		loadCubeMap(getAssetPath() + "textures/" + path, format);
	}

	void buildCommandBuffers() override
	{
		using namespace vks::initializers;

		VkCommandBufferBeginInfo cmdBI = commandBufferBeginInfo();
		
		VkClearValue clearVal[2];
		clearVal[0].color = defaultClearColor;
		clearVal[1].depthStencil = { 1.0f,0 };

		VkRenderPassBeginInfo rpBI = renderPassBeginInfo();
		rpBI.clearValueCount = wws::arrLen<uint32_t>(clearVal);
		rpBI.pClearValues = clearVal;
		rpBI.renderPass = renderPass;
		rpBI.renderArea = { 0,0,width,height };
		for (int i = 0; i < drawCmdBuffers.size(); ++i)
		{
			rpBI.framebuffer = frameBuffers[i];

			vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBI);
			vkCmdBeginRenderPass(drawCmdBuffers[i], &rpBI, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport vp = viewport(width, height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &vp);
			VkRect2D scrssor = rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scrssor);

			VkDeviceSize offset[] = { 0 };
			if (displaySkybox)
			{
				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.skybox, 0, nullptr);
				vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &models.skybox.vertices.buffer, offset);
				vkCmdBindIndexBuffer(drawCmdBuffers[i], models.skybox.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
				vkCmdDrawIndexed(drawCmdBuffers[i], models.skybox.indexCount, 1, 0, 0, 0);
			}

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.object, 0, nullptr);
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &models.objects[models.object_index].vertices.buffer, offset);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], models.objects[models.object_index].indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.reflect);
			vkCmdDrawIndexed(drawCmdBuffers[i], models.objects[models.object_index].indexCount, 1, 0, 0, 0);

			drawUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);
			vkEndCommandBuffer(drawCmdBuffers[i]);
		}
	}

	void loadAssets()
	{
		models.skybox.loadFromFile(getAssetPath() + "models/cube.obj", vertexLayout, 0.05f, vulkanDevice, queue);

		std::vector<std::string> filenames = { "sphere.obj", "teapot.dae", "torusknot.obj", "venus.fbx" };
		objectNames = { "Sphere", "Teapot", "Torusknot", "Venus" };
		for (auto file : filenames) {
			vks::Model model;
			model.loadFromFile(getAssetPath() + "models/" + file, vertexLayout, 0.05f * (file == "venus.fbx" ? 3.0f : 1.0f), vulkanDevice, queue);
			models.objects.push_back(model);
		}
	}

private:
	bool displaySkybox = true;
	vks::Texture cubeMap;

	struct {
		vks::Model skybox;
		std::vector<vks::Model> objects;
		int32_t object_index = 0;
	} models;

	struct {
		vks::Buffer skybox;
		vks::Buffer object;
	} uniformBuffer;

	struct {
		glm::mat4 projection = glm::mat4(1.f);
		glm::mat4 model = glm::mat4(1.f);
		float lodBias = 0.0f;
	} uboVS;

	struct {
		VkPipeline skybox;
		VkPipeline reflect;
	} pipelines;

	struct {
		VkDescriptorSet skybox;
		VkDescriptorSet object;
	} descriptorSets;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;

	std::vector<std::string> objectNames;
};

#if defined(_WIN32)

Example *example;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (example != nullptr)
	{
		example->handleMessages(hWnd, uMsg, wParam, lParam);
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	/*for (size_t i = 0; i < __argc; i++) { Example::args.push_back(__argv[i]); };
	example = new Example();
	example->initVulkan();
	example->setupWindow(hInstance, WndProc);
	example->prepare();
	example->renderLoop();
	delete example;*/
	MessageBoxA(nullptr, "ss", "zzzzzz", MB_OK);
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
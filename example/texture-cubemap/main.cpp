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

    void loadCubeMap(std::string& path,VkFormat format,bool forceLinearTiling = false)
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
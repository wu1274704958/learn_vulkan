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

public:
	void render() override
	{

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

		texture.mipLevels = static_cast<uint32_t>(tex2D.levels);
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


		}
		else {

		}
	}

	void buildCommandBuffers() override
	{

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
	/*for (size_t i = 0; i < __argc; i++) { Example::args.push_back(__argv[i]); };
	example = new Example();
	example->initVulkan();
	example->setupWindow(hInstance, WndProc);
	example->prepare();
	example->renderLoop();
	delete example;*/

	MessageBoxA(NULL, "sssssss", "tip", MB_OK);
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
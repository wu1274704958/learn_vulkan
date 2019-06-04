//
// Created by wws on 2019/6/4.
//
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>
#include <VulkanBuffer.hpp>
#include <VulkanDevice.hpp>
#include <comm/CommTool.hpp>
#include <comm/dbg.hpp>
#include <VulkanTexture.hpp>

struct Vertex{
    glm::vec3 pos;
    glm::vec2 uv;
};

class Example : public VulkanExampleBase{
public:

    void render() override {
        if(prepared)
            draw();
    }
    void draw()
    {
        VulkanExampleBase::prepareFrame();
        submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
        submitInfo.commandBufferCount = 1;
        vkQueueSubmit(queue,1,&submitInfo,VK_NULL_HANDLE);
        VulkanExampleBase::submitFrame();
    }
    Example() : VulkanExampleBase(true) {
        zoom = -15.0f;
        rotationSpeed = 0.25f;
        rotation = {-15.0f,35.f,0.f};
        title = "texture array";
        settings.overlay = true;
    }
    ~Example() {
        vkDestroyImageView(device,texture.view, nullptr);
        vkDestroyImage(device,texture.image, nullptr);
        vkDestroySampler(device,texture.sampler, nullptr);
        vkFreeMemory(device,texture.deviceMemory, nullptr);

        vkDestroyPipeline(device,pipeline, nullptr);
        vkDestroyPipelineLayout(device,pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device,descriptorSetLayout, nullptr);

        uniformBuffer.destroy();
        vertexBuffer.destroy();
        indexBuffer.destroy();

        delete[] uboVS.instance;
    }

    void loadTexture(std::string& path,VkFormat format)
    {
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

		gli::texture2d_array tex2DArray(gli::load((const char*)textureData, size));
#else
        gli::texture2d_array tex2DArray(gli::load(path));
#endif
    }

private:
    uint32_t layer_count = 0;
    vks::Texture texture;

    struct{
        std::vector<VkVertexInputBindingDescription> bindings;
        std::vector<VkVertexInputAttributeDescription> attrs;
        VkPipelineVertexInputStateCreateInfo inputStateCreateInfo;
    } vertexInputStatus;

    vks::Buffer vertexBuffer;
    vks::Buffer indexBuffer;
    vks::Buffer uniformBuffer;
    uint32_t index_count;

    struct UboInstanceData
    {
        glm::mat4 model;
        uint32_t arrayIndex;
    };

    struct {
        glm::mat4 projection;
        glm::mat4 view;
        UboInstanceData *instance;
    } uboVS;

    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet  descriptorSet;
    VkPipeline  pipeline;
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
//    for (size_t i = 0; i < __argc; i++) { Example::args.push_back(__argv[i]); };
//    example = new Example();
//    example->initVulkan();
//    example->setupWindow(hInstance, WndProc);
//    example->prepare();
//    example->renderLoop();
//    delete example;
    MessageBoxA(nullptr,"ssssss","tip",MB_OK);
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
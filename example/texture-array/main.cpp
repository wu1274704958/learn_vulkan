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

        delete[] instance_ptr;
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
        assert(!tex2DArray.empty());
        texture.width = tex2DArray.extent().x;
        texture.height = tex2DArray.extent().y;
        layer_count = tex2DArray.layers();

        using namespace vks::initializers;

        VkMemoryAllocateInfo memAllocI = memoryAllocateInfo();
        VkMemoryRequirements memReq;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMem;

        VkBufferCreateInfo bufferCI = bufferCreateInfo();
        bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferCI.size = tex2DArray.size();

        vkCreateBuffer(device,&bufferCI, nullptr,&stagingBuffer);
        vkGetBufferMemoryRequirements(device,stagingBuffer,&memReq);

        memAllocI.allocationSize = memReq.size;
        memAllocI.memoryTypeIndex = vulkanDevice->getMemoryType(memReq.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device,&memAllocI, nullptr,&stagingMem);
        vkBindBufferMemory(device,stagingBuffer,stagingMem,0);

        void *data;
        vkMapMemory(device,stagingMem,0,memReq.size,0,&data);
        memcpy(data,tex2DArray.data(),tex2DArray.size());
        vkUnmapMemory(device,stagingMem);

        std::vector<VkBufferImageCopy> bufferCopyRegions;
        uint32_t offset = 0;
        for(uint32_t i = 0;i < tex2DArray.layers();++i)
        {
            VkBufferImageCopy copy = {};
            copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.baseArrayLayer = i;
            copy.imageSubresource.layerCount = 1;
            copy.imageSubresource.mipLevel = 0;

            copy.imageExtent.width = tex2DArray[i][0].extent().x;
            copy.imageExtent.height = tex2DArray[i][0].extent().y;
            copy.imageExtent.depth = 1;

            copy.bufferOffset = offset;
            bufferCopyRegions.push_back(copy);
            offset += tex2DArray[i][0].size();
        }

        VkImageCreateInfo imageCI = imageCreateInfo();
        imageCI.extent = {texture.width,texture.height,1};
        imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageCI.format = format;
        imageCI.mipLevels = 1;
        imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.arrayLayers = tex2DArray.layers();
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkCreateImage(device,&imageCI, nullptr,&texture.image);

        vkGetImageMemoryRequirements(device,texture.image,&memReq);
        memAllocI.allocationSize = memReq.size;
        memAllocI.memoryTypeIndex = vulkanDevice->getMemoryType(memReq.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkAllocateMemory(device,&memAllocI, nullptr,&texture.deviceMemory);

        vkBindImageMemory(device,texture.image,texture.deviceMemory,0);

        auto copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY,true);

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.layerCount = layer_count;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;

        vks::tools::setImageLayout(copyCmd,
                texture.image,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                subresourceRange);

        vkCmdCopyBufferToImage(copyCmd,
                stagingBuffer,
                texture.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                bufferCopyRegions.size(),
                bufferCopyRegions.data());
        vks::tools::setImageLayout(copyCmd,
                texture.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                subresourceRange);

        texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VulkanExampleBase::flushCommandBuffer(copyCmd,queue, true);

        VkSamplerCreateInfo samplerCI = samplerCreateInfo();
        samplerCI.minFilter = VK_FILTER_LINEAR;
        samplerCI.magFilter = VK_FILTER_LINEAR;
        samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.compareOp = VK_COMPARE_OP_NEVER;
        samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerCI.anisotropyEnable = 8;

        vkCreateSampler(device,&samplerCI, nullptr,&texture.sampler);

        VkImageViewCreateInfo viewCI = imageViewCreateInfo();
        viewCI.image = texture.image;
        viewCI.format = format;
        viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewCI.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B,VK_COMPONENT_SWIZZLE_A};
        viewCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,layer_count};

        vkCreateImageView(device,&viewCI, nullptr,&texture.view);

        vkFreeMemory(device,stagingMem, nullptr);
        vkDestroyBuffer(device,stagingBuffer, nullptr);
    }

    void loadTextures()
    {
        std::string filename = getAssetPath() + "textures/";
        VkFormat format;
        if (deviceFeatures.textureCompressionBC) {
            filename += "texturearray_bc3_unorm.ktx";
            format = VK_FORMAT_BC3_UNORM_BLOCK;
        }
        else if (deviceFeatures.textureCompressionASTC_LDR) {
            filename += "texturearray_astc_8x8_unorm.ktx";
            format = VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
        }
        else if (deviceFeatures.textureCompressionETC2) {
            filename += "texturearray_etc2_unorm.ktx";
            format = VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        }
        else {
            vks::tools::exitFatal("Device does not support any compressed texture format!",
                                  VK_ERROR_FEATURE_NOT_PRESENT);
        }
        loadTexture(filename, format);
    }

    void buildCommandBuffers() override {
        using namespace vks::initializers;
        VkCommandBufferBeginInfo cmdBeginI = commandBufferBeginInfo();

        VkClearValue clearVal[2];
        clearVal[0].color = defaultClearColor;
        clearVal[1].depthStencil = { 1.0f,0 };

        VkRenderPassBeginInfo renderPassBI = renderPassBeginInfo();
        renderPassBI.renderPass = renderPass;
        renderPassBI.clearValueCount = wws::arrLen(clearVal);
        renderPassBI.pClearValues = clearVal;
        renderPassBI.renderArea = { {0,0} ,{width,height } };
        for(uint32_t i = 0;i < drawCmdBuffers.size();++i)
        {
            renderPassBI.framebuffer = frameBuffers[i];
            vkBeginCommandBuffer(drawCmdBuffers[i],&cmdBeginI);
            vkCmdBeginRenderPass(drawCmdBuffers[i],&renderPassBI,VK_SUBPASS_CONTENTS_INLINE);

            VkViewport vp = viewport((float)width,(float)height,0.0f,1.0f);
            vkCmdSetViewport(drawCmdBuffers[i],0,1,&vp);
            VkRect2D scissor = rect2D(width,height,0,0);
            vkCmdSetScissor(drawCmdBuffers[i],0,1,&scissor);

            vkCmdBindDescriptorSets(drawCmdBuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&descriptorSet,0,
                                    nullptr);
            vkCmdBindPipeline(drawCmdBuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
            VkDeviceSize offset[] = {0};
            vkCmdBindVertexBuffers(drawCmdBuffers[i],0,1,&vertexBuffer.buffer,offset);
            vkCmdBindIndexBuffer(drawCmdBuffers[i],indexBuffer.buffer,0,VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(drawCmdBuffers[i],index_count,layer_count,0,0,0);

            drawUI(drawCmdBuffers[i]);
            vkCmdEndRenderPass(drawCmdBuffers[i]);
            vkEndCommandBuffer(drawCmdBuffers[i]);
        }
    }

    void generateQuad()
    {
        std::vector<Vertex> vertices ={
                { {  2.5f,  2.5f, 0.0f }, { 1.0f, 1.0f } },
                { { -2.5f,  2.5f, 0.0f }, { 0.0f, 1.0f } },
                { { -2.5f, -2.5f, 0.0f }, { 0.0f, 0.0f } },
                { {  2.5f, -2.5f, 0.0f }, { 1.0f, 0.0f } }
        };

        // Setup indices
        std::vector<uint32_t> indices = { 0,1,2, 2,3,0 };
        index_count = static_cast<uint32_t>(indices.size());

        vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                &vertexBuffer,
                sizeof(Vertex) * vertices.size(),
                vertices.data());
        vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                &indexBuffer,
                sizeof(uint32_t) * indices.size(),
                indices.data()
        );
    }

    void setupVertexDescription()
    {
        using namespace vks::initializers;
        vertexInputStatus.bindings = {
                vertexInputBindingDescription(0, sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX)
        };
        vertexInputStatus.attrs = {
                vertexInputAttributeDescription(0,0,VK_FORMAT_R32G32B32_SFLOAT,0),
                vertexInputAttributeDescription(0,1,VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 3)
        };
        vertexInputStatus.inputStateCreateInfo = pipelineVertexInputStateCreateInfo();
        vertexInputStatus.inputStateCreateInfo.pVertexBindingDescriptions = vertexInputStatus.bindings.data();
        vertexInputStatus.inputStateCreateInfo.vertexBindingDescriptionCount = vertexInputStatus.bindings.size();
        vertexInputStatus.inputStateCreateInfo.pVertexAttributeDescriptions = vertexInputStatus.attrs.data();
        vertexInputStatus.inputStateCreateInfo.vertexAttributeDescriptionCount = vertexInputStatus.attrs.size();
    }

    void setupDescriptorPool()
    {
        using namespace vks::initializers;
        VkDescriptorPoolSize sizes[] = {
             descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1),
             descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1)
        };
        VkDescriptorPoolCreateInfo poolCI = descriptorPoolCreateInfo(wws::arrLen(sizes),sizes,2);
        vkCreateDescriptorPool(device,&poolCI, nullptr,&descriptorPool);
    }

    void setupDescriptorSetLayout()
    {
        using namespace vks::initializers;
        VkDescriptorSetLayoutBinding binding[] = {
                descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT,0),
                descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT,1)
        };

        VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = descriptorSetLayoutCreateInfo(binding,wws::arrLen(binding));
        VkPipelineLayoutCreateInfo pipelineLCI = pipelineLayoutCreateInfo(&descriptorSetLayout);

		vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &descriptorSetLayout);
        vkCreatePipelineLayout(device,&pipelineLCI, nullptr,&pipelineLayout);
    }

    void setupDescriptorSet()
    {
        using namespace vks::initializers;
        VkDescriptorSetAllocateInfo setAllocateInfo = descriptorSetAllocateInfo(descriptorPool,&descriptorSetLayout,1);
        vkAllocateDescriptorSets(device,&setAllocateInfo,&descriptorSet);

		VkDescriptorImageInfo descriptor = {};
		descriptor.imageLayout = texture.imageLayout;
		descriptor.imageView = texture.view;
		descriptor.sampler = texture.sampler;

        VkWriteDescriptorSet ws[] = {
                writeDescriptorSet(descriptorSet,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,0,&uniformBuffer.descriptor),
                writeDescriptorSet(descriptorSet,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,&descriptor)
        };
        vkUpdateDescriptorSets(device,wws::arrLen(ws),ws,0, nullptr);
    }

    void preparePipeline()
    {
        using namespace vks::initializers;
        auto inputAssemblySCI = pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                0,VK_FALSE);
        auto colorAttachment = pipelineColorBlendAttachmentState(0xf,VK_FALSE);
        auto colorSCI  = pipelineColorBlendStateCreateInfo(1,&colorAttachment);
        auto depthSCI = pipelineDepthStencilStateCreateInfo(VK_TRUE,VK_TRUE,VK_COMPARE_OP_LESS_OR_EQUAL);
        auto rasterizationSCI = pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL,VK_CULL_MODE_NONE,
                VK_FRONT_FACE_COUNTER_CLOCKWISE);
        auto vpSCI = pipelineViewportStateCreateInfo(1,1);
        VkDynamicState states[] = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
        };
        auto dynamicSCI = pipelineDynamicStateCreateInfo(states,wws::arrLen(states));
        auto multsamplerSCI = pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
        VkPipelineShaderStageCreateInfo stageCreateInfos[2];

        stageCreateInfos[0] = loadShader(getAssetPath() + "shaders/texturearray/instancing.vert.spv",VK_SHADER_STAGE_VERTEX_BIT);
        stageCreateInfos[1] = loadShader(getAssetPath() + "shaders/texturearray/instancing.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT);

        VkGraphicsPipelineCreateInfo pipelineCI = pipelineCreateInfo(pipelineLayout,renderPass);
        pipelineCI.pStages = stageCreateInfos;
        pipelineCI.stageCount = wws::arrLen(stageCreateInfos);
        pipelineCI.pVertexInputState = &vertexInputStatus.inputStateCreateInfo;
        pipelineCI.pViewportState = &vpSCI;
        pipelineCI.pDynamicState = &dynamicSCI;
        pipelineCI.pMultisampleState = &multsamplerSCI;
        pipelineCI.pDepthStencilState = &depthSCI;
        pipelineCI.pRasterizationState = &rasterizationSCI;
        pipelineCI.pInputAssemblyState = &inputAssemblySCI;
        pipelineCI.pColorBlendState = &colorSCI;

        vkCreateGraphicsPipelines(device,pipelineCache,1,&pipelineCI, nullptr,&pipeline);
    }

    void prepareUniformBuffers()
    {
        instance_ptr = new UboInstanceData[layer_count];

        VkDeviceSize size = sizeof(uboVS) + sizeof(UboInstanceData) * layer_count;
        vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &uniformBuffer, size);
        float offset = 1.5f;
        float begin = (layer_count * offset) / -2.0f;

        for(uint32_t i = 0;i < layer_count;++i)
        {
            instance_ptr[i].arrayIndex = i;
            glm::mat4 model(1.0f);
            model = glm::translate(model,glm::vec3(0.0f,begin + i * offset,0.0f));
            model = glm::rotate(model,glm::radians(60.0f),glm::vec3(1.0f,0.0f,0.0f));
            instance_ptr[i].model = model;
        }

        uniformBuffer.map(size, sizeof(uboVS));
        memcpy(instance_ptr,uniformBuffer.mapped, sizeof(UboInstanceData) * layer_count);
        uniformBuffer.unmap();

        updateUniformBuffer_matrix();
    }

    void updateUniformBuffer_matrix()
    {
        uboVS.projection = glm::perspective(glm::radians(60.f),(float)width/(float)height,0.01f,512.f);
        glm::mat4 view = glm::mat4(1.0f);
        view = glm::translate(view,glm::vec3(0.0f,0.0f,zoom));
        view = glm::rotate(view,rotation.x,glm::vec3(1.0f,0.0f,0.0f));
        view = glm::rotate(view,rotation.y,glm::vec3(0.0f,1.0f,0.0f));
        view = glm::rotate(view,rotation.z,glm::vec3(0.0f,0.0f,1.0f));

        uniformBuffer.map(sizeof(uboVS));
        memcpy(uniformBuffer.mapped,&uboVS, sizeof(uboVS));
        uniformBuffer.unmap();
    }

    void viewChanged() override {
        updateUniformBuffer_matrix();
    }

    void prepare() override {
        VulkanExampleBase::prepare();
        loadTextures();
        setupVertexDescription();
        generateQuad();
        prepareUniformBuffers();
        setupDescriptorSetLayout();
        preparePipeline();
        setupDescriptorPool();
        setupDescriptorSet();
        buildCommandBuffers();
        prepared = true;
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
    } uboVS;

    UboInstanceData * instance_ptr;

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
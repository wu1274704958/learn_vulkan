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


#define FRACTAL

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
		glm::vec3 viewPos;
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
    }

	void draw()
	{

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
			std::cout << "Error : Device not support flag ¡®Transfer_Dst¡¯ for select format! ";
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
//    for (size_t i = 0; i < __argc; i++) { Example::args.push_back(__argv[i]); };
//    example = new Example();
//    example->initVulkan();
//    example->setupWindow(hInstance, WndProc);
//    example->prepare();
//    example->renderLoop();
//    delete example;
	auto perNoise = PerlinNoise<float>();
	auto res = perNoise.noise(0.5f, 0.5f, 0.0f);
	std::string r;
	r.reserve(200);
	sprintf(r.data(), "res = %f", res);
	MessageBoxA(0, r.data(), "tip", 0);
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

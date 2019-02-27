#define GLFW_INCLUDE_VULKAN
#ifdef WIN32
#include <Windows.h>
#endif
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <fstream>
#include <numeric>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <chrono>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static uint16_t WIDTH = 500;
static uint16_t HEIGHT = 400;

#define APPNAME "Demo1"

const std::vector<const char *> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

const std::vector<const char *> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription description = {};
		description.binding = 0;
		description.stride = sizeof(Vertex);
		description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		
		return description;
	}

	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescription()
	{
		std::array<VkVertexInputAttributeDescription,2> attributeDescriptions = {};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);
		return attributeDescriptions;
	}
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

std::vector<uint16_t> indices = {
	 0, 1, 2, 2, 3, 0
};

struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentFamily = -1;
	bool isComplete()
	{
		return graphicsFamily >= 0 && presentFamily >= 0;
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentMods;
	SwapChainSupportDetails() = default;
	SwapChainSupportDetails(const SwapChainSupportDetails &) = default;
	SwapChainSupportDetails(SwapChainSupportDetails && other)
	{
		capabilities = other.capabilities;
		formats = std::move(other.formats);
		presentMods = std::move(other.presentMods);
	}
};

#ifdef NDEBUG 
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif
VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback)
{
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT pCallback, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != nullptr)
	{
		return func(instance, pCallback, pAllocator);
	}
}

class Demo {
public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanUp();
	}
private:
	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(WIDTH, HEIGHT, APPNAME, nullptr, nullptr);

		glfwSetWindowUserPointer(window, this);

		glfwSetWindowSizeCallback(window, WindowReSize);
	}
	void initVulkan() {
		createInstance();
		setUpDebugCallback();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createFrameBuffers();
		createCommandPool();
		createTextureImage();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffer();
		createDescriptorPool();
		createDescriptorSet();
		createCommandBuffers();
		createSemaphores();
	}
	void drawFrame() {
		uint32_t imageIndex;
		VkResult res = vkAcquireNextImageKHR(device, swapChain, std::numeric_limits<uint32_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

		if (res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapChain();
		}
		else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to present swap chain image!");
		}

		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = waitSemaphores;
		submit_info.pWaitDstStageMask = waitStages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &commandBuffers[imageIndex];
		
		VkSemaphore signSemaphores[] = { renderFinishedSemaphore };
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signSemaphores;
		if (vkQueueSubmit(graphicsQueue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit command buffer!");
		}
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		res = vkQueuePresentKHR(presentQueue, &presentInfo);
		if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
			recreateSwapChain();
		}
		else if (res != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		vkQueueWaitIdle(presentQueue);
	}
	void mainLoop() {
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			updateUniformBuffer();
			drawFrame();
		}
		vkDeviceWaitIdle(device);
	}
	void recreateSwapChain()
	{
		vkDeviceWaitIdle(device);

		cleanUpSwapChain();

		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFrameBuffers();
		createCommandBuffers();
	}
	void cleanUpSwapChain()
	{
		vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
		for (auto fb : swapChainFrameBuffers)
		{
			vkDestroyFramebuffer(device, fb, nullptr);
		}

		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

		for (int i = 0; i < swapChainImageViews.size(); ++i)
		{
			vkDestroyImageView(device, swapChainImageViews[i], nullptr);
		}

		vkDestroySwapchainKHR(device, swapChain, nullptr);
	}
	void cleanUp() {
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
		cleanUpSwapChain();

		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		vkFreeMemory(device, uniformBufferMem, nullptr);
		vkDestroyBuffer(device, uniformBuffer, nullptr);

		vkFreeMemory(device, indexBufferMem, nullptr);
		vkDestroyBuffer(device, indexBuffer, nullptr);

		vkFreeMemory(device, vertexBufferMem, nullptr);
		vkDestroyBuffer(device, vertexBuffer, nullptr);

		vkDestroyDescriptorPool(device, descriptorPool, nullptr);

		vkDestroyCommandPool(device, commandPool, nullptr);
		
		vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
		DestroyDebugReportCallbackEXT(instance, debugReport, nullptr);
		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window);
		glfwTerminate();
	}
	void createInstance()
	{
		if (enableValidationLayers && !checkValidationLayerSupport())
		{
			throw std::runtime_error("Enable ValidationLayer but not support!");
		}
		VkApplicationInfo app_info = {};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = APPNAME;
		app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
		app_info.pEngineName = "NO Engine";
		app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
		app_info.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo instance_info = {};
		instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_info.pApplicationInfo = &app_info;

		auto extensions = getRequiredExtensions();

		instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		instance_info.ppEnabledExtensionNames = extensions.data();

		if (enableValidationLayers)
		{
			instance_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			instance_info.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			instance_info.enabledLayerCount = 0;
		}
		if (VK_SUCCESS != vkCreateInstance(&instance_info, nullptr, &instance))
		{
			throw std::runtime_error("Create instance failed!");
		}
	}
	std::vector<const char *> getRequiredExtensions()
	{
		uint32_t count;
		const char ** requiredExtensions;
		requiredExtensions = glfwGetRequiredInstanceExtensions(&count);

		std::vector<const char *> extensions;
		extensions.reserve(count);

		for (int i = 0; i < count; ++i)
		{
			extensions.push_back(requiredExtensions[i]);
		}
		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
		return extensions;
	}

	bool checkValidationLayerSupport() {
		uint32_t count;
		vkEnumerateInstanceLayerProperties(&count, nullptr);
		std::vector<VkLayerProperties> ils(count);


		vkEnumerateInstanceLayerProperties(&count, ils.data());

		for (const char *p : validationLayers)
		{
			bool has = false;
			for (const auto& lp : ils)
			{
				if (strcmp(p, lp.layerName) == 0)
				{
					has = true;
					break;
				}
			}
			if (!has)
			{
				return false;
			}
		}
		return true;
	}
	void setUpDebugCallback()
	{
		if (!enableValidationLayers)
			return;
		VkDebugReportCallbackCreateInfoEXT callback_info = {};
		callback_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		callback_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		callback_info.pfnCallback = Demo::debugCallback;
		if (CreateDebugReportCallbackEXT(instance, &callback_info, nullptr, &debugReport) != VK_SUCCESS)
		{
			throw std::runtime_error("create Debug Report Callback failed!");
		}
	}
	void createSurface()
	{
		if (VK_SUCCESS != glfwCreateWindowSurface(instance, window, nullptr, &surface))
		{
			throw std::runtime_error("create surface failed!");
		}
	}
	void pickPhysicalDevice()
	{
		uint32_t count;
		vkEnumeratePhysicalDevices(instance, &count, nullptr);
		if (!count)
			throw std::runtime_error("failed to find a physical device with vulkan support!");
		std::vector<VkPhysicalDevice> devices(count);
		vkEnumeratePhysicalDevices(instance, &count, devices.data());
		for (const auto d : devices)
		{
			if (isDeviceSuitable(d))
			{
				physicalDevice = d;
				break;
			}
		}
		if (physicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}

	bool isDeviceSuitable(const VkPhysicalDevice d)
	{
		QueueFamilyIndices indices = findQueueFamilies(d);

		bool extensionSupport = checkDeviceExtensionSupport(d);
		bool swapChainAdequate = false;
		if (extensionSupport)
		{
			SwapChainSupportDetails details = querySwapChainSupport(d);
			swapChainAdequate = !(details.formats.empty() && details.presentMods.empty());
		}
		return indices.isComplete() && extensionSupport && swapChainAdequate;
	}

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice d)
	{
		QueueFamilyIndices indices;
		uint32_t queueFamilyCount;

		vkGetPhysicalDeviceQueueFamilyProperties(d, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> qfps(queueFamilyCount);

		vkGetPhysicalDeviceQueueFamilyProperties(d, &queueFamilyCount, qfps.data());
		int i = 0;
		for (const auto &qfp : qfps)
		{
			if (qfp.queueCount > 0 && qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}
			VkBool32 supportPresent = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(d, i, surface, &supportPresent);
			if (qfp.queueCount > 0 && supportPresent)
			{
				indices.presentFamily = i;
			}
			if (indices.isComplete())
				break;
			++i;
		}
		return indices;
	}
	bool checkDeviceExtensionSupport(VkPhysicalDevice d)
	{
		uint32_t count;
		vkEnumerateDeviceExtensionProperties(d, nullptr, &count, nullptr);

		std::vector<VkExtensionProperties> eps(count);
		vkEnumerateDeviceExtensionProperties(d, nullptr, &count, eps.data());

		int sameCount = 0;

		for (const char *p : deviceExtensions)
		{
			for (const auto &ep : eps)
			{
				if (strcmp(p, ep.extensionName) == 0)
				{
					++sameCount;
				}
			}
			if (sameCount == deviceExtensions.size())
				break;
		}
		return sameCount == deviceExtensions.size();
	}
	void createLogicalDevice()
	{
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		std::set<int> uniqueQueueFamily = { queueFamilyIndices.graphicsFamily,queueFamilyIndices.presentFamily };
		std::vector<VkDeviceQueueCreateInfo> deviceQueue_info;

		float queuePriority = 1.0f;

		for (int i : uniqueQueueFamily)
		{
			VkDeviceQueueCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			info.queueFamilyIndex = i;
			info.queueCount = 1;
			info.pQueuePriorities = &queuePriority;

			deviceQueue_info.push_back(info);
		}

		VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
		VkDeviceCreateInfo device_info = {};

		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		device_info.queueCreateInfoCount = static_cast<uint32_t>(deviceQueue_info.size());
		device_info.pQueueCreateInfos = deviceQueue_info.data();

		device_info.pEnabledFeatures = &physicalDeviceFeatures;

		device_info.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		device_info.ppEnabledExtensionNames = deviceExtensions.data();

		if (enableValidationLayers)
		{
			device_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			device_info.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			device_info.enabledLayerCount = 0;
		}
		if (vkCreateDevice(physicalDevice, &device_info, nullptr, &device) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical device!");
		}
		vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily, 0, &graphicsQueue);
		vkGetDeviceQueue(device, queueFamilyIndices.presentFamily, 0, &presentQueue);
	}

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice d)
	{
		SwapChainSupportDetails res;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(d, surface, &res.capabilities);

		uint32_t count = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(d, surface, &count, nullptr);
		if (count > 0)
		{
			res.formats.resize(count);
			vkGetPhysicalDeviceSurfaceFormatsKHR(d, surface, &count, res.formats.data());
		}

		vkGetPhysicalDeviceSurfacePresentModesKHR(d, surface, &count, nullptr);
		if (count > 0)
		{
			res.presentMods.resize(count);
			vkGetPhysicalDeviceSurfacePresentModesKHR(d, surface, &count, res.presentMods.data());
		}
		return res;
	}

	void createSwapChain()
	{
		SwapChainSupportDetails swapChainSupportDetails = querySwapChainSupport(physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupportDetails.formats);
		VkPresentModeKHR presentMode = chooseSwapPresent(swapChainSupportDetails.presentMods);
		VkExtent2D extent = chooseSwapExtent(swapChainSupportDetails.capabilities);

		uint32_t imageCount = swapChainSupportDetails.capabilities.minImageCount + 1;
		if (swapChainSupportDetails.capabilities.maxImageCount > 0 && imageCount > swapChainSupportDetails.capabilities.minImageCount)
			imageCount = swapChainSupportDetails.capabilities.maxImageCount;

		VkSwapchainCreateInfoKHR swapChain_info = {};

		swapChain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapChain_info.surface = surface;

		swapChain_info.minImageCount = imageCount;
		swapChain_info.imageFormat = surfaceFormat.format;
		swapChain_info.imageColorSpace = surfaceFormat.colorSpace;
		swapChain_info.imageExtent = extent;
		swapChain_info.imageArrayLayers = 1;
		swapChain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices qfs = findQueueFamilies(physicalDevice);
		uint32_t qfis[] = { qfs.graphicsFamily,qfs.presentFamily };
		if (qfs.graphicsFamily != qfs.presentFamily)
		{
			swapChain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapChain_info.queueFamilyIndexCount = 2;
			swapChain_info.pQueueFamilyIndices = qfis;
		}
		else {
			swapChain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}
		swapChain_info.preTransform = swapChainSupportDetails.capabilities.currentTransform;
		swapChain_info.presentMode = presentMode;
		swapChain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapChain_info.clipped = VK_TRUE;

		swapChain_info.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device, &swapChain_info, nullptr, &swapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);

		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats)
	{
		if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
			return { VK_FORMAT_R8G8B8A8_UNORM , VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		for (const auto &f : formats)
		{
			if (f.format == VK_FORMAT_R8G8B8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return f;
		}
		return formats[0];
	}
	VkPresentModeKHR chooseSwapPresent(const std::vector<VkPresentModeKHR> &presentMods)
	{
		VkPresentModeKHR best = VK_PRESENT_MODE_FIFO_KHR;
		for (const auto &m : presentMods)
		{
			if (m == VK_PRESENT_MODE_MAILBOX_KHR)
				return m;
			else if (m == VK_PRESENT_MODE_IMMEDIATE_KHR)
				best = m;
		}
		return best;
	}
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else {
			VkExtent2D actualExent = { WIDTH, HEIGHT };
			actualExent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExent.width));
			actualExent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExent.height));

			return actualExent;
		}
	}
	void createImageViews()
	{
		swapChainImageViews.resize(swapChainImages.size());

		for (int i = 0; i < swapChainImageViews.size(); ++i)
		{
			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = swapChainImages[i];
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.format = swapChainImageFormat;
			info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			info.subresourceRange.baseMipLevel = 0;
			info.subresourceRange.levelCount = 1;
			info.subresourceRange.baseArrayLayer = 0;
			info.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device, &info, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create image views!");
			}
		}
	}
	std::vector<char> readFile(const char *path)
	{
		std::vector<char> res;
		std::ifstream fs(path, std::ios::ate | std::ios::binary);
		if (!fs.is_open())
		{
			throw std::runtime_error("failed to read file!");
		}
		size_t fileSize = (size_t)fs.tellg();

		fs.seekg(0);
		res.resize(fileSize);
		fs.read(res.data(), fileSize);
		fs.close();
		return res;
	}
	void createRenderPass()
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference ref = {};
		ref.attachment = 0;
		ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &ref;

		VkRenderPassCreateInfo renderPass_info = {};
		renderPass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPass_info.attachmentCount = 1;
		renderPass_info.pAttachments = &colorAttachment;
		renderPass_info.subpassCount = 1;
		renderPass_info.pSubpasses = &subpass;

		if (vkCreateRenderPass(device, &renderPass_info, nullptr, &renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create render pass!");
		}
	}
	void createGraphicsPipeline()
	{
#ifdef  WIN32
		auto vsCode = readFile("../../shader_23/vert.spv");
		auto fgCode = readFile("../../shader_23/frag.spv");
#else
		auto vsCode = readFile("../shader_23/vert.spv");
		auto fgCode = readFile("../shader_23/frag.spv");
#endif
		VkShaderModule vsModule = createShaderModule(vsCode);
		VkShaderModule fgModule = createShaderModule(fgCode);

		VkPipelineShaderStageCreateInfo vsStageInfo = {};
		vsStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vsStageInfo.module = vsModule;
		vsStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vsStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fgStageInfo = {};
		fgStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fgStageInfo.module = fgModule;
		fgStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fgStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vsStageInfo,fgStageInfo };

		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescription = Vertex::getAttributeDescription();

		VkPipelineVertexInputStateCreateInfo vertexInputState_info = {};
		vertexInputState_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputState_info.vertexBindingDescriptionCount = 1;
		vertexInputState_info.pVertexBindingDescriptions = &bindingDescription;
		vertexInputState_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
		vertexInputState_info.pVertexAttributeDescriptions = attributeDescription.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState_info = {};//������� ָ��ͼԪ
		inputAssemblyState_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyState_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyState_info.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0.f;
		viewport.y = 0.f;
		viewport.width = (float)swapChainExtent.width;
		viewport.height = (float)swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.f;

		VkRect2D scissor = { };
		scissor.offset = { 0,0 };
		scissor.extent = swapChainExtent;

		VkPipelineViewportStateCreateInfo viewPort_info = {};
		viewPort_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewPort_info.pViewports = &viewport;
		viewPort_info.viewportCount = 1;
		viewPort_info.pScissors = &scissor;
		viewPort_info.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterization_info = {};
		rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_info.depthClampEnable = VK_FALSE;
		rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization_info.lineWidth = 1.0f;
		rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterization_info.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisample_info = {};
		multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_info.sampleShadingEnable = VK_FALSE;
		multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState colorBlendState = {};
		colorBlendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendState.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlend_info = {};
		colorBlend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend_info.logicOpEnable = VK_FALSE;
		colorBlend_info.logicOp = VK_LOGIC_OP_COPY;
		colorBlend_info.attachmentCount = 1;
		colorBlend_info.pAttachments = &colorBlendState;
		colorBlend_info.blendConstants[0] = 0.0f;
		colorBlend_info.blendConstants[1] = 0.0f;
		colorBlend_info.blendConstants[2] = 0.0f;
		colorBlend_info.blendConstants[3] = 0.0f;

		VkPipelineLayoutCreateInfo pipelineLayout_info = {};
		pipelineLayout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayout_info.setLayoutCount = 1;
		pipelineLayout_info.pSetLayouts = &descriptorSetLayout;

		if (vkCreatePipelineLayout(device, &pipelineLayout_info, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}

		
		VkGraphicsPipelineCreateInfo graphicsPipeline_info = {};
		graphicsPipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		graphicsPipeline_info.stageCount = 2;
		graphicsPipeline_info.pStages = shaderStages;
		graphicsPipeline_info.pVertexInputState = &vertexInputState_info;
		graphicsPipeline_info.pInputAssemblyState = &inputAssemblyState_info;
		graphicsPipeline_info.pViewportState = &viewPort_info;
		graphicsPipeline_info.pRasterizationState = &rasterization_info;
		graphicsPipeline_info.pColorBlendState = &colorBlend_info;
		graphicsPipeline_info.pMultisampleState = &multisample_info;
		graphicsPipeline_info.layout = pipelineLayout;
		graphicsPipeline_info.renderPass = renderPass;
		graphicsPipeline_info.subpass = 0;

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipeline_info, nullptr, &graphicsPipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create graphics pipeline");
		}

		vkDestroyShaderModule(device, vsModule, nullptr);
		vkDestroyShaderModule(device, fgModule, nullptr);

	}
	VkShaderModule createShaderModule(std::vector<char> &code)
	{
		VkShaderModule res;

		VkShaderModuleCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = static_cast<uint32_t>(code.size());
		info.pCode = reinterpret_cast<const uint32_t *>(code.data());

		if (vkCreateShaderModule(device, &info, nullptr, &res) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create Shader Module!");
		}
		return res;
	}
	void createFrameBuffers()
	{
		swapChainFrameBuffers.resize(swapChainImageViews.size());
		int i = 0;
		for (auto swapChainImageView : swapChainImageViews)
		{
			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = renderPass;
			info.attachmentCount = 1;
			info.pAttachments = &swapChainImageView;
			info.width = swapChainExtent.width;
			info.height = swapChainExtent.height;
			info.layers = 1;

			if (vkCreateFramebuffer(device, &info, nullptr, &swapChainFrameBuffers[i++]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create frame buffer!");
			}
		}
	}
	void createCommandPool()
	{
		auto queueFamilyIndices = findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		info.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

		if (vkCreateCommandPool(device, &info, nullptr, &commandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create command pool !!");
		}
	}
	void createCommandBuffers()
	{
		commandBuffers.resize(swapChainFrameBuffers.size());

		VkCommandBufferAllocateInfo allocateInfo = {};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = commandPool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = commandBuffers.size();
		if (vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffers!");
		}
		int i = 0;
		for (auto cb : commandBuffers)
		{
			VkCommandBufferBeginInfo commandBufferBeginInfo = {};
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			commandBufferBeginInfo.pInheritanceInfo = nullptr;

			if (vkBeginCommandBuffer(cb, &commandBufferBeginInfo) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to begin command buffer!");
			}
			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.framebuffer = swapChainFrameBuffers[i];
			renderPassBeginInfo.renderArea.offset = { 0,0 };
			renderPassBeginInfo.renderArea.extent = swapChainExtent;

			VkClearValue clearVar = VkClearValue{ 0.0f,0.0f,0.0f,1.0f };
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = &clearVar;
			vkCmdBeginRenderPass(cb, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			VkBuffer vertexBuffers[] = { vertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT16);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

			//vkCmdDraw(cb, static_cast<uint32_t>( vertices.size() ), 1, 0, 0);
			vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

			vkCmdEndRenderPass(cb);

			if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
				throw std::runtime_error("failed to record command buffer!");
			}
			i++;
		}
	}
	void createSemaphores()
	{
		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS  ){
				throw std::runtime_error("failed to create synchronization objects for a frame!");
		}
	}

	void createVertexBuffer()
	{
		VkDeviceSize size = sizeof(vertices[0]) * vertices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMem;

		createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingMem);

		void *data = nullptr;
		vkMapMemory(device, stagingMem, 0, size, 0, &data);
		memcpy(data, vertices.data(), size);
		vkUnmapMemory(device, stagingMem);

		createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertexBuffer, vertexBufferMem
		);

		copyBuffer(stagingBuffer, vertexBuffer, size);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingMem, nullptr);
	}
	void createIndexBuffer()
	{
		VkDeviceSize size = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMem;
		createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingMem);

		void *data = nullptr;
		vkMapMemory(device, stagingMem, 0, size, 0, &data);
		memcpy(data, indices.data(), size);
		vkUnmapMemory(device, stagingMem);
		
		createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			indexBuffer, indexBufferMem);

		copyBuffer(stagingBuffer, indexBuffer, size);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingMem, nullptr);
	}
	void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
	{
		
		auto cb = beginSingleTimeCommands();
		VkBufferCopy copyRegion = {};
		copyRegion.size = size;
		vkCmdCopyBuffer(cb, src, dst, 1, &copyRegion);
		endSingleTimeCommands(cb);
	}

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if (typeFilter & (1 << i) && ((memProperties.memoryTypes[i].propertyFlags & properties) == properties) )
			{
				return i;
			}
		}
		throw std::runtime_error("failed to find memory type!");
	}
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProperty, VkBuffer &buffer, VkDeviceMemory &mem)
	{
		VkBufferCreateInfo buffer_info = {};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = size;
		buffer_info.usage = usage;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		
		if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create buffer!");
		}

		VkMemoryRequirements memRequirments;
		vkGetBufferMemoryRequirements(device, buffer, &memRequirments);

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = size;
		alloc_info.memoryTypeIndex = findMemoryType(memRequirments.memoryTypeBits, memProperty);

		if (vkAllocateMemory(device, &alloc_info, nullptr, &mem) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate memory!");
		}
		vkBindBufferMemory(device, buffer, mem, 0);
	}

	void createDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding setLayoutBinging = {};
		setLayoutBinging.binding = 0;
		setLayoutBinging.descriptorCount = 1;
		setLayoutBinging.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		setLayoutBinging.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo setLayout_info = {};
		setLayout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setLayout_info.bindingCount = 1;
		setLayout_info.pBindings = &setLayoutBinging;

		if (vkCreateDescriptorSetLayout(device, &setLayout_info, nullptr, &descriptorSetLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create Descriptor Set Layout!");
		}
		
	}

	void createUniformBuffer()
	{
		VkDeviceSize size = sizeof(UniformBufferObject);

		createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			uniformBuffer, uniformBufferMem);

	}
	void updateUniformBuffer()
	{
		static auto start = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - start).count() / 1000.f;

		UniformBufferObject ubo = {};
		ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj[1][1] *= -1;
		void *data = nullptr;
		vkMapMemory(device, uniformBufferMem, 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(device, uniformBufferMem);
	}
	void createDescriptorPool()
	{
		VkDescriptorPoolSize poolSize = {};
		poolSize.descriptorCount = 1;
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.poolSizeCount = 1;
		pool_info.pPoolSizes = &poolSize;
		
		pool_info.maxSets = 1;
		if (vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	void createDescriptorSet()
	{
		VkDescriptorSetAllocateInfo descriptorSetAlloc_info = {};
		descriptorSetAlloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAlloc_info.descriptorPool = descriptorPool;
		descriptorSetAlloc_info.descriptorSetCount = 1;
		descriptorSetAlloc_info.pSetLayouts = &descriptorSetLayout;

		if (vkAllocateDescriptorSets(device, &descriptorSetAlloc_info, &descriptorSet) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate descriptor set!");
		}
		VkDescriptorBufferInfo descriptorBuffer_info = {};
		descriptorBuffer_info.buffer = uniformBuffer;
		descriptorBuffer_info.offset = 0;
		descriptorBuffer_info.range = sizeof(UniformBufferObject);

		VkWriteDescriptorSet writeDescriptSet = {};
		writeDescriptSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptSet.dstSet = descriptorSet;
		writeDescriptSet.dstBinding = 0;
		writeDescriptSet.dstArrayElement = 0;
		writeDescriptSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptSet.descriptorCount = 1;
		writeDescriptSet.pBufferInfo = &descriptorBuffer_info;

		vkUpdateDescriptorSets(device, 1, &writeDescriptSet, 0, nullptr);
	}
	void createTextureImage()
	{
		int texWidth, texHeight, texChannel;
#ifdef WIN32
		stbi_uc* pixels =  stbi_load("../../textures/texture.jpg", &texWidth, &texHeight, &texChannel, STBI_rgb_alpha);
#else
		stbi_uc* pixels =  stbi_load("../textures/texture.jpg", &texWidth, &texHeight, &texChannel, STBI_rgb_alpha);
#endif
		if (!pixels)
		{
			throw std::runtime_error("failed to load image!");
		}
		VkDeviceSize size = texWidth * texHeight * 4;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMem;
		createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			stagingBuffer, stagingBufferMem
		);

		void *data = nullptr;
		vkMapMemory(device, stagingBufferMem, 0, size, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(size));
		vkUnmapMemory(device, stagingBufferMem);

		stbi_image_free(pixels);
		
		createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			, textureImag, textureImageMem
		);
		
	}
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
	{
		VkImageCreateInfo image_info = {};
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.extent = { static_cast<uint32_t>(width),static_cast<uint32_t>(height),1 };
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.format = format;
		image_info.tiling = tiling;
		image_info.usage = usage;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.samples = VK_SAMPLE_COUNT_1_BIT;

		if (vkCreateImage(device, &image_info, nullptr, &textureImag) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create texture image!");
		}

		VkMemoryRequirements memRequirement;
		vkGetImageMemoryRequirements(device, textureImag, &memRequirement);

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = memRequirement.size;
		alloc_info.memoryTypeIndex = findMemoryType(memRequirement.memoryTypeBits, properties);

		if (vkAllocateMemory(device, &alloc_info, nullptr, &textureImageMem) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate texture image memory!");
		}

		vkBindImageMemory(device, textureImag, textureImageMem, 0);
	}

	VkCommandBuffer beginSingleTimeCommands()
	{
		VkCommandBufferAllocateInfo cbAlloc_info = {};
		cbAlloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cbAlloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cbAlloc_info.commandPool = commandPool;
		cbAlloc_info.commandBufferCount = 1;

		VkCommandBuffer cb;
		if (vkAllocateCommandBuffers(device, &cbAlloc_info, &cb) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffer!");
		}

		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(cb, &begin_info);
		return cb;
	}
	void endSingleTimeCommands(VkCommandBuffer cb)
	{
		vkEndCommandBuffer(cb);

		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cb;

		vkQueueSubmit(graphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
		vkDeviceWaitIdle(device);
		vkFreeCommandBuffers(device, commandPool, 1, &cb);
	}

	GLFWwindow *window;
	VkInstance instance;
	VkDebugReportCallbackEXT debugReport;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;
	VkQueue graphicsQueue, presentQueue;
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;
	VkPipeline graphicsPipeline;
	std::vector<VkFramebuffer> swapChainFrameBuffers;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMem;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMem;
	VkDescriptorSetLayout descriptorSetLayout;
	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMem;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;
	VkImage textureImag;
	VkDeviceMemory textureImageMem;
public:
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData) {
		std::cerr << "validation layer: " << msg << std::endl;

		return VK_FALSE;
	}
	static void WindowReSize(GLFWwindow* window, int w, int h)
	{
		if (w == 0 || h == 0)
			return ;
		WIDTH = w;
		HEIGHT = h;
		Demo *d = reinterpret_cast<Demo *>(glfwGetWindowUserPointer(window));
		d->recreateSwapChain();
	}
};

int main()
{
	{
		Demo d;
		try {
			d.run();
		}
		catch (const std::runtime_error& e)
		{
			std::cout << e.what();
#ifdef WIN32
			system("pause");
#endif
			return -1;
		}
	}
#ifdef WIN32
	system("pause");
#endif
	return 0;
}
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <glm/glm.hpp>
#include <array>

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

std::vector<Vertex> vertices = {
	{{0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}},
	{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
	{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
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
		createGraphicsPipeline();
		createFrameBuffers();
		createCommandPool();
		createVertexBuffer();
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
			drawFrame();
		}
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
		vkFreeMemory(device, vertexBufferMem, nullptr);
		vkDestroyBuffer(device, vertexBuffer, nullptr);
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
		auto vsCode = readFile("shader_19/vert.spv");
		auto fgCode = readFile("shader_19/frag.spv");

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

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState_info = {};//输入组件 指定图元
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
		rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

		VkPipelineLayoutCreateInfo layout_info = {};
		layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_info.pushConstantRangeCount = 0;
		layout_info.setLayoutCount = 0;

		if (vkCreatePipelineLayout(device, &layout_info, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create Pipeline Layout!");
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

			vkCmdDraw(cb, static_cast<uint32_t>( vertices.size() ), 1, 0, 0);

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
		VkBufferCreateInfo buffer_info = {};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = sizeof(vertices[0]) * vertices.size();

		buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateBuffer(device, &buffer_info, nullptr, &vertexBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create vertex buffer!");
		}

		VkMemoryRequirements memRequirements = {};
		vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

		VkMemoryAllocateInfo  memAlloc_info = {};
		memAlloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAlloc_info.allocationSize = memRequirements.size;
		memAlloc_info.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (vkAllocateMemory(device, &memAlloc_info, nullptr, &vertexBufferMem) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate vertex buffer memory!");
		}
	
		vkBindBufferMemory(device, vertexBuffer, vertexBufferMem, 0);

		void *data = nullptr;
		vkMapMemory(device, vertexBufferMem, 0, buffer_info.size, 0, &data);
		memcpy(data, vertices.data(), buffer_info.size);
		vkUnmapMemory(device, vertexBufferMem);
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
	Demo d;
	try {
		d.run();
	}
	catch (const std::runtime_error& e)
	{
		std::cout << e.what();
		system("pause");
		return -1;
	}
	system("pause");
	return 0;
}
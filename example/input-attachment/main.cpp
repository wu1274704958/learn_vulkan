#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vulkan/vulkan.h>
#include <vulkanexamplebase.h>
#include <VulkanBuffer.hpp>
#include <VulkanDevice.hpp>
#include <comm/CommTool.hpp>
#include <comm/dbg.hpp>
#include "comm/macro.h"
#include <VulkanTexture.hpp>

class Example : public VulkanExampleBase
{
private:
	void render() override
	{
		if (prepared)
		{
			draw();
			/*if(camera.updated)*/
		}
	}
	void draw()
	{

	}
};

EDF_EXAMPLE_MAIN_FUNC(Example, true, {
	int a = 1;
	char buf[9] = {0};
	sprintf(buf,"hhh%d",a);
	MessageBoxA(NULL, buf, "Tip", MB_OK);
})
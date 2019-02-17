#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>

#define WIDTH 800
#define HEIGHT 600
#define APP_NAME "cube"

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

		window = glfwCreateWindow(WIDTH, HEIGHT, APP_NAME, nullptr, nullptr);

		glfwSetWindowUserPointer(window, this);

		glfwSetWindowSizeCallback(window, WindowReSize);
	}

	void initVulkan()
	{

	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			drawFrame();
		}
	}

	void drawFrame()
	{

	}

	void cleanUp()
	{

	}
private:
	GLFWwindow *window;
	static void WindowReSize(GLFWwindow *window, int w, int h);
};

void Demo::WindowReSize(GLFWwindow *window, int w, int h)
{

}


int main()
{
	Demo demo;
	demo.run();
	return 0;
}
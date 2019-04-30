#include "DrawVec3Demo.hpp"

class Example : public DrawVec3Demo {

	void init_vec() override
	{
		auto xy = DrawVec3(glm::vec3(1.0f, 1.0f, 0.0f));
		xy.set_color(xy.get_vec());
		dvs.push_back(xy);
		dvs.push_back(DrawVec3(glm::vec3(1.0f, 1.0f, 1.0f)));
		dvs.push_back(DrawVec3(glm::vec3(1.0f, 1.0f, -2.0f)));
	}

	void keyPressed(uint32_t code) override
	{
		switch (code)
		{
		case 'A':
			glm::mat4 mat(1.0f);
			mat = glm::rotate(mat, rotatex, glm::vec3(0.0f, 0.0f, 1.0f));
			glm::vec4 v = mat * glm::vec4(1.0f, 1.0f, -2.0f, 1.0f);
			dvs[4].set_vec(glm::vec3(v.x, v.y, v.z));
			//dvs[4].set_pos(glm::vec3( 1.0f,0.0f,0.0f ));
			if (dvs[4].get_line_width() == 1.0f)
			{
				dvs[4].set_line_width(3.0f);
				buildCommandBuffers();
			}
			updateVertexBuffer(4);
			rotatex += 0.1;
			break;
		case 'S':
			dvs[4].set_pos(dvs[4].get_vec());
			updateVertexBuffer(4);
			break;
		}
	}
};

	

#if defined(_WIN32)

Example* example;
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
	for (size_t i = 0; i < __argc; i++) { Example::args.push_back(__argv[i]); };
	example = new Example();
	example->initVulkan();
	example->setupWindow(hInstance, WndProc);
	example->prepare();
	example->renderLoop();
	delete example;
	/*DrawVec3 dv(glm::vec3(1.0f, 1.0f, 0.0f));

	std::vector<DrawVec3::Vertex> res = dv.build_vertices();
	std::string str;
	for (auto& a : res)
	{
		char buf[100] = { 0 };
		sprintf(buf,"%f %f %f\n", a.pos.x, a.pos.y, a.pos.z);
		str += buf;
	}

	MessageBox(NULL, str.data(), "tip", MB_OK);*/
	return 0;
}

#elif defined(__linux__)

// Linux entry point
Example* example;
static void handleEvent(const xcb_generic_event_t* event)
{
	if (example != NULL)
	{
		example->handleEvent(event);
	}
}
int main(const int argc, const char* argv[])
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
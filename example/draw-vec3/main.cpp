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

class Example2 : public DrawVec3Demo {

	float angle = 0.0f;
	glm::vec3 ww1;
	glm::vec3 ww2;

	void init_vec() override
	{
		DrawVec3 dv = DrawVec3(glm::vec3(2.0f, 3.0f, 4.0f));
		dv.set_line_width(1.5f);
		dv.set_color(glm::vec3(0.5f, 0.0f, 0.5f));
		dv.set_arrow_rate(0.06f);
		dvs.push_back(dv);
		dvs.push_back(dv);

		auto v1 = glm::vec3(2.0f, 3.0f, 4.0f);
		float len = glm::length(v1);
		auto w = glm::cross(v1, glm::vec3(0.0f, 1.0f, 0.0f));
		
		ww1 = v1;
		auto w2 = glm::cross(v1, w);
		float rate = len / glm::length(w2);
		ww2 = rate * w2;
	}
	void update_vec() override 
	{
		auto vec = ww1 * glm::cos(angle) + ww2 * glm::sin(angle);
		dvs[3].set_vec(vec);
		angle += 0.001f;
		if (angle >= glm::two_pi<float>())
			angle = 0.0f;
		updateVertexBuffer(3);
	}
};

	

#if defined(_WIN32)

DrawVec3Demo* example;
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
	example = new Example2();
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
DrawVec3Demo* example;
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
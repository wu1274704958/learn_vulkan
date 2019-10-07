#pragma once


#if defined(_WIN32)

#define EDF_EXAMPLE_MAIN_FUNC(Example,is_test,...)							\
Example* example;															\
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)\
{																			\
	if (example != NULL)													\
	{																		\
		example->handleMessages(hWnd, uMsg, wParam, lParam);				\
	}																		\
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));						\
}																			\
																			\
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)	\
{																									\
	if constexpr(!is_test)																			\
	{																								\
		for (size_t i = 0; i < __argc; i++) { Example::args.push_back(__argv[i]); };				\
		example = new Example();																	\
		example->initVulkan();																		\
		example->setupWindow(hInstance, WndProc);													\
		example->prepare();																			\
		example->renderLoop();																		\
		delete example;																				\
	}																								\
	else {																							\
		__VA_ARGS__																					\
	}																								\
	return 0;																						\
}																									\

#elif defined(__linux__)

#define EDF_EXAMPLE_MAIN_FUNC(Example,is_test,...)													\
Example* example;																					\
static void handleEvent(const xcb_generic_event_t* event)											\
{																									\
	if (example != NULL)																			\
	{																								\
		example->handleEvent(event);																\
	}																								\
}																									\
int main(const int argc, const char* argv[])														\
{																									\
	if constexpr(!is_test)																			\
	{																								\
		for (size_t i = 0; i < argc; i++) { Example::args.push_back(argv[i]); };					\
		example = new Example();																	\
		example->initVulkan();																		\
		example->setupWindow();																		\
		example->prepare();																			\
		example->renderLoop();																		\
		delete example;																				\
	}																								\
	else {																							\
		__VA_ARGS__																					\
	}																								\
	return 0;																						\
}																									
#endif



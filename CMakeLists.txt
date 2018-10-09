cmake_minimum_required(VERSION 3.2)
project(learn_vulkan)

add_compile_options("/std:c++17")

include_directories(D:/MyLib/1.1.82.1/Include D:/MyLib/glfw-3.2.1/include D:/MyLib/glm-0.9.6.3/glm D:/drop/vulkan-tutorial/3rd_party/others/stb)

link_directories(D:/MyLib/1.1.82.1/lib32  D:/MyLib/glfw-3.2.1/build/src/Release)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} glfw3.lib vulkan-1.lib)



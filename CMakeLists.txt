cmake_minimum_required(VERSION 3.2)
project(learn_vulkan)

if(WIN32)

    add_compile_options("/std:c++17")

else()

    add_compile_options("-std:c++17")

endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

SET(BASE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/3rd_party/base")
SET(IMGUI_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/3rd_party/imgui")

if(WIN32)

add_definitions(-DVK_EXAMPLE_DATA_DIR=\"${CMAKE_SOURCE_DIR}/data/\")
add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
SET(BASE_LIBRARY "${CMAKE_SOURCE_DIR}/build/x64-Release/3rd_party/base/base.lib")

endif()


find_package(GLM)
find_package(GLFW3)
find_package(VULKAN)
find_package(ASSIMP)

message("${GLFW3_FOUND} ${VULKAN_FOUND} ${GLM_FOUND}")
add_subdirectory(3rd_party/base)
add_subdirectory(test_libbase)

if( GLFW3_FOUND AND VULKAN_FOUND AND GLM_FOUND)
	
	message(OK!)
	message(${GLM_INCLUDE_DIR})
    include_directories(${GLFW3_INCLUDE_DIR} ${VULKAN_INCLUDE_DIR} ${GLM_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rd_party/stb)
    add_executable(${PROJECT_NAME} main.cpp)
    target_link_libraries(${PROJECT_NAME} ${GLFW3_LIBRARY} ${VULKAN_LIBRARY})

else()

message("Failed!")

endif()






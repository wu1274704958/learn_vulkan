cmake_minimum_required(VERSION 3.2)
project(learn_vulkan)

if(WIN32)

    add_compile_options("/std:c++17")
	add_compile_options("/permissive-")

else()

    add_compile_options("-std=c++17")
	set(USE_D2D_WSI TRUE)

endif()

include_directories(${CMAKE_SOURCE_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

SET(BASE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/3rd_party/base")
SET(IMGUI_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/3rd_party/imgui")
SET(GLI_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/3rd_party/gli")
add_definitions(-DVK_EXAMPLE_DATA_DIR=\"${CMAKE_SOURCE_DIR}/data/\")

if(WIN32)

	add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)

	if(${CMAKE_BUILD_TYPE} STREQUAL "Release" )
		find_library( BASE_LIBRARY base "${CMAKE_SOURCE_DIR}/build/x64-Release/3rd_party/base")
		if(NOT BASE_LIBRARY)
			set(BASE_LIBRARY "${CMAKE_SOURCE_DIR}/build/x64-Release/3rd_party/base/base.lib")
		endif()
	else()
		find_library( BASE_LIBRARY base "${CMAKE_SOURCE_DIR}/build/x64-Debug/3rd_party/base")
		if(NOT BASE_LIBRARY)
			set(BASE_LIBRARY "${CMAKE_SOURCE_DIR}/build/x64-Debug/3rd_party/base/base.lib")
		endif()
	endif()

elseif(USE_D2D_WSI)

	if(${CMAKE_BUILD_TYPE} STREQUAL "Release" )
		find_library( BASE_LIBRARY base "${CMAKE_SOURCE_DIR}/cmake-build-release/3rd_party/base")
		if(NOT BASE_LIBRARY)
			set(BASE_LIBRARY "${CMAKE_SOURCE_DIR}/cmake-build-release/3rd_party/base/libbase.a")
		endif()
	else()
		find_library( BASE_LIBRARY base "${CMAKE_SOURCE_DIR}/cmake-build-debug/3rd_party/base")
		if(NOT BASE_LIBRARY)
			set(BASE_LIBRARY "${CMAKE_SOURCE_DIR}/cmake-build-debug/3rd_party/base/libbase.a")
		endif()
	endif()

	
	message("USE_D2D_WSI = TRUE")
	find_package(XCB REQUIRED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XCB_KHR")

endif()

message("BASE_LIBRARY = ${BASE_LIBRARY}")

find_package(GLM)
find_package(GLFW3)
find_package(VULKAN)
find_package(ASSIMP)

message("${GLFW3_FOUND} ${VULKAN_FOUND} ${GLM_FOUND}")
add_subdirectory(3rd_party/base)
add_subdirectory(test_libbase)
add_subdirectory(triangle)
add_subdirectory(pipelines)
add_subdirectory(example/descriptorSets)
add_subdirectory(example/dynamic-uniform-buffer)
add_subdirectory(example/push-constants)
add_subdirectory(example/specialization-constants)
add_subdirectory(example/draw-vec3)
add_subdirectory(example/texture)
add_subdirectory(example/texture-cubemap)

if( GLFW3_FOUND AND VULKAN_FOUND AND GLM_FOUND)
	
	message(OK!)
	message(${GLM_INCLUDE_DIR})
    include_directories(${GLFW3_INCLUDE_DIR} ${VULKAN_INCLUDE_DIR} ${GLM_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rd_party/stb)
    add_executable(${PROJECT_NAME} main.cpp)
    target_link_libraries(${PROJECT_NAME} ${GLFW3_LIBRARY} ${VULKAN_LIBRARY})

else()

message("Failed!")

endif()






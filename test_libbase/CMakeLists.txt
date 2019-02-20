file(GLOB SOURCE_FILES "*.cpp")
set(BUILD_NAME "test_libbase")
add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
include_directories(${BASE_INCLUDE_DIR} ${IMGUI_INCLUDE_DIR})

if( GLFW3_FOUND AND VULKAN_FOUND AND GLM_FOUND)
	
	message(OK!)
	message(${GLM_INCLUDE_DIR})
    include_directories(${GLFW3_INCLUDE_DIR} ${VULKAN_INCLUDE_DIR} ${GLM_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rd_party/stb)
    add_executable(${BUILD_NAME} ${SOURCE_FILES})
    add_dependencies(test_libbase base)
    target_link_libraries(${BUILD_NAME} ${GLFW3_LIBRARY} ${VULKAN_LIBRARY} ${BASE_LIBRARY})

else()

message("Failed!")

endif()

set(SOURCE_FILES cube.cpp)
set(BUILD_NAME "cube")

if( GLFW3_FOUND AND VULKAN_FOUND AND GLM_FOUND)
	
	message(OK!)
	message(${GLM_INCLUDE_DIR})
    include_directories(${GLFW3_INCLUDE_DIR} ${VULKAN_INCLUDE_DIR} ${GLM_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rd_party/stb)
    add_executable(${BUILD_NAME} ${SOURCE_FILES})
    target_link_libraries(${BUILD_NAME} ${GLFW3_LIBRARY} ${VULKAN_LIBRARY})

else()

message("Failed!")

endif()
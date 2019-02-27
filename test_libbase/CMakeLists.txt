file(GLOB SOURCE_FILES "*.cpp")
set(BUILD_NAME "test_libbase")
include_directories(${BASE_INCLUDE_DIR} ${IMGUI_INCLUDE_DIR})

if(VULKAN_FOUND AND GLM_FOUND)
	
	message(OK!)
	message(${GLM_INCLUDE_DIR})
    include_directories(${VULKAN_INCLUDE_DIR} ${GLM_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rd_party/stb)
    add_executable(${BUILD_NAME} ${SOURCE_FILES})
    add_dependencies(${BUILD_NAME} base)
    target_link_libraries(${BUILD_NAME} ${VULKAN_LIBRARY} ${BASE_LIBRARY})

else()

message("Failed!")

endif()
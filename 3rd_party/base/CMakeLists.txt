file(GLOB BASE_SRC "*.cpp" "../imgui/*.cpp")
file(GLOB BASE_HEADERS "*.hpp")

message("${VULKAN_FOUND}  ${ASSIMP_FOUND}")
message(${BASE_SRC})

include_directories(${IMGUI_INCLUDE_DIR}) 
if(WIN32)
    add_library(base STATIC ${BASE_SRC})
    target_link_libraries(base glfw Vulkan::Vulkan glm::glm )
 else(WIN32)
    add_library(base STATIC ${BASE_SRC})
    target_link_libraries(base glfw Vulkan::Vulkan glm::glm ${XCB_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif(WIN32)


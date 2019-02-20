file(GLOB BASE_SRC "*.cpp" "../imgui/*.cpp")
file(GLOB BASE_HEADERS "*.hpp")

message("${VULKAN_FOUND}  ${ASSIMP_FOUND}")
message(${BASE_SRC})

if(GLM_FOUND AND VULKAN_FOUND AND ASSIMP_FOUND)

    include_directories(${VULKAN_INCLUDE_DIR} ${ASSIMP_INCLUDE_DIR} ${GLM_INCLUDE_DIR} ${IMGUI_INCLUDE_DIR}) 

    if(WIN32)
        add_library(base STATIC ${BASE_SRC})
        target_link_libraries(base ${VULKAN_LIBRARY} ${ASSIMP_LIBRARIE} )
     else(WIN32)
        add_library(base STATIC ${BASE_SRC})
        target_link_libraries(base ${VULKAN_LIBRARY} ${ASSIMP_LIBRARIE} ${XCB_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
    endif(WIN32)

endif()
file(GLOB SOURCE_FILES "*.cpp")
set(BUILD_NAME "texture-array")
include_directories(${BASE_INCLUDE_DIR} ${IMGUI_INCLUDE_DIR})

if(USE_D2D_WSI)

    if(VULKAN_FOUND AND GLM_FOUND AND XCB_FOUND )

        message(OK!)
        message(${GLM_INCLUDE_DIR})
        include_directories(${VULKAN_INCLUDE_DIR} ${GLM_INCLUDE_DIR} ${XCB_INCLUDE_DIRS} ${GLI_INCLUDE_DIR})
        add_executable(${BUILD_NAME} ${SOURCE_FILES})
        add_dependencies(${BUILD_NAME} base)
        target_link_libraries(${BUILD_NAME} ${VULKAN_LIBRARY} ${BASE_LIBRARY} ${XCB_LIBRARIES})

    else()

        message("Failed!")

    endif()

else()

    if( VULKAN_FOUND AND GLM_FOUND)

        message(OK!)
        message(${GLM_INCLUDE_DIR})
        include_directories(${VULKAN_INCLUDE_DIR} ${GLM_INCLUDE_DIR} ${GLI_INCLUDE_DIR})
        add_executable(${BUILD_NAME} ${SOURCE_FILES})
        add_dependencies(${BUILD_NAME} base)
        target_link_libraries(${BUILD_NAME} ${VULKAN_LIBRARY} ${BASE_LIBRARY})

    else()

        message("Failed!")

    endif()
endif()
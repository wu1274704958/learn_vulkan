include(CMakeParseArguments)

function(CreateExample)
set(USE_GLI true)
set(USE_ASSIMP true)

cmake_parse_arguments(CE "NO_GLI;NO_ASSIMP" "DIR" "FILES" ${ARGN} )

if(CE_NO_GLI)
    set(USE_GLI false)
endif()

if(CE_NO_ASSIMP)
    set(USE_ASSIMP false)
endif()

set(SOURCE_FILE_LIST "")

foreach(f ${CE_FILES})
    list(APPEND SOURCE_FILE_LIST "example/${CE_DIR}/${f} ")
endforeach()

set(BUILD_NAME "${CE_DIR}")

message(---------------------------------)
message(name = ${BUILD_NAME})
message(use_gli = ${USE_GLI})
message(use_assimp = ${USE_ASSIMP})
message(files = ${SOURCE_FILE_LIST})
message(---------------------------------)

add_executable(${BUILD_NAME} ${SOURCE_FILE_LIST})
add_dependencies(${BUILD_NAME} base)


target_include_directories(${BUILD_NAME} PRIVATE ${BASE_INCLUDE_DIR} ${IMGUI_INCLUDE_DIR})

target_include_directories(${BUILD_NAME} PRIVATE ${VULKAN_INCLUDE_DIR} ${GLM_INCLUDE_DIR})
target_link_libraries(${BUILD_NAME} ${VULKAN_LIBRARY} ${BASE_LIBRARY})

if(USE_GLI)
    target_include_directories(${BUILD_NAME} PRIVATE ${GLI_INCLUDE_DIR})
endif(USE_GLI)

if(USE_ASSIMP)
    target_include_directories(${BUILD_NAME} PRIVATE ${ASSIMP_INCLUDE_DIR})
    target_link_libraries(${BUILD_NAME} ${ASSIMP_LIBRARY})
endif(USE_ASSIMP)

if(USE_D2D_WSI)
    if(XCB_FOUND)
        target_include_directories(${BUILD_NAME} PRIVATE ${XCB_INCLUDE_DIRS})
        target_link_libraries(${BUILD_NAME} ${XCB_LIBRARIES})
    else()
        message("Not Find XCB!!!")
    endif()
endif(USE_D2D_WSI)

endfunction(CreateExample)

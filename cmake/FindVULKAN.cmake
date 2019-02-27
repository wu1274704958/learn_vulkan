message( "finding VULKAN!"  )

if(WIN32)

    message("Is Windows")
    set(VULKAN_PATH $ENV{VULKAN_PATH})
    if( VULKAN_PATH )

        message("Find VULKAN_PATH env!")
        message(${VULKAN_PATH})

        find_path( VULKAN_INCLUDE_DIR vulkan "${VULKAN_PATH}/Include" )
        find_library( VULKAN_LIBRARY  vulkan-1.lib "${VULKAN_PATH}/Lib")

        if( VULKAN_INCLUDE_DIR AND VULKAN_LIBRARY)

            set( VULKAN_FOUND TRUE )

        else()

            set( VULKAN_FOUND FALSE )

        endif()

    else()

        set( VULKAN_FOUND FALSE )
        message("Not Find VULKAN_PATH env!")

    endif()

else()

    message("Not Windows!")
    find_path( VULKAN_INCLUDE_DIR vulkan "/usr/include" )
    find_library( VULKAN_LIBRARY  vulkan "/usr/lib/x86_64-linux-gnu/")

    if( VULKAN_INCLUDE_DIR AND VULKAN_LIBRARY)

        set( VULKAN_FOUND TRUE )

    else()

        set( VULKAN_FOUND FALSE )

    endif()

endif()

message("................................................................")
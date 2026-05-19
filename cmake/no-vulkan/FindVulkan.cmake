set(Vulkan_FOUND TRUE)
set(Vulkan_LIBRARIES Vulkan::Vulkan)
set(Vulkan_INCLUDE_DIRS "")

if(NOT TARGET Vulkan::Vulkan)
    add_library(Vulkan::Vulkan INTERFACE IMPORTED)
endif()

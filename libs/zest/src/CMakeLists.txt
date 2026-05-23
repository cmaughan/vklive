find_package(glm CONFIG REQUIRED)
find_package(concurrentqueue CONFIG QUIET)
if(NOT TARGET concurrentqueue::concurrentqueue)
    find_package(unofficial-concurrentqueue CONFIG REQUIRED)
    add_library(concurrentqueue::concurrentqueue INTERFACE IMPORTED)
    set_target_properties(concurrentqueue::concurrentqueue PROPERTIES
        INTERFACE_LINK_LIBRARIES unofficial::concurrentqueue::concurrentqueue)
endif()
find_package(SDL2 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Stb REQUIRED)
find_path(TINYDIR_INCLUDE_DIR "tinydir.h")
find_path(CPPCODEC_INCLUDE_DIRS "cppcodec/base32_crockford.hpp")

set(UTILS_SOURCES
    ${ZEST_ROOT}/src/CMakeLists.txt
    ${ZEST_ROOT}/src/algorithm/container_utils.cpp
    ${ZEST_ROOT}/src/callback/callback.cpp
    ${ZEST_ROOT}/src/file/file.cpp
    ${ZEST_ROOT}/src/file/runtree.cpp
    ${ZEST_ROOT}/src/file/toml_utils.cpp
    ${ZEST_ROOT}/src/math/math_utils.cpp
    ${ZEST_ROOT}/src/settings/settings.cpp
    ${ZEST_ROOT}/src/string/string_utils.cpp
    ${ZEST_ROOT}/src/time/profiler.cpp
    ${ZEST_ROOT}/src/time/time_provider.cpp
    ${ZEST_ROOT}/src/time/timer.cpp
    ${ZEST_ROOT}/src/ui/colors.cpp
    ${ZEST_ROOT}/src/ui/fonts.cpp
    ${ZEST_ROOT}/src/ui/dpi.cpp
    ${ZEST_ROOT}/src/ui/imgui_extras.cpp
    ${ZEST_ROOT}/src/ui/layout_manager.cpp
    ${ZEST_ROOT}/src/ui/nanovg.cpp

    ${ZEST_ROOT}/include/zest/algorithm/container_utils.h
    ${ZEST_ROOT}/include/zest/algorithm/ring_buffer.h
    ${ZEST_ROOT}/include/zest/algorithm/ringiterator.h
    ${ZEST_ROOT}/include/zest/file/runtree.h
    ${ZEST_ROOT}/include/zest/file/toml_utils.h
    ${ZEST_ROOT}/include/zest/thread/thread_utils.h
    ${ZEST_ROOT}/include/zest/thread/threadpool.h
    ${ZEST_ROOT}/include/zest/time/profiler.h
    ${ZEST_ROOT}/include/zest/ui/colors.h
    ${ZEST_ROOT}/include/zest/ui/dpi.h
    ${ZEST_ROOT}/include/zest/ui/imgui_extras.h
    ${ZEST_ROOT}/include/zest/ui/layout_manager.h
    ${ZEST_ROOT}/include/zest/ui/fonts.h
    ${ZEST_ROOT}/include/zest/ui/fontstash.h
    ${ZEST_ROOT}/include/zest/ui/nanovg.h
    )

set(IMGUI_SOURCE
    ${ZEST_ROOT}/src/imgui/imgui.cpp
    ${ZEST_ROOT}/src/imgui/implot.cpp
    ${ZEST_ROOT}/src/imgui/implot_demo.cpp
    ${ZEST_ROOT}/src/imgui/implot_items.cpp
    ${ZEST_ROOT}/src/imgui/imgui_demo.cpp
    ${ZEST_ROOT}/src/imgui/imgui_draw.cpp
    ${ZEST_ROOT}/src/imgui/imgui_tables.cpp
    ${ZEST_ROOT}/src/imgui/imgui_widgets.cpp
    ${ZEST_ROOT}/src/imgui/imgui_impl_sdl2.cpp
    ${ZEST_ROOT}/src/imgui/imgui_impl_vulkan.cpp
    ${ZEST_ROOT}/include/zest/imgui/imgui_impl_sdl2.h
    ${ZEST_ROOT}/include/zest/imgui/imgui_impl_vulkan.h
    ${ZEST_ROOT}/include/zest/imgui/imstb_rectpack.h
    ${ZEST_ROOT}/include/zest/imgui/imstb_textedit.h
    ${ZEST_ROOT}/include/zest/imgui/imstb_truetype.h
    ${ZEST_ROOT}/include/zest/imgui/imgui_internal.h
    ${ZEST_ROOT}/include/zest/imgui/imconfig.h
    ${ZEST_ROOT}/include/zest/imgui/imgui.h
)

# Create the library
add_library(Zest STATIC
    ${UTILS_SOURCES}
    ${IMGUI_SOURCE}
    )

target_compile_definitions(Zest
    PUBLIC
    )

add_library(Zest::Zest ALIAS Zest)

target_include_directories(Zest
    PRIVATE
        ${ZEST_ROOT}/libs
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${TINYDIR_INCLUDE_DIR}
        ${CPPCODEC_INCLUDE_DIRS}
    PUBLIC
        ${ZEST_ROOT}/external
        ${ZEST_ROOT}/include
        ${ZEST_ROOT}/include/zest/imgui
)

target_link_libraries(Zest
PUBLIC
    glm::glm
    concurrentqueue::concurrentqueue
    Vulkan::Vulkan
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    )

source_group ("Zest\\Timing" .*time.*)
source_group ("Zest\\String" .*string.*)
source_group ("Zest\\Math" .*math.*)
source_group ("Zest\\Geometry" .*geometry.*)
source_group ("Zest\\File" .*file.*)
source_group ("Zest\\Compile" .*compile.*)
source_group ("Zest\\Chibi" .*chibi.*)
source_group ("Zest\\Callback" .*callback.*)
source_group ("Zest\\Settings" .*settings.*)
source_group ("Zest\\GL" .*gl.*)
source_group ("Zest\\Thread" .*thread.*)
source_group ("Zest\\ImGui" FILES ${IMGUI_SOURCE})
source_group ("Zest\\UI" .*ui.*)
source_group ("Zest\\VM" .*vm.*)
source_group ("Zest\\Device" .*device.*)
source_group ("Zest\\Algorithm" .*algorithm.*)


find_package(Freetype REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(harfbuzz CONFIG REQUIRED)

add_library(vklive_nvim STATIC
    src/atlas_grid.cpp
    src/font/font_manager.cpp
    src/font/font_resolver.cpp
    src/font/glyph_cache.cpp
    src/font/text_service.cpp
    src/font/text_shaper.cpp
    src/input.cpp
    src/mpack_codec.cpp
    src/nvim_ui.cpp
    src/nvim_host.cpp
    src/nvim_process.cpp
    src/nvim_rpc.cpp
    src/render_model.cpp
    third_party/mpack/mpack-common.c
    third_party/mpack/mpack-expect.c
    third_party/mpack/mpack-node.c
    third_party/mpack/mpack-platform.c
    third_party/mpack/mpack-reader.c
    third_party/mpack/mpack-writer.c

    include/vklive_nvim/atlas_grid.h
    include/vklive_nvim/font_metrics.h
    include/vklive_nvim/glyph_atlas.h
    include/vklive_nvim/highlight.h
    include/vklive_nvim/input.h
    include/vklive_nvim/log.h
    include/vklive_nvim/mpack_codec.h
    include/vklive_nvim/nvim_host.h
    include/vklive_nvim/nvim_process.h
    include/vklive_nvim/nvim_rpc.h
    include/vklive_nvim/nvim_ui.h
    include/vklive_nvim/perf_timing.h
    include/vklive_nvim/render_model.h
    include/vklive_nvim/result.h
    include/vklive_nvim/text_service.h
    include/vklive_nvim/thread_check.h
    include/vklive_nvim/types.h
    include/vklive_nvim/unicode.h
)

add_library(vklive_nvim::vklive_nvim ALIAS vklive_nvim)

target_compile_features(vklive_nvim PUBLIC cxx_std_20)
target_compile_definitions(vklive_nvim PUBLIC MPACK_EXTENSIONS=1)
target_link_libraries(vklive_nvim
    PUBLIC
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        glm::glm
    PRIVATE
        Freetype::Freetype
        harfbuzz::harfbuzz
)

target_include_directories(vklive_nvim
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/third_party/mpack
)

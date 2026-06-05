#include <vklive_nvim/text_service.h>

#include <cassert>
#include <string>

int main(int argc, char** argv)
{
    assert(argc == 2);

    vklive_nvim::TextServiceConfig config;
    config.font_path = argv[1];
    config.bold_font_path = argv[1];
    config.italic_font_path = argv[1];
    config.bold_italic_font_path = argv[1];
    config.enable_ligatures = true;

    vklive_nvim::TextService service;
    assert(service.initialize(config, 13.0f, 96.0f));
    assert(service.metrics().cell_width > 0.0f);
    assert(service.metrics().cell_height > 0.0f);
    assert(service.atlas_width() == vklive_nvim::kAtlasSize);
    assert(service.atlas_height() == vklive_nvim::kAtlasSize);
    assert(service.atlas_data() != nullptr);

    const vklive_nvim::AtlasRegion ascii = service.resolve_cluster("A", false, false);
    assert(ascii.bitmap_size.x > 0);
    assert(ascii.bitmap_size.y > 0);
    assert(ascii.advance_px > 0);
    assert(service.atlas_dirty());

    const vklive_nvim::AtlasDirtyRect dirty = service.atlas_dirty_rect();
    assert(dirty.size.x > 0);
    assert(dirty.size.y > 0);

    service.clear_atlas_dirty();
    assert(!service.atlas_dirty());

    const std::string powerline_branch = "\xEE\x82\xA0";
    const vklive_nvim::AtlasRegion icon = service.resolve_cluster(powerline_branch, false, false);
    assert(icon.bitmap_size.x > 0);
    assert(icon.bitmap_size.y > 0);

    return 0;
}

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SURFACE_CPP = ROOT / "src" / "vulkan" / "vulkan_surface.cpp"


class SurfaceHdrStaticTests(unittest.TestCase):
    def test_surface_loader_has_hdr_and_rgba_decode_paths(self):
        text = SURFACE_CPP.read_text(encoding="utf-8")

        self.assertIn("stbi_is_hdr_from_memory", text)
        self.assertIn("stbi_loadf_from_memory", text)
        self.assertIn("STBI_rgb_alpha", text)
        self.assertIn("stbi_image_free", text)

    def test_default_sampler_uses_repeat_wrapping(self):
        text = SURFACE_CPP.read_text(encoding="utf-8")

        self.assertIn("samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eRepeat", text)
        self.assertIn("samplerCreateInfo.addressModeV = vk::SamplerAddressMode::eRepeat", text)
        self.assertIn("samplerCreateInfo.addressModeW = vk::SamplerAddressMode::eRepeat", text)

    def test_texture_upload_can_flip_rows_without_global_stbi_state(self):
        text = SURFACE_CPP.read_text(encoding="utf-8")

        self.assertIn("bool flipY", text)
        self.assertIn("flip_image_rows", text)
        self.assertNotIn("stbi_set_flip_vertically_on_load", text)


if __name__ == "__main__":
    unittest.main()

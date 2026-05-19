from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "run_tree" / "projects" / "pbr_robot"


class PbrTemplateTests(unittest.TestCase):
    def test_template_contains_named_model_environment_and_passes(self):
        scene = TEMPLATE / "default.scenegraph"
        text = scene.read_text(encoding="utf-8")

        self.assertIn("environment: studio_sky", text)
        self.assertIn("model: robot", text)
        self.assertIn("position: (0.0, -1.25, 6.0)", text)
        self.assertIn("samplers: (studio_sky)", text)
        self.assertIn("path: models/robot/scene.gltf", text)
        self.assertIn("uv_origin: lower_left", text)
        self.assertNotIn("flip_uv_y", text)
        self.assertIn("path: textures/environment/farm_field_puresky_1k.hdr", text)
        self.assertNotIn("surface: RobotChest_baseColor", text)
        self.assertNotIn("surface: RobotHead_baseColor", text)
        self.assertNotIn("surface: RobotExtremities_baseColor", text)
        self.assertNotIn("RobotChest_baseColor, RobotHead_baseColor, RobotExtremities_baseColor", text)

    def test_template_assets_are_self_contained(self):
        self.assertTrue((TEMPLATE / "models" / "robot" / "scene.gltf").is_file())
        self.assertTrue((TEMPLATE / "models" / "robot" / "scene.bin").is_file())
        self.assertTrue((TEMPLATE / "textures" / "environment" / "farm_field_puresky_1k.hdr").is_file())

    def test_template_shaders_bind_expected_environment_sampler(self):
        vertex = (TEMPLATE / "pbr.vert").read_text(encoding="utf-8")
        skybox = (TEMPLATE / "skybox.frag").read_text(encoding="utf-8")
        pbr = (TEMPLATE / "pbr.frag").read_text(encoding="utf-8")

        self.assertIn("ubo.iTime", vertex)
        self.assertIn("rotateY", vertex)
        self.assertIn("inTangent", vertex)
        self.assertIn("inBitangent", vertex)
        self.assertIn("outTangent", vertex)
        self.assertIn("outBitangent", vertex)
        self.assertIn("uniform sampler2D studio_sky", skybox)
        self.assertIn("uniform sampler2D studio_sky", pbr)
        self.assertIn('#include "vklive_pbr_material.glsl"', pbr)
        self.assertIn("vklBaseColor(outUV)", pbr)
        self.assertIn("vklNormalSample(uv)", pbr)
        self.assertIn("mat3 tbn", pbr)
        self.assertIn("vklMetallicRoughness(outUV)", pbr)
        self.assertNotIn("RobotChest_baseColor", pbr)
        self.assertNotIn("RobotHead_baseColor", pbr)
        self.assertNotIn("RobotExtremities_baseColor", pbr)
        self.assertIn("distributionGGX", pbr)

    def test_template_has_uv_debug_probe_scene(self):
        debug_scene = (TEMPLATE / "uv_debug.scenegraph").read_text(encoding="utf-8")
        debug_shader = (TEMPLATE / "uv_debug.frag").read_text(encoding="utf-8")

        self.assertIn("fs: uv_debug.frag", debug_scene)
        self.assertIn("uv_origin: lower_left", debug_scene)
        self.assertNotIn("flip_uv_y", debug_scene)
        self.assertIn('#include "vklive_pbr_material.glsl"', debug_shader)
        self.assertIn("vec2 flippedUv = vec2(outUV.x, 1.0 - outUV.y)", debug_shader)
        self.assertIn("gl_FragCoord.x < ubo.iResolution.x * 0.5", debug_shader)
        self.assertIn("vklBaseColor(sampleUv)", debug_shader)


if __name__ == "__main__":
    unittest.main()

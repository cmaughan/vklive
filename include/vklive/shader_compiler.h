#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <zest/file/file.h>

#include <vklive/scene.h>
#include <vklive/shader_bindings.h>

bool shader_parse_output(const std::string& strOutput, const fs::path& shaderPath, Scene& scene);
bool shader_reflect_binding_sets(const void* spirvData, size_t spirvSize, const fs::path& shaderPath, ShaderBindingSets& bindingSets);
bool shader_reflect_binding_sets(const std::vector<uint32_t>& spirv, const fs::path& shaderPath, ShaderBindingSets& bindingSets);

#pragma once

#include <string>

#include <zest/file/file.h>

#include <vklive/scene.h>

bool shader_parse_output(const std::string& strOutput, const fs::path& shaderPath, Scene& scene);

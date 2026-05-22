#pragma once

#include <string>

#include <zest/file/file.h>

namespace metal
{

enum class MetalGeometryCompatibility
{
    Unsupported,
    NormalLineVisualizer
};

MetalGeometryCompatibility metal_geometry_classify_source(const std::string& source);
MetalGeometryCompatibility metal_geometry_classify(const fs::path& shaderPath);

} // namespace metal

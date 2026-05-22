#include <vklive/metal/metal_geometry_compat.h>

#include <fstream>
#include <regex>
#include <sstream>

namespace
{

std::string strip_glsl_comments(const std::string& source)
{
    std::string stripped;
    stripped.reserve(source.size());

    bool inLineComment = false;
    bool inBlockComment = false;
    for (size_t i = 0; i < source.size(); ++i)
    {
        const char c = source[i];
        const char next = i + 1 < source.size() ? source[i + 1] : '\0';

        if (inLineComment)
        {
            if (c == '\n')
            {
                inLineComment = false;
                stripped.push_back(c);
            }
            continue;
        }

        if (inBlockComment)
        {
            if (c == '*' && next == '/')
            {
                inBlockComment = false;
                ++i;
            }
            else if (c == '\n')
            {
                stripped.push_back(c);
            }
            continue;
        }

        if (c == '/' && next == '/')
        {
            inLineComment = true;
            ++i;
            continue;
        }

        if (c == '/' && next == '*')
        {
            inBlockComment = true;
            ++i;
            continue;
        }

        stripped.push_back(c);
    }

    return stripped;
}

bool has_regex(const std::string& source, const char* pattern)
{
    return std::regex_search(source, std::regex(pattern));
}

size_t count_regex(const std::string& source, const char* pattern)
{
    const std::regex regex(pattern);
    return static_cast<size_t>(std::distance(std::sregex_iterator(source.begin(), source.end(), regex), std::sregex_iterator()));
}

} // namespace

namespace metal
{

MetalGeometryCompatibility metal_geometry_classify_source(const std::string& source)
{
    const std::string stripped = strip_glsl_comments(source);
    const bool hasTriangleInput = has_regex(stripped, R"(layout\s*\(\s*triangles\s*\)\s*in\s*;)");
    const bool hasLineStripOutput = has_regex(stripped, R"(layout\s*\(\s*line_strip\s*,\s*max_vertices\s*=\s*6\s*\)\s*out\s*;)");
    const bool iteratesInputTriangle = has_regex(stripped, R"(for\s*\([^;]*;[^;]*<\s*gl_in\s*\.\s*length\s*\(\s*\)[^;]*;[^)]*\))");
    const bool readsInputPosition = has_regex(stripped, R"(gl_in\s*\[[^\]]+\]\s*\.\s*gl_Position)");
    const bool hasNormalInput = has_regex(stripped, R"(\binNormal\b)");
    const bool writesLineEndpoints = count_regex(stripped, R"(\bgl_Position\s*=)") >= 2;
    const bool offsetsEndpointByNormal = has_regex(stripped, R"((inNormal|normal)\s*(\[[^\]]+\])?(\s*\.\s*xyz)?\s*\*)");
    const bool emitsTwoVertices = count_regex(stripped, R"(\bEmitVertex\s*\()") >= 2;
    const bool endsPrimitive = count_regex(stripped, R"(\bEndPrimitive\s*\()") >= 1;

    if (hasTriangleInput && hasLineStripOutput && iteratesInputTriangle && readsInputPosition && hasNormalInput && writesLineEndpoints && offsetsEndpointByNormal && emitsTwoVertices && endsPrimitive)
    {
        return MetalGeometryCompatibility::NormalLineVisualizer;
    }

    return MetalGeometryCompatibility::Unsupported;
}

MetalGeometryCompatibility metal_geometry_classify(const fs::path& shaderPath)
{
    std::ifstream shaderFile(shaderPath);
    if (!shaderFile)
    {
        return MetalGeometryCompatibility::Unsupported;
    }

    std::ostringstream source;
    source << shaderFile.rdbuf();
    return metal_geometry_classify_source(source.str());
}

} // namespace metal

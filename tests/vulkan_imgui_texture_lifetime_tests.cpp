#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <config_app.h>

namespace
{

std::string read_file(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

bool require(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

std::string function_body(const std::string& source, const std::string& signature, const std::string& nextSignature)
{
    const auto begin = source.find(signature);
    if (begin == std::string::npos)
    {
        return {};
    }

    const auto end = source.find(nextSignature, begin + signature.size());
    if (end == std::string::npos)
    {
        return source.substr(begin);
    }

    return source.substr(begin, end - begin);
}

} // namespace

int main()
{
    const std::string source = read_file(std::string(VKLIVE_ROOT) + "/src/vulkan/vulkan_imgui_texture.cpp");
    const std::string flush = function_body(source, "void VulkanImGuiTexture::Flush()", "int VulkanImGuiTexture::CreateTexture");
    const std::string destroy = function_body(source, "void VulkanImGuiTexture::DestroyFontInfo", "int VulkanImGuiTexture::UpdateTexture");
    const std::string update = function_body(source, "int VulkanImGuiTexture::UpdateTexture", "void VulkanImGuiTexture::DeleteTexture");

    bool ok = true;
    ok &= require(!flush.empty(), "VulkanImGuiTexture::Flush should exist");
    ok &= require(flush.find("vkDestroyImageView") == std::string::npos, "runtime font texture flush must not destroy image views still reachable from ImGui descriptors");
    ok &= require(flush.find("vkDestroyImage") == std::string::npos, "runtime font texture flush must not destroy images still reachable from ImGui descriptors");

    const auto freeDescriptor = destroy.find("vkFreeDescriptorSets");
    const auto destroyImageView = destroy.find("vkDestroyImageView");
    ok &= require(freeDescriptor != std::string::npos, "font texture teardown should free descriptor sets");
    ok &= require(destroyImageView != std::string::npos, "font texture teardown should destroy image views");
    ok &= require(freeDescriptor < destroyImageView, "font texture teardown should free descriptors before destroying image views");

    const auto waitFence = update.find("vkWaitForFences");
    const auto resetPool = update.find("vkResetCommandPool");
    ok &= require(waitFence != std::string::npos, "font texture uploads should wait for their previous upload fence");
    ok &= require(resetPool != std::string::npos, "font texture uploads should reset their command pool before re-recording");
    ok &= require(waitFence < resetPool, "font texture uploads should wait before resetting the upload command pool");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

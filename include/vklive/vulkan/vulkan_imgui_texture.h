#include <map>
#include <memory>
#include <zest/ui/fonts.h>
#include <vklive/vulkan/vulkan_context.h>

namespace vulkan
{

class VulkanImGuiTexture : public Zest::IFontTexture
{
public:
    VulkanImGuiTexture(VulkanContext& ctx, VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkDescriptorPool pool);

    // Texture handling bits using the imgui API
    virtual int UpdateTexture(int image, int x, int y, int w, int h, const unsigned char* data) override;
    virtual int CreateTexture(int w, int h, const unsigned char* data) override;
    virtual void DeleteTexture(int image) override;
    virtual void GetTextureSize(int image, int* w, int* h) override;
    virtual void* GetTexture(int image) override;
    virtual void BeginFrame() override;
    virtual void EndFrame() override;

    std::vector<void*> GetTextures();
    void Flush();

private:
    struct FontInfo
    {
        VkImage image = nullptr;
        VkImageView imageView = nullptr;
        VkDeviceMemory memory = nullptr;
        VkDescriptorSet descriptorSet = nullptr;
        VkSampler sampler = nullptr;
        VkBuffer uploadBuffer = nullptr;
        VkDeviceMemory uploadMemory = nullptr;
        VkDeviceSize memoryAlignment = 4;
        VkCommandPool commandPool = nullptr;
        VkCommandBuffer commandBuffer = nullptr;
        int width = 0;
        int height = 0;
        int textureId = 0;
        bool init = false;
    };

    int m_currentTextureId = 1;
    std::map<int, std::shared_ptr<FontInfo>> m_mapFonts;
    std::map<uint64_t, std::vector<std::shared_ptr<FontInfo>>> m_mapOldFonts;

    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    VkQueue m_queue;
    VkDescriptorPool m_pool;
    uint64_t m_frameIndex = 0;
    VulkanContext& m_ctx;
};

} // vulkan


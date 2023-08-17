#pragma once

#include "vklive/model.h"

#include "vulkan_context.h"
#include "vulkan_buffer.h"
#include "vulkan_model.h"

namespace vulkan
{

void vulkan_model_build_acceleration_structure(VulkanContext& ctx, VulkanModel& model);

} // namespace vulkan

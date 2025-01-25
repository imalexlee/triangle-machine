#pragma once

#include "system/device/vk_types.h"
#include <span>

VkRenderingInfo vk_rendering_info_create(std::span<VkRenderingAttachmentInfo*> color_attachment, const VkRenderingAttachmentInfo* depth_attachment,
                                         VkExtent2D extent);

VkRenderingAttachmentInfo vk_color_attachment_info_create(VkImageView view, const VkClearValue* clear, VkAttachmentLoadOp load_op,
                                                          VkAttachmentStoreOp store_op, VkImageView resolve_img_view = nullptr);

VkRenderingAttachmentInfo vk_depth_attachment_info_create(VkImageView view, VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op);

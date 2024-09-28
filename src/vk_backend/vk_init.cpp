#include "vk_init.h"
#include <vulkan/vulkan_core.h>

VkRenderingInfo vk_rendering_info_create(const VkRenderingAttachmentInfo* color_attachment, const VkRenderingAttachmentInfo* depth_attachment,
                                      VkExtent2D extent) {

    VkRenderingInfo rendering_info{};
    rendering_info.sType      = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea = VkRect2D{
        .offset = VkOffset2D{0, 0},
        .extent = extent,
    };

    rendering_info.pColorAttachments    = color_attachment;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.layerCount           = 1;
    rendering_info.pStencilAttachment   = nullptr;
    rendering_info.pDepthAttachment     = depth_attachment;

    return rendering_info;
}

VkRenderingAttachmentInfo vk_color_attachment_info_create(VkImageView view, const VkClearValue* clear, VkAttachmentLoadOp load_op,
                                                       VkAttachmentStoreOp store_op, VkImageView resolve_img_view) {
    VkRenderingAttachmentInfo color_attachment{};
    color_attachment.sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.pNext              = nullptr;
    color_attachment.imageView          = view;
    color_attachment.imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp             = load_op;
    color_attachment.storeOp            = store_op;
    color_attachment.resolveMode        = VK_RESOLVE_MODE_AVERAGE_BIT;
    color_attachment.resolveImageView   = resolve_img_view;
    color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    if (clear) {
        color_attachment.clearValue = *clear;
    }

    return color_attachment;
}

VkRenderingAttachmentInfo vk_depth_attachment_info_create(VkImageView view, VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op) {
    VkRenderingAttachmentInfo depth_attachment{};
    depth_attachment.sType                         = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_attachment.pNext                         = nullptr;
    depth_attachment.imageView                     = view;
    depth_attachment.imageLayout                   = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depth_attachment.loadOp                        = load_op;
    depth_attachment.storeOp                       = store_op;
    depth_attachment.clearValue.depthStencil.depth = 0.f;

    return depth_attachment;
}

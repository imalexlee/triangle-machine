#include <vk_backend/vk_types.h>

VkRenderingInfo create_rendering_info(VkRenderingAttachmentInfo& color_attachment,
                                      VkRenderingAttachmentInfo& depth_attachment,
                                      VkExtent2D extent);

VkRenderingAttachmentInfo create_color_attachment_info(VkImageView view, VkClearValue* clear,
                                                       VkAttachmentLoadOp load_op,
                                                       VkAttachmentStoreOp store_op,
                                                       VkImageView resolve_img_view);

VkRenderingAttachmentInfo create_depth_attachment_info(VkImageView view, VkAttachmentLoadOp load_op,
                                                       VkAttachmentStoreOp store_op);

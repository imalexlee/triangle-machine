#include <vk_backend/vk_types.h>

VkRenderingInfo create_rendering_info(const VkRenderingAttachmentInfo* color_attachment, const VkRenderingAttachmentInfo* depth_attachment,
                                      VkExtent2D extent);

VkRenderingAttachmentInfo create_color_attachment_info(VkImageView view, const VkClearValue* clear, VkAttachmentLoadOp load_op,
                                                       VkAttachmentStoreOp store_op, VkImageView resolve_img_view);

VkRenderingAttachmentInfo create_depth_attachment_info(VkImageView view, VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op);

#pragma once

#include <array>
#include <cstdint>
#include <vk_backend/vk_types.h>

struct Debugger {
    VkDebugUtilsMessengerEXT         messenger;
    PFN_vkSetDebugUtilsObjectNameEXT obj_name_pfn;
    DeletionQueue                    _deletion_queue;
    VkDevice                         logical_device;
};

/**
 * @brief Creates and sets up debug messages for vulkan objects
 *
 * @param db	    The Debugger to Initialize
 * @param instance  The instance attached to our debuggable Vulkan objects
 * @param device    The device attached to our debuggable Vulkan objects
 * @return	    Whether the debug extensions are available or not
 */
VkResult debugger_init(Debugger* db, VkInstance instance, VkDevice device);

/**
 * @brief Destroys debug messages
 *
 * @param db	    The Debugger to deinitialize
 * @param instance  The instance attached to our debuggable Vulkan objects
 */
void debugger_deinit(const Debugger* db, VkInstance instance);

/**
 * @brief Sets up our desired message severity and types of messages we want
 *
 * @return  A filled messanger creation info for our instance creation
 */
VkDebugUtilsMessengerCreateInfoEXT vk_messenger_info_create();

/**
 * @brief Sets our enabled validation features for our instance creation
 *
 * @return  A filled validation features struct
 */
VkValidationFeaturesEXT vk_validation_features_create();

/**
 * @brief Creates an array of validation layers we want
 *
 * @return  a compile-time generated array of validation features
 */
consteval std::array<const char*, 1> vk_validation_layers_create() { return std::array{"VK_LAYER_KHRONOS_validation"}; };

/**
 * @brief Enables setting readable names for vulkan objects like buffers, images, etc. for easier
 * debugging. For example, instead of seeing that "0x072849" messed up, we can see "depth image"
 *
 * @tparam T	      Generic which accepts any Vulkan handle object
 * @param db	      The Debugger attached to the instance that this object is also associated with
 * @param handle      The Vulkan object handle to ascribe a name to
 * @param object_type The type of object this is
 * @param name	      The name to give the object
 */
template <typename T> void debugger_set_handle_name(const Debugger* db, T handle, VkObjectType object_type, std::string name) {
    VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType                    = object_type;
    name_info.objectHandle                  = reinterpret_cast<uint64_t>(handle);
    name_info.pObjectName                   = name.data();
    db->obj_name_pfn(db->logical_device, &name_info);
};

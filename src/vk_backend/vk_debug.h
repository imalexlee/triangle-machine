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
VkResult init_debugger(Debugger* db, VkInstance instance, VkDevice device);

/**
 * @brief Destroys debug messages
 *
 * @param db	    The Debugger to deinitialize
 * @param instance  The instance attached to our debuggable Vulkan objects
 */
void deinit_debugger(Debugger* db, VkInstance instance);

/**
 * @brief Sets up our desired message severity and types of messages we want
 *
 * @return  A filled messanger creation info for our instance creation
 */
VkDebugUtilsMessengerCreateInfoEXT create_messenger_info();

/**
 * @brief Sets our enabled validation features for our instance creation
 *
 * @return  A filled validation features struct
 */
VkValidationFeaturesEXT create_validation_features();

/**
 * @brief Creates an array of validation layers we want
 *
 * @return  a compile-time generated array of validation features
 */
consteval std::array<const char*, 1> create_validation_layers() {
    return std::array<const char*, 1>{"VK_LAYER_KHRONOS_validation"};
};

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
template <typename T>
void set_handle_name(const Debugger* db, T handle, VkObjectType object_type, std::string name) {
    VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType                    = object_type;
    name_info.objectHandle                  = (uint64_t)handle;
    name_info.pObjectName                   = name.data();
    db->obj_name_pfn(db->logical_device, &name_info);
};

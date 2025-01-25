#include "vk_context.h"
#include "system/platform/window.h"
#include "vk_debug.h"
#include "vk_options.h"
#include "vk_utils.h"
#include <array>

static std::vector<const char*> get_instance_extensions();
static VkInstance               vk_instance_create(const char* app_name, const char* engine_name);

void vk_context_init(VkContext* vk_ctx, const Window* window) {
    vk_ctx->instance        = vk_instance_create("triangle machine", "my engine");
    vk_ctx->physical_device = vk_physical_device_create(vk_ctx->instance);
    vk_ctx->surface         = window_get_vk_surface(window, vk_ctx->instance);
    vk_ctx->queue_families  = vk_queue_families_get(vk_ctx->physical_device, vk_ctx->surface);
    vk_ctx->logical_device  = vk_logical_device_create(vk_ctx->physical_device, &vk_ctx->queue_families);
    vk_ctx->queues          = vk_device_queues_create(vk_ctx->logical_device, &vk_ctx->queue_families);

    if (vk_opts::validation_enabled) {
        debugger_init(&vk_ctx->debugger, vk_ctx->instance, vk_ctx->logical_device);
    }
}

static std::vector<const char*> get_instance_extensions() {
    uint32_t                 count{0};
    const char**             glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char*> extensions;
    for (size_t i = 0; i < count; i++) {
        extensions.emplace_back(glfw_extensions[i]);
    }
    if constexpr (vk_opts::validation_enabled) {
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

static VkInstance vk_instance_create(const char* app_name, const char* engine_name) {

    VkApplicationInfo app_info{};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext              = nullptr;
    app_info.pApplicationName   = app_name;
    app_info.pEngineName        = engine_name;
    app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion         = VK_API_VERSION_1_3;

    const std::vector<const char*> instance_extensions = get_instance_extensions();

    VkInstanceCreateInfo instance_ci{};
    instance_ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pApplicationInfo        = &app_info;
    instance_ci.flags                   = 0;
    instance_ci.ppEnabledExtensionNames = instance_extensions.data();
    instance_ci.enabledExtensionCount   = instance_extensions.size();

    VkDebugUtilsMessengerCreateInfoEXT debug_ci;
    VkValidationFeaturesEXT            validation_features;
    std::array<const char*, 1>         validation_layers;

    if constexpr (vk_opts::validation_enabled) {
        debug_ci            = vk_messenger_info_create();
        validation_features = vk_validation_features_create();
        validation_layers   = vk_validation_layers_create();

        validation_features.pNext       = &debug_ci;
        instance_ci.pNext               = &validation_features;
        instance_ci.enabledLayerCount   = validation_layers.size();
        instance_ci.ppEnabledLayerNames = validation_layers.data();
    }

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instance_ci, nullptr, &instance));
    return instance;
}

void vk_context_deinit(VkContext* vk_ctx) {
    DEBUG_PRINT("destroying Vulkan Context");

    if (vk_opts::validation_enabled) {
        debugger_deinit(&vk_ctx->debugger, vk_ctx->instance);
    }
    vkDestroyDevice(vk_ctx->logical_device, nullptr);
    vkDestroySurfaceKHR(vk_ctx->instance, vk_ctx->surface, nullptr);
    vkDestroyInstance(vk_ctx->instance, nullptr);
}
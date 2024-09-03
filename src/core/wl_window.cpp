#include "wl_window.h"
#include "xdg-shell.h"
#include <cassert>
#include <cstdio>
#include <vk_backend/vk_backend.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_wayland.h>
#include <wayland-client.h>

// decls
static void handle_shell_ping(void* data, struct xdg_wm_base* shell, uint32_t serial);

static void
handle_shell_surface_configure(void* data, struct xdg_surface* shell_surface, uint32_t serial);

static void handle_registry(void*               data,
                            struct wl_registry* registry,
                            uint32_t            name,
                            const char*         interface,
                            uint32_t            version);

static void handle_toplevel_configure(void*                data,
                                      struct xdg_toplevel* toplevel,
                                      int32_t              width,
                                      int32_t              height,
                                      struct wl_array*     states);
static void handle_toplevel_close(void* data, struct xdg_toplevel* toplevel);

static const struct wl_registry_listener reg_listener = {
    .global = handle_registry,
};
static const struct xdg_wm_base_listener shell_listener = {
    .ping = handle_shell_ping,
};
static const struct xdg_surface_listener shell_surface_listener = {
    .configure = handle_shell_surface_configure,
};
static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = handle_toplevel_configure,
    .close     = handle_toplevel_close,
};

static wl_compositor*       compositor;
static xdg_wm_base*         shell;
static struct xdg_surface*  shell_surface;
static wl_display*          display;
static wl_surface*          surface;
static wl_registry*         registry;
static struct xdg_toplevel* toplevel;
inline int                  new_width       = 0;
inline int                  new_height      = 0;
inline uint32_t             wl_width        = 1600;
inline uint32_t             wl_height       = 900;
static bool                 resize          = false;
static bool                 ready_to_resize = false;
inline bool                 wl_quit         = false;

static void
handle_shell_ping([[maybe_unused]] void* data, struct xdg_wm_base* shell, uint32_t serial) {
    xdg_wm_base_pong(shell, serial);
}

static void handle_shell_surface_configure([[maybe_unused]] void* data,
                                           struct xdg_surface*    shell_surface,
                                           uint32_t               serial) {
    xdg_surface_ack_configure(shell_surface, serial);
    if (resize) {
        ready_to_resize = true;
    }
}

static void handle_registry([[maybe_unused]] void*    data,
                            struct wl_registry*       registry,
                            uint32_t                  name,
                            const char*               interface,
                            [[maybe_unused]] uint32_t version) {

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = (wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 1);
        assert(compositor);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        shell = (xdg_wm_base*)wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        assert(shell);
        xdg_wm_base_add_listener(shell, &shell_listener, NULL);
    }
}

static void handle_toplevel_configure([[maybe_unused]] void*                data,
                                      [[maybe_unused]] struct xdg_toplevel* toplevel,
                                      int32_t                               width,
                                      int32_t                               height,
                                      [[maybe_unused]] struct wl_array*     states) {
    if (width != 0 && height != 0) {
        resize     = 1;
        new_width  = width;
        new_height = height;
    }
}

static void handle_toplevel_close([[maybe_unused]] void*                data,
                                  [[maybe_unused]] struct xdg_toplevel* toplevel) {
    wl_quit = true;
}

void init_wayland_client(const char* app_name) {
    display = wl_display_connect("wayland-1");
    assert(display);

    registry = wl_display_get_registry(display);
    assert(registry);

    wl_registry_add_listener(registry, &reg_listener, NULL);
    wl_display_roundtrip(display);

    surface = wl_compositor_create_surface(compositor);
    assert(surface);

    shell_surface = xdg_wm_base_get_xdg_surface(shell, surface);
    assert(shell_surface);
    xdg_surface_add_listener(shell_surface, &shell_surface_listener, NULL);

    toplevel = xdg_surface_get_toplevel(shell_surface);
    xdg_toplevel_add_listener(toplevel, &toplevel_listener, NULL);

    xdg_toplevel_set_title(toplevel, app_name);
    xdg_toplevel_set_app_id(toplevel, app_name);

    wl_surface_commit(surface);
    wl_display_roundtrip(display);
    wl_surface_commit(surface);
}

VkSurfaceKHR get_wayland_surface(VkInstance instance) {
    VkWaylandSurfaceCreateInfoKHR createInfo{};
    createInfo.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    createInfo.display = display;
    createInfo.surface = surface;
    createInfo.pNext   = nullptr;
    createInfo.flags   = 0;

    VkSurfaceKHR vk_surface;
    VK_CHECK(vkCreateWaylandSurfaceKHR(instance, &createInfo, nullptr, &vk_surface));

    return vk_surface;
};

void deinit_wayland_client() {
    xdg_toplevel_destroy(toplevel);
    xdg_surface_destroy(shell_surface);
    wl_surface_destroy(surface);
    xdg_wm_base_destroy(shell);
    wl_compositor_destroy(compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
}

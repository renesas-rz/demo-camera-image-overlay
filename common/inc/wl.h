/*******************************************************************************
 * FILENAME: wl.h
 *
 * DESCRIPTION:
 *   Wayland functions.
 *
 * PUBLIC FUNCTIONS:
 *   wl_connect_display
 *   wl_disconnect_display
 *
 *   wl_create_window
 *   wl_delete_window
 *
 * AUTHOR: RVC       START DATE: 28/03/2023
 *
 ******************************************************************************/

#ifndef _WL_H_
#define _WL_H_

#include <stdbool.h>
#include <wayland-egl.h>
#include <wayland-client.h>

#include "xdg-shell-client-protocol.h"

/******************************************************************************
 *                              GLOBAL VARIABLES                              *
 ******************************************************************************/

/* 1: Window closed */
extern volatile sig_atomic_t g_window_closed;

/******************************************************************************
 *                           STRUCTURE DEFINITIONS                            *
 ******************************************************************************/

/* https://wayland-book.com/
 * https://wayland.app/protocols/
 * https://jan.newmarch.name/Wayland/index.html
 * https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_display
 */
typedef struct
{
    /* The core global object. This is a special singleton object.
     * It's used for internal Wayland protocol features */
    struct wl_display * p_display;

    /* The singleton global registry object. When a client creates a registry
     * object, the registry object will emit a global event for each global
     * currently in the registry.
     *
     * Globals come and go as a result of device or monitor hotplugs,
     * reconfiguration, or other events, and the registry will send out 'global'
     * and 'global_remove' events to keep the client up to date with changes.
     *
     * Warning: Currently, event 'global_remove' is not implemented */
    struct wl_registry * p_registry;

    /* A compositor. This object is a singleton global.
     * It is in charge of combining the contents of multiple surfaces into
     * one displayable output */
    struct wl_compositor * p_compositor;

    /* The interface is exposed as a global object enabling clients to turn
     * their wl_surfaces into windows in a desktop environment.
     *
     * It defines the basic functionality needed for clients and the compositor
     * to create windows that can be dragged, resized, maximized, etc, as well
     * as creating transient windows such as popup menus */
    struct xdg_wm_base * p_wm_base;

} wl_display_t;

typedef struct
{
    /* A surface is a rectangular area that may be displayed on zero or more
     * outputs, and shown any number of times at the compositor's descretion.
     *
     * A surface without a "role" is fairly useless: a compositor does not
     * know where, when, or how to present it. The role is the purpose of a
     * 'wl_surface'. In this case, the role is a window as defined by a shell
     * protocol */
    struct wl_surface * p_surface;

    /* An interface that may be implemented by a 'wl_surface', for
     * implementations that provide a desktop-style user interface.
     *
     * It provides a base set of functionality required to construct user
     * interface elements requiring management by the compositor, such as
     * toplevel windows, menus, etc. The types of functionality are split
     * into 'xdg_surface' roles */
    struct xdg_surface * p_xdg_surface;

    /* This interface defines an 'xdg_surface' role which allows a surface to,
     * among other things, set window-like properties such as maximize,
     * fullscreen, and minimize, set application-specific metadata like title
     * and id, and well as trigger user interactive operations such as
     * interactive resize and move */
    struct xdg_toplevel * p_xdg_toplevel;

    /* An EGL window created from Wayland surface */
    struct wl_egl_window * p_egl_window;

} wl_window_t;

/******************************************************************************
 *                            FUNCTION DECLARATION                            *
 ******************************************************************************/

/* Connect to Wayland display and get necessary objects.
 * Return a value other than NULL if successful.
 *
 * Note 1: The code is based on:
 *   - https://gitlab.freedesktop.org/wayland/weston/-/blob/8.0.0/clients/
 *   - https://github.com/krh/weston/blob/master/clients/simple-egl.c
 *   - https://jan.newmarch.name/Wayland/EGL/
 *
 * Note 2: Struct 'wl_display_t' must be freed when no longer used */
wl_display_t * wl_connect_display();

/* Destroy all objects. Then, close the connection to display and free all
 * resources associated with it.
 *
 * Note: This function will deallocate 'p_display' */
void wl_disconnect_display(wl_display_t * p_display);

/* Create a window on Wayland desktop.
 * Return a value other than NULL if successful.
 *
 * Note: Struct 'wl_window_t' must be freed when no longer used */
wl_window_t * wl_create_window(const wl_display_t * p_display,
                               const char * p_title,
                               uint32_t width, uint32_t height);

/* Delete window.
 * Note: This function will deallocate 'p_window' */
void wl_delete_window(wl_window_t * p_window);

#endif /* _WL_H_ */

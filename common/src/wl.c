/*******************************************************************************
 * FILENAME: wl.c
 *
 * DESCRIPTION:
 *   Wayland function definition.
 * 
 * NOTE:
 *   For function usage, please refer to 'wl.h'.
 * 
 * AUTHOR: RVC       START DATE: 29/03/2023
 *
 ******************************************************************************/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "wl.h"
#include "util.h"

/******************************************************************************
 *                        STATIC FUNCTION DECLARATION                         *
 ******************************************************************************/

static void global_registry_handler(void * p_data,
                                    struct wl_registry * p_registry,
                                    uint32_t id, const char * p_interface,
                                    uint32_t version);

static void global_registry_remover(void * p_data,
                                    struct wl_registry * p_registry,
                                    uint32_t id);

/******************************************************************************
 *                          STATIC GLOBAL VARIABLES                           *
 ******************************************************************************/

static const struct wl_registry_listener g_registry_listener =
{
    global_registry_handler,
    global_registry_remover
};

/******************************************************************************
 *                            FUNCTION DEFINITION                             *
 ******************************************************************************/

wl_display_t * wl_connect_display()
{
    wl_display_t * p_result = NULL;

    struct wl_display  * p_display  = NULL;
    struct wl_registry * p_registry = NULL;

    /* Connect to Wayland display */
    p_display = wl_display_connect(NULL);
    if (p_display == NULL)
    {
        printf("Error: Failed to connect to Wayland display\n");
        return NULL;
    }

    /* Allocate struct 'wl_display_t' */
    p_result = calloc(1, sizeof(wl_display_t));

    /* Get Wayland objects */
    p_registry = wl_display_get_registry(p_display);
    wl_registry_add_listener(p_registry, &g_registry_listener, p_result);

    wl_display_roundtrip(p_display);

    /* Update struct 'wl_display_t' */
    p_result->p_display  = p_display;
    p_result->p_registry = p_registry;

    if ((p_result->p_compositor == NULL) || (p_result->p_shell == NULL))
    {
        printf("Error: Failed to get Wayland objects\n");
        wl_disconnect_display(p_result);

        return NULL;
    }

    return p_result;
}

void wl_disconnect_display(wl_display_t * p_display)
{
    /* Check parameter */
    assert(p_display != NULL);
    assert(p_display->p_display  != NULL);
    assert(p_display->p_registry != NULL);

    if (p_display->p_shell != NULL)
    {
        wl_shell_destroy(p_display->p_shell);
    }

    if (p_display->p_compositor != NULL)
    {
        wl_compositor_destroy(p_display->p_compositor);
    }

    wl_registry_destroy(p_display->p_registry);
    wl_display_disconnect(p_display->p_display);

    /* Deallocate struct 'wl_display_t' */
    free(p_display);
}

wl_window_t * wl_create_window(const wl_display_t * p_display,
                               const char * p_title,
                               uint32_t width, uint32_t height)
{
    wl_window_t * p_result = NULL;

    struct wl_surface       * p_surface       = NULL;
    struct wl_shell_surface * p_shell_surface = NULL;
    struct wl_egl_window    * p_egl_window    = NULL;

    /* Check parameters */
    assert(p_display != NULL);
    assert((width > 0) && (height > 0));

    assert(p_display->p_shell      != NULL);
    assert(p_display->p_compositor != NULL);

    /* Ask compositor to create a new surface */
    p_surface = wl_compositor_create_surface(p_display->p_compositor);
    if (p_surface == NULL)
    {
        printf("Error: Failed to create Wayland surface\n");
        return NULL;
    }

    /* Create a shell surface from a surface */
    p_shell_surface = wl_shell_get_shell_surface(p_display->p_shell, p_surface);
    wl_shell_surface_set_toplevel(p_shell_surface);

    /* Set window title */
    if (p_title != NULL)
    {
        wl_shell_surface_set_title(p_shell_surface, p_title);
    }

    /* Create EGL window from Wayland surface */
    p_egl_window = wl_egl_window_create(p_surface, width, height);
    if (p_egl_window == NULL)
    {
        printf("Error: Failed to create EGL window\n");
        wl_shell_surface_destroy(p_shell_surface);
        wl_surface_destroy(p_surface);

        return NULL;
    }

    p_result = calloc(1, sizeof(wl_window_t));
    p_result->p_surface       = p_surface;
    p_result->p_shell_surface = p_shell_surface;
    p_result->p_egl_window    = p_egl_window;

    return p_result;
}

void wl_delete_window(wl_window_t * p_window)
{
    /* Check parameter */
    assert(p_window != NULL);
    assert(p_window->p_surface       != NULL);
    assert(p_window->p_shell_surface != NULL);
    assert(p_window->p_egl_window    != NULL);

    wl_egl_window_destroy(p_window->p_egl_window);

    wl_shell_surface_destroy(p_window->p_shell_surface);
    wl_surface_destroy(p_window->p_surface);

    /* Deallocate struct 'wl_window_t' */
    free(p_window);
}

/******************************************************************************
 *                         STATIC FUNCTION DEFINITION                         *
 ******************************************************************************/

static void global_registry_handler(void * p_data,
                                    struct wl_registry * p_registry,
                                    uint32_t id, const char * p_interface,
                                    uint32_t version)
{
    /* Mark parameter as unused */
    UNUSED(version);

    wl_display_t * p_display = (wl_display_t *)p_data;

    if (strcmp(p_interface, "wl_compositor") == 0)
    {
        p_display->p_compositor = wl_registry_bind(p_registry, id,
                                                   &wl_compositor_interface, 1);
    }
    else if (strcmp(p_interface, "wl_shell") == 0)
    {
        p_display->p_shell = wl_registry_bind(p_registry, id,
                                              &wl_shell_interface, 1);
    }
}

static void global_registry_remover(void * p_data,
                                    struct wl_registry * p_registry,
                                    uint32_t id)
{
    /* Mark parameters as unused */
    UNUSED(id);
    UNUSED(p_data);
    UNUSED(p_registry);

    /* Not implemented */
}

/****************************************************************************
 * This file is part of Hawaii Shell.
 *
 * Copyright (C) 2013 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 * Copyright (C) 2013 Giulio Camuffo <giuliocamuffo@gmail.com>
 *
 * Author(s):
 *    Giulio Camuffo
 *    Pier Luigi Fiorini
 *
 * $BEGIN_LICENSE:LGPL2.1+$
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * $END_LICENSE$
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <linux/input.h>

#include <wayland-server.h>

#include <weston/compositor.h>
#include <weston/matrix.h>
// #include <weston/config-parser.h>

#include "desktop-shell.h"
#include "wayland-hawaii-server-protocol.h"
#include "wayland-notification-daemon-server-protocol.h"
#include "wayland-screensaver-server-protocol.h"
#include "shellsurface.h"

#include "scaleeffect.h"
#include "griddesktops.h"
#include "fademovingeffect.h"
#include "zoomeffect.h"
#include "inoutsurfaceeffect.h"
#include "opacityeffect.h"
#include "inputpanel.h"
#include "shellseat.h"
#include "shellwindow.h"
#include "workspace.h"
#include "minimizeeffect.h"
#include "weston-version.h"

class PopupGrab : public ShellGrab
{
public:
    weston_surface *surface;
    wl_resource *shsurfResource;
    bool inside;
    uint32_t creationTime;

    void end()
    {
        wl_hawaii_shell_surface_send_popup_done(shsurfResource);
        Shell::endGrab(this);
        wl_resource *res = shsurfResource;
        shsurfResource = nullptr;
        wl_resource_destroy(res);
        delete this;
    }

    void dismiss()
    {
        Shell::endGrab(this);
    }

    static const struct wl_hawaii_shell_surface_interface m_shellSurfaceImpl;
};

const struct wl_hawaii_shell_surface_interface PopupGrab::m_shellSurfaceImpl = {
    wrapInterface(&PopupGrab::dismiss)
};

static void shell_surface_focus(struct weston_pointer_grab *base)
{
    ShellGrab *grab = container_of(base, ShellGrab, grab);
    PopupGrab *cgrab = static_cast<PopupGrab *>(grab);

    wl_fixed_t sx, sy;
    struct weston_surface *surface = weston_compositor_pick_surface(grab->pointer->seat->compositor,
                                                                    grab->pointer->x, grab->pointer->y,
                                                                    &sx, &sy);

    cgrab->inside = surface == cgrab->surface;
    if (cgrab->inside)
        weston_pointer_set_focus(grab->pointer, surface, sx, sy);
}

static void shell_surface_motion(struct weston_pointer_grab *base, uint32_t time)
{
    ShellGrab *grab = container_of(base, ShellGrab, grab);

#if (WESTON_VERSION_NUMBER >= WESTON_VERSION_CHECK(1, 3, 0))
    struct wl_resource *resource;
    wl_resource_for_each(resource, &grab->pointer->focus_resource_list) {
#else
    struct wl_resource *resource = grab->pointer->focus_resource;
    if (resource) {
#endif
        wl_fixed_t sx, sy;
        weston_surface_from_global_fixed(grab->pointer->focus, grab->pointer->x, grab->pointer->y, &sx, &sy);
        wl_pointer_send_motion(resource, time, sx, sy);
    }
}

static void shell_surface_button(struct weston_pointer_grab *base, uint32_t time, uint32_t button, uint32_t state)
{
    ShellGrab *grab = container_of(base, ShellGrab, grab);
    PopupGrab *cgrab = static_cast<PopupGrab *>(grab);

#if (WESTON_VERSION_NUMBER >= WESTON_VERSION_CHECK(1, 3, 0))
    wl_resource *resource;
    wl_resource_for_each(resource, &grab->pointer->focus_resource_list) {
#else
    struct wl_resource *resource = grab->pointer->focus_resource;
    if (resource) {
#endif
        struct wl_display *display = wl_client_get_display(wl_resource_get_client(resource));
        uint32_t serial = wl_display_get_serial(display);
        wl_pointer_send_button(resource, serial, time, button, state);
    }

    // Make sure windows don't get shown or hidden too fast because of a QQuickWindow bug
    // that hangs the client process
    if (!cgrab->inside && state == WL_POINTER_BUTTON_STATE_RELEASED && time - cgrab->creationTime > 500)
        cgrab->end();
}

static const struct weston_pointer_grab_interface shell_surface_interface = {
    shell_surface_focus,
    shell_surface_motion,
    shell_surface_button,
};

static void shell_surface_destroyed(wl_resource *res)
{
    PopupGrab *grab = static_cast<PopupGrab *>(wl_resource_get_user_data(res));
    if (grab->shsurfResource) {
        Shell::endGrab(grab);
        delete grab;
    }
}

DesktopShell::DesktopShell(struct weston_compositor *ec)
            : Shell(ec)
            , m_screenSaverBinding(nullptr)
            , m_screenSaverDuration(5*60*1000)
            , m_inputPanel(nullptr)
            , m_notificationsEdge(DesktopShell::EdgeRight)
            , m_notificationsCornerAnchor(DesktopShell::CornerTopRight)
            , m_notificationsOrder(DesktopShell::OrderNewestFirst)
            , m_notificationsMargin(20)
            , m_prepareEventSent(false)
            , m_locked(false)
            , m_lockSurface(nullptr)
{
    m_screenSaverChild.shell = this;
    m_screenSaverChild.process.pid = 0;
}

void DesktopShell::init()
{
    Shell::init();

    if (!wl_global_create(compositor()->wl_display, &wl_notification_daemon_interface, 1, this,
                          [](struct wl_client *client, void *data, uint32_t version, uint32_t id) { static_cast<DesktopShell *>(data)->bindNotifications(client, version, id); }))
        return;

    if (!wl_global_create(compositor()->wl_display, &wl_hawaii_shell_interface, 1, this,
                          [](struct wl_client *client, void *data, uint32_t version, uint32_t id) { static_cast<DesktopShell *>(data)->bindDesktopShell(client, version, id); }))
        return;

    if (!wl_global_create(compositor()->wl_display, &screensaver_interface, 1, this,
                          [](struct wl_client *client, void *data, uint32_t version, uint32_t id) { static_cast<DesktopShell *>(data)->bindScreenSaver(client, version, id); }))
        return;

    struct wl_event_loop *loop = wl_display_get_event_loop(m_compositor->wl_display);
    m_screenSaverTimer = wl_event_loop_add_timer(loop, [](void *data) {
        return static_cast<DesktopShell *>(data)->screenSaverTimeout(); }, this);

    weston_compositor_add_button_binding(compositor(), BTN_LEFT, MODIFIER_SUPER,
                                         [](struct weston_seat *seat, uint32_t time, uint32_t button, void *data) {
        static_cast<DesktopShell *>(data)->moveBinding(seat, time, button);
    }, this);

    weston_compositor_add_button_binding(compositor(), BTN_MIDDLE, MODIFIER_SUPER,
                                         [](struct weston_seat *seat, uint32_t time, uint32_t button, void *data) {
        static_cast<DesktopShell *>(data)->resizeBinding(seat, time, button);
    }, this);

    weston_compositor_add_key_binding(compositor(), KEY_TAB, MODIFIER_SUPER,
                                      [](struct weston_seat *seat, uint32_t time, uint32_t key, void *data) {
        static_cast<DesktopShell *>(data)->switcherBinding(seat, time, key);
    }, this);

    new ScaleEffect(this);
    //new GridDesktops(this);
    new FadeMovingEffect(this);
    new OpacityEffect(this);
    new ZoomEffect(this);
    //new InOutSurfaceEffect(this);
    new MinimizeEffect(this);

    m_inputPanel = new InputPanel(compositor()->wl_display);
}

void DesktopShell::setGrabCursor(uint32_t cursor)
{
    wl_hawaii_shell_send_grab_cursor(m_child.desktop_shell, cursor);
}

struct BusyGrab : public ShellGrab {
    ShellSurface *surface;
};

static void busy_cursor_grab_focus(struct weston_pointer_grab *base)
{
    BusyGrab *grab = static_cast<BusyGrab *>(container_of(base, ShellGrab, grab));
    wl_fixed_t sx, sy;
    struct weston_surface *surface = weston_compositor_pick_surface(grab->pointer->seat->compositor,
                                                                    grab->pointer->x, grab->pointer->y,
                                                                    &sx, &sy);

    if (grab->surface->weston_surface() != surface) {
        Shell::endGrab(grab);
        delete grab;
    }
}

static void busy_cursor_grab_button(struct weston_pointer_grab *base, uint32_t time, uint32_t button, uint32_t state)
{
    BusyGrab *grab = static_cast<BusyGrab *>(container_of(base, ShellGrab, grab));

    struct weston_seat *seat = grab->pointer->seat;

    if (grab->surface && button == BTN_LEFT && state) {
        ShellSeat::shellSeat(seat)->activate(grab->surface);
        grab->surface->dragMove(seat);
    } else if (grab->surface && button == BTN_RIGHT && state) {
        ShellSeat::shellSeat(seat)->activate(grab->surface);
        //         surface_rotate(grab->surface, &seat->seat);
    }
}

static const struct weston_pointer_grab_interface busy_cursor_grab_interface = {
    busy_cursor_grab_focus,
    [](struct weston_pointer_grab *grab, uint32_t time) {},
    busy_cursor_grab_button,
};

void DesktopShell::setBusyCursor(ShellSurface *surface, struct weston_seat *seat)
{
    BusyGrab *grab = new BusyGrab;
    if (!grab && grab->pointer)
        return;

    grab->pointer = seat->pointer;
    grab->surface = surface;
    startGrab(grab, &busy_cursor_grab_interface, seat, WL_HAWAII_SHELL_CURSOR_BUSY);
}

void DesktopShell::endBusyCursor(struct weston_seat *seat)
{
    ShellGrab *grab = container_of(seat->pointer->grab, ShellGrab, grab);

    if (grab->grab.interface == &busy_cursor_grab_interface) {
        endGrab(grab);
        delete grab;
    }
}

void DesktopShell::sendInitEvents()
{
    for (uint i = 0; i < numWorkspaces(); ++i) {
        workspace(i)->init(m_child.client);
        workspaceAdded(workspace(i));
    }

    for (ShellSurface *shsurf: surfaces()) {
        shsurf->advertize();
    }
}

void DesktopShell::workspaceAdded(Workspace *ws)
{
    wl_hawaii_shell_send_workspace_added(m_child.desktop_shell, ws->resource(), ws->active());
}

void DesktopShell::bindNotifications(wl_client *client, uint32_t version, uint32_t id)
{
    struct wl_resource *resource = wl_resource_create(client, &wl_notification_daemon_interface, version, id);

    if (resource) {
        wl_resource_set_implementation(resource, &m_notificationDaemonImpl, this,
                                       [](struct wl_resource *resource) { static_cast<DesktopShell *>(resource->data)->unbindNotifications(resource); });
        return;
    }

    wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT, "permission to bind wl_notification_daemon denied");
    wl_resource_destroy(resource);
}

void DesktopShell::unbindNotifications(struct wl_resource *resource)
{
    free(resource);
}

void DesktopShell::bindDesktopShell(struct wl_client *client, uint32_t version, uint32_t id)
{
    struct wl_resource *resource = wl_resource_create(client, &wl_hawaii_shell_interface, version, id);

    if (client == m_child.client) {
        wl_resource_set_implementation(resource, &m_desktopShellImpl, this,
                                       [](struct wl_resource *resource) { static_cast<DesktopShell *>(resource->data)->unbindDesktopShell(resource); });
        m_child.desktop_shell = resource;

        sendInitEvents();
        wl_hawaii_shell_send_loaded(resource);
        return;
    }

    wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT, "permission to bind wl_hawaii_shell denied");
    wl_resource_destroy(resource);
}

void DesktopShell::unbindDesktopShell(struct wl_resource *resource)
{
    if (m_locked)
        resumeDesktop();

    m_child.desktop_shell = nullptr;
    m_prepareEventSent = false;
}

void DesktopShell::bindScreenSaver(wl_client *client, uint32_t version, uint32_t id)
{
    struct wl_resource *resource = wl_resource_create(client, &screensaver_interface, version, id);

    if (!m_screenSaverBinding) {
        wl_resource_set_implementation(resource, &m_screenSaverImpl, this,
                                       [](struct wl_resource *resource) { static_cast<DesktopShell *>(resource->data)->unbindScreenSaver(resource); });
        m_screenSaverBinding = resource;
        return;
    }

    wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT, "permission to bind wl_notification_daemon denied");
    wl_resource_destroy(resource);
}

void DesktopShell::unbindScreenSaver(struct wl_resource *resource)
{
    m_screenSaverBinding = nullptr;
}

void DesktopShell::moveBinding(struct weston_seat *seat, uint32_t time, uint32_t button)
{
    struct weston_surface *surface = seat->pointer->focus;
    if (!surface) {
        return;
    }

    ShellSurface *shsurf = getShellSurface(surface);
    if (!shsurf || shsurf->type() == ShellSurface::Type::Fullscreen || shsurf->type() == ShellSurface::Type::Maximized) {
        return;
    }

    shsurf = shsurf->topLevelParent();
    if (shsurf) {
        shsurf->dragMove(seat);
    }
}

void DesktopShell::resizeBinding(weston_seat *seat, uint32_t time, uint32_t button)
{
    weston_surface *surface = weston_surface_get_main_surface(seat->pointer->focus);
    if (!surface) {
        return;
    }

    ShellSurface *shsurf = getShellSurface(surface);
    if (!shsurf || shsurf->type() == ShellSurface::Type::Fullscreen || shsurf->type() == ShellSurface::Type::Maximized) {
        return;
    }

    shsurf = shsurf->topLevelParent();
    if (shsurf) {
        int32_t x, y;
        weston_surface_from_global(surface, wl_fixed_to_int(seat->pointer->grab_x), wl_fixed_to_int(seat->pointer->grab_y), &x, &y);

        pixman_box32_t *bbox = pixman_region32_extents(&surface->input);

        uint32_t edges = 0;
        int32_t w = surface->geometry.width / 3;
        if (w > 20) w = 20;
        w += bbox->x1;

        if (x < w)
            edges |= WL_SHELL_SURFACE_RESIZE_LEFT;
        else if (x < surface->geometry.width - w)
            edges |= 0;
        else
            edges |= WL_SHELL_SURFACE_RESIZE_RIGHT;

        int32_t h = surface->geometry.height / 3;
        if (h > 20) h = 20;
        h += bbox->y1;
        if (y < h)
            edges |= WL_SHELL_SURFACE_RESIZE_TOP;
        else if (y < surface->geometry.height - h)
            edges |= 0;
        else
            edges |= WL_SHELL_SURFACE_RESIZE_BOTTOM;

        shsurf->dragResize(seat, edges);
    }
}

class Switcher
{
public:
    Switcher()
    {
    }

    void destroy()
    {
        weston_keyboard *keyboard = grab.keyboard;

        for (weston_surface *surface: shell->currentWorkspace()->layer()) {
            surface->alpha = 1.f;
            weston_surface_damage(surface);
        }

        if (current) {
            ShellSurface *shsurf = shell->getShellSurface(current);
            if (shsurf)
                shsurf->activate();
        }
        wl_list_remove(&listener.link);
        weston_keyboard_end_grab(keyboard);
        if (keyboard->input_method_resource)
            keyboard->grab = &keyboard->input_method_grab;

        wl_hawaii_shell_send_window_switching_finished(shell->shellClientResource());
    }

    void switchNext()
    {
        wl_hawaii_shell_send_window_switching_started(shell->shellClientResource());

        weston_surface *first = nullptr, *prev = nullptr, *next = nullptr;

        for (weston_surface *surface: shell->currentWorkspace()->layer()) {
            ShellSurface *shsurf = shell->getShellSurface(surface);
            if (!shsurf)
                continue;

            switch (shsurf->type()) {
            case ShellSurface::Type::TopLevel:
            case ShellSurface::Type::Fullscreen:
            case ShellSurface::Type::Maximized:
                if (!first)
                    first = surface;
                if (prev == current)
                    next = surface;
                prev = surface;
                surface->alpha = 0.25f;
                weston_surface_geometry_dirty(surface);
                weston_surface_damage(surface);
                break;
            default:
                break;
            }

            if (shell->isBlackSurface(surface, nullptr)) {
                surface->alpha = 0.25f;
                weston_surface_geometry_dirty(surface);
                weston_surface_damage(surface);
            }
        }

        if (!next)
            next = first;
        if (!next)
            return;

        wl_list_remove(&listener.link);
        wl_signal_add(&next->destroy_signal, &listener);

        current = next;
        next->alpha = 1.f;

        ShellSurface *shsurf = shell->getShellSurface(next);
        if (shsurf)
            wl_hawaii_shell_send_window_switched(shell->shellClientResource(),
                                                 shsurf->windowResource());
    }

    static void handleSurfaceDestroy(wl_listener *l, void *data)
    {
        Switcher *switcher = container_of(l, Switcher, listener);
        switcher->switchNext();
    }

    DesktopShell *shell;
    weston_surface *current;
    wl_listener listener;
    weston_keyboard_grab grab;
};

static void switcherKey(weston_keyboard_grab *grab,
                        uint32_t time, uint32_t key, uint32_t state_w)
{
    Switcher *switcher = container_of(grab, Switcher, grab);
    enum wl_keyboard_key_state state = (enum wl_keyboard_key_state)state_w;

    if (key == KEY_TAB && state == WL_KEYBOARD_KEY_STATE_PRESSED)
        switcher->switchNext();
}

static void switcherModifier(weston_keyboard_grab *grab,
                             uint32_t serial, uint32_t mods_depressed,
                             uint32_t mods_latched, uint32_t mods_locked,
                             uint32_t group)
{
    Switcher *switcher = container_of(grab, Switcher, grab);
    weston_seat *seat = grab->keyboard->seat;

    if ((seat->modifier_state & MODIFIER_SUPER) == 0) {
        switcher->destroy();
        delete switcher;
    }
}

static const struct weston_keyboard_grab_interface switcherGrab = {
    switcherKey,
    switcherModifier
};

void DesktopShell::switcherBinding(weston_seat *seat, uint32_t time, uint32_t button)
{
    Switcher *switcher = new Switcher();
    switcher->shell = this;
    switcher->current = nullptr;
    switcher->listener.notify = Switcher::handleSurfaceDestroy;
    wl_list_init(&switcher->listener.link);

    switcher->grab.interface = &switcherGrab;
    weston_keyboard_start_grab(seat->keyboard, &switcher->grab);
    weston_keyboard_set_focus(seat->keyboard, nullptr);
    switcher->switchNext();
}

void DesktopShell::addKeyBinding(struct wl_client *client, struct wl_resource *resource, uint32_t id, uint32_t key, uint32_t modifiers)
{
    wl_resource *res = wl_resource_create(client, &wl_hawaii_key_binding_interface, wl_resource_get_version(resource), id);
    wl_resource_set_implementation(res, nullptr, res, [](wl_resource *) {});

    weston_compositor_add_key_binding(compositor(), key, (weston_keyboard_modifier)modifiers,
                                      [](struct weston_seat *seat, uint32_t time, uint32_t key, void *data) {

        wl_hawaii_key_binding_send_triggered(static_cast<wl_resource *>(data));
    }, res);
}

void DesktopShell::lockSession()
{
    if (m_locked) {
        weston_compositor_sleep(m_compositor);
        return;
    }

    m_locked = true;

    // Hide all surfaces by removing all layers except for backgrounds,
    // this way nothing else can show or receive input events while
    // we are locked.
    // We don't remove the backgrounds layer because currently we only
    // show the lock screen on the default output so we need to show
    // something on the other outputs as well as the default one.
    // However this is safe because our background only shows a wallpaper
    // so it won't do much with input events, in the future we need to
    // show the lock screen on all outpus and remove the backgrounds
    // layer as well.
    // TODO: Proper multiscreen setup
    currentWorkspace()->reset();
    m_lockLayer.insert(&compositor()->cursor_layer);
    m_backgroundLayer.insert(&m_lockLayer);

    // TODO: Disable bindings that are not supposed to work while locked

    // Run screensaver or sleep
    launchScreenSaverProcess();

    // All this must be undone in resumeDesktop()
}

void DesktopShell::unlockSession()
{
    weston_log("unlocking session...\n");

    if (!m_locked || m_lockSurface) {
        fadeIn();
        return;
    }

    // If the client has gone away, unlock immediately
    if (!shellClientResource()) {
        resumeDesktop();
        return;
    }

    if (m_prepareEventSent)
        return;

    weston_log("preparing lock surface...\n");
    weston_compositor_damage_all(m_compositor);
    wl_hawaii_shell_send_prepare_lock_surface(shellClientResource());
    m_prepareEventSent = true;
}

void DesktopShell::resumeDesktop()
{
    terminateScreenSaverProcess();

    m_lockLayer.reset();
    m_dialogsLayer.insert(&m_compositor->cursor_layer);
    m_overlayLayer.insert(&m_dialogsLayer);
    m_notificationsLayer.insert(&m_overlayLayer);
    m_fullscreenLayer.insert(&m_notificationsLayer);
    m_panelsLayer.insert(&m_fullscreenLayer);
    m_backgroundLayer.insert(&m_panelsLayer);
    currentWorkspace()->insert(&m_panelsLayer);

    m_locked = false;
    fadeIn();
    weston_compositor_damage_all(m_compositor);
}

void DesktopShell::lockSurfaceDestroy()
{
    weston_log("lock surface gone\n");
    m_lockSurface = nullptr;
}

void DesktopShell::setAvailableGeometry(struct wl_client *client, struct wl_resource *resource,
                                        struct wl_resource *output_resource,
                                        int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct weston_output *output = static_cast<weston_output *>(output_resource->data);

    IRect2D area(x, y, width, height);
    m_windowsArea[output] = area;
}

static void configure_static_surface(struct weston_surface *es, Layer *layer, int32_t width, int32_t height)
{
    if (width == 0)
        return;

    weston_surface_configure(es, es->geometry.x, es->geometry.y, width, height);

    if (wl_list_empty(&es->layer_link) || es->layer_link.next == es->layer_link.prev) {
        layer->addSurface(es);
        weston_compositor_schedule_repaint(es->compositor);
    }
}

void DesktopShell::setBackground(struct wl_client *client, struct wl_resource *resource, struct wl_resource *output_resource,
                                 struct wl_resource *surface_resource)
{
    struct weston_surface *surface = static_cast<weston_surface *>(surface_resource->data);
    if (surface->configure) {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "surface role already assigned");
        return;
    }

    surface->configure = [](struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height) {
        configure_static_surface(es, &static_cast<DesktopShell *>(es->configure_private)->m_backgroundLayer, width, height); };
    surface->configure_private = this;
    surface->output = static_cast<weston_output *>(output_resource->data);
}

void DesktopShell::setPanel(struct wl_client *client, struct wl_resource *resource, struct wl_resource *output_resource,
                            struct wl_resource *surface_resource)
{
    struct weston_surface *surface = static_cast<struct weston_surface *>(surface_resource->data);

    if (surface->configure) {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "surface role already assigned");
        return;
    }

    surface->configure = [](struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height) {
        configure_static_surface(es, &static_cast<DesktopShell *>(es->configure_private)->m_panelsLayer, width, height); };
    surface->configure_private = this;
    surface->output = static_cast<weston_output *>(output_resource->data);
}

void DesktopShell::setLauncher(struct wl_client *client, struct wl_resource *resource, struct wl_resource *output_resource,
                               struct wl_resource *surface_resource)
{
    struct weston_surface *surface = static_cast<struct weston_surface *>(surface_resource->data);

    if (surface->configure) {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "surface role already assigned");
        return;
    }

    surface->configure = [](struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height) {
        configure_static_surface(es, &static_cast<DesktopShell *>(es->configure_private)->m_panelsLayer, width, height); };
    surface->configure_private = this;
    surface->output = static_cast<weston_output *>(output_resource->data);
}

void DesktopShell::setSpecial(struct wl_client *client, struct wl_resource *resource,
                              struct wl_resource *output_resource,
                              struct wl_resource *surface_resource)
{
    struct weston_surface *surface = static_cast<struct weston_surface *>(surface_resource->data);

    if (surface->configure) {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "surface role already assigned");
        return;
    }

    surface->configure = [](struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height) {
        configure_static_surface(es, &static_cast<DesktopShell *>(es->configure_private)->m_panelsLayer, width, height); };
    surface->configure_private = this;
    surface->output = static_cast<weston_output *>(output_resource->data);
}

void DesktopShell::setOverlay(struct wl_client *client, struct wl_resource *resource, struct wl_resource *output_resource, struct wl_resource *surface_resource)
{
    struct weston_surface *surface = static_cast<weston_surface *>(surface_resource->data);
    if (surface->configure) {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "surface role already assigned");
        return;
    }

    surface->configure = [](struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height) {
        configure_static_surface(es, &static_cast<DesktopShell *>(es->configure_private)->m_overlayLayer, width, height); };
    surface->configure_private = this;
    surface->output = static_cast<weston_output *>(output_resource->data);

    pixman_region32_fini(&surface->pending.input);
    pixman_region32_init_rect(&surface->pending.input, 0, 0, 0, 0);
}

void DesktopShell::setPopup(struct wl_client *client, struct wl_resource *resource,
                            uint32_t id,
                            struct wl_resource *output_resource,
                            struct wl_resource *surface_resource,
                            int32_t x, int32_t y)
{
    struct weston_surface *surface = static_cast<struct weston_surface *>(surface_resource->data);
    if (!surface->configure) {
        surface->configure = [](struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height) {
            configure_static_surface(es, &static_cast<DesktopShell *>(es->configure_private)->m_panelsLayer, width, height); };
        surface->configure_private = this;
        surface->output = static_cast<weston_output *>(output_resource->data);
    }

    weston_surface_set_position(surface, x, y);

    PopupGrab *grab = new PopupGrab;
    if (!grab)
        return;

    grab->shsurfResource = wl_resource_create(client, &wl_hawaii_shell_surface_interface, wl_resource_get_version(resource), id);
    wl_resource_set_implementation(grab->shsurfResource, &PopupGrab::m_shellSurfaceImpl, grab,
                                   [](struct wl_resource *resource){});
    wl_resource_set_destructor(grab->shsurfResource, shell_surface_destroyed);
    wl_resource_set_user_data(grab->shsurfResource, grab);

    weston_seat *seat = container_of(compositor()->seat_list.next, weston_seat, link);
    grab->pointer = seat->pointer;
    grab->surface = surface;
    grab->creationTime = grab->pointer->grab_time;

    ShellSeat::shellSeat(seat)->endPopupGrab();

    wl_fixed_t sx, sy;
    weston_surface_from_global_fixed(surface, grab->pointer->x, grab->pointer->y, &sx, &sy);
    weston_pointer_set_focus(grab->pointer, surface, sx, sy);

    grab->grab.interface = &shell_surface_interface;
    weston_pointer_start_grab(grab->pointer, &grab->grab);
}

void DesktopShell::setDialog(struct wl_client *client, struct wl_resource *resource,
                             struct wl_resource *output_resource,
                             struct wl_resource *surface_resource)
{
    struct weston_surface *surface = static_cast<struct weston_surface *>(surface_resource->data);

    if (surface->configure) {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "surface role already assigned");
        return;
    }

    surface->configure = [](struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height) {
        if (width == 0)
            return;

        ShellWindow *window = static_cast<ShellWindow *>(es->configure_private);
        window->shell()->centerSurfaceOnOutput(es, es->output);
        if (weston_surface_is_mapped(es))
            window->animateDialog(es);

        if (wl_list_empty(&es->layer_link) || es->layer_link.next == es->layer_link.prev) {
            // Create a black translucent surface to prevent underlying layers to get input events
            if (!window->dimmedSurface()) {
                window->createDimmedSurface(es->output);
                window->shell()->m_dialogsLayer.addSurface(window->dimmedSurface());
            }

            // Add the dialog surface and stack it above the dimmed surface
            window->shell()->m_dialogsLayer.addSurface(es);
            window->shell()->m_dialogsLayer.stackAbove(es, window->dimmedSurface());

            // Give focus to the dialog
            weston_seat *seat = container_of(es->compositor->seat_list.next, weston_seat, link);
            weston_surface_activate(es, seat);
        }
    };
    surface->output = static_cast<weston_output *>(output_resource->data);

    ShellWindow *window = new ShellWindow(this);
    window->connectSignal(&surface->destroy_signal);
    surface->configure_private = window;
}

void DesktopShell::setPosition(struct wl_client *client, struct wl_resource *resource,
                               struct wl_resource *surface_resource,
                               int32_t x, int32_t y)
{
    struct weston_surface *surface = static_cast<struct weston_surface *>(surface_resource->data);

    if (!surface->configure) {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "surface role not yet assigned");
        return;
    }

    // Set given position
    weston_surface_set_position(surface, x, y);
}

void DesktopShell::lockSurfaceConfigure(weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height)
{
    if (width == 0)
        return;

    es->geometry.width = width;
    es->geometry.height = height;

    if (wl_list_empty(&es->layer_link) || es->layer_link.next == es->layer_link.prev) {
        m_lockLayer.addSurface(es);
        weston_surface_update_transform(es);
        fadeIn();
    }
}

void DesktopShell::setLockSurface(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface_resource)
{
    struct weston_surface *surface = static_cast<struct weston_surface *>(surface_resource->data);

    m_prepareEventSent = false;

    if (!m_locked)
        return;

    m_lockSurface = surface;

    m_lockSurfaceDestroyListener.listen(&surface->destroy_signal);
    m_lockSurfaceDestroyListener.signal->connect(this, &DesktopShell::lockSurfaceDestroy);

    surface->configure = [](struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height) {
        static_cast<DesktopShell *>(es->configure_private)->lockSurfaceConfigure(es, sx, sy, width, height); };
    surface->configure_private = this;
}

void DesktopShell::quit(wl_client *client, wl_resource *resource)
{
    Shell::quit();
}

void DesktopShell::lock(wl_client *client, wl_resource *resource)
{
    // Don't lock again if we are already locked
    if (m_locked || m_lockSurface)
        return;

    wl_signal_emit(&m_compositor->idle_signal, nullptr);
}

void DesktopShell::unlock(struct wl_client *client, struct wl_resource *resource)
{
    m_prepareEventSent = false;

    if (m_locked)
        resumeDesktop();
}

void DesktopShell::setGrabSurface(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface_resource)
{
    this->Shell::setGrabSurface(static_cast<struct weston_surface *>(surface_resource->data));
}

void DesktopShell::desktopReady(struct wl_client *client, struct wl_resource *resource)
{
    fadeIn();
}

void DesktopShell::addWorkspace(wl_client *client, wl_resource *resource)
{
    Workspace *ws = new Workspace(this, numWorkspaces());
    ws->init(client);
    Shell::addWorkspace(ws);
    workspaceAdded(ws);
}

void DesktopShell::selectWorkspace(wl_client *client, wl_resource *resource, wl_resource *workspace_resource)
{
    Shell::selectWorkspace(Workspace::fromResource(workspace_resource)->number());
}

class ClientGrab : public ShellGrab {
public:
    wl_resource *resource;
    weston_surface *focus;
    bool pressed;
};

static void client_grab_focus(struct weston_pointer_grab *base)
{
    ShellGrab *grab = container_of(base, ShellGrab, grab);
    ClientGrab *cgrab = static_cast<ClientGrab *>(grab);

    wl_fixed_t sx, sy;
    struct weston_surface *surface = weston_compositor_pick_surface(grab->pointer->seat->compositor,
                                                                    grab->pointer->x, grab->pointer->y,
                                                                    &sx, &sy);
    if (cgrab->focus != surface) {
        cgrab->focus = surface;
        wl_hawaii_grab_send_focus(cgrab->resource, surface->resource, sx, sy);
    }
}

static void client_grab_motion(struct weston_pointer_grab *base, uint32_t time)
{
    ShellGrab *grab = container_of(base, ShellGrab, grab);
    ClientGrab *cgrab = static_cast<ClientGrab *>(grab);

    wl_fixed_t sx = cgrab->pointer->x;
    wl_fixed_t sy = cgrab->pointer->y;
    if (cgrab->focus) {
        weston_surface_from_global_fixed(cgrab->focus, cgrab->pointer->x, cgrab->pointer->y, &sx, &sy);
    }

    wl_hawaii_grab_send_motion(cgrab->resource, time, sx, sy);
}

static void client_grab_button(struct weston_pointer_grab *base, uint32_t time, uint32_t button, uint32_t state)
{
    ShellGrab *grab = container_of(base, ShellGrab, grab);
    ClientGrab *cgrab = static_cast<ClientGrab *>(grab);

    // Send the event to the application as normal if the mouse was pressed initially.
    // The application has to know the button was released, otherwise its internal state
    // will be inconsistent with the physical button state.
    // Eat the other events, as the app doesn't need to know them.
    // NOTE: this works only if there is only 1 button pressed initially. i can know how many button
    // are pressed but weston currently has no API to determine which ones they are.
#if (WESTON_VERSION_NUMBER >= WESTON_VERSION_CHECK(1, 3, 0))
    wl_resource *resource;
    wl_resource_for_each(resource, &grab->pointer->focus_resource_list) {
#else
    wl_resource *resource = grab->pointer->focus_resource;
    if (resource) {
#endif
        if (cgrab->pressed && button == grab->pointer->grab_button) {
            wl_display *display = wl_client_get_display(wl_resource_get_client(resource));
            uint32_t serial = wl_display_next_serial(display);
            wl_pointer_send_button(resource, serial, time, button, state);
            cgrab->pressed = false;
        }
    }

    wl_hawaii_grab_send_button(cgrab->resource, time, button, state);
}

static const struct weston_pointer_grab_interface client_grab_interface = {
    client_grab_focus,
    client_grab_motion,
    client_grab_button,
};

void client_grab_end(wl_client *client, wl_resource *resource)
{
    ClientGrab *cg = static_cast<ClientGrab *>(resource->data);
    Shell::endGrab(cg);
    weston_output_schedule_repaint(cg->pointer->focus->output);
}

static const struct wl_hawaii_grab_interface hawaii_grab_implementation = {
    client_grab_end
};

void DesktopShell::createGrab(wl_client *client, wl_resource *resource, uint32_t id)
{
    wl_resource *res = wl_resource_create(client, &wl_hawaii_grab_interface, wl_resource_get_version(resource), id);

    ClientGrab *grab = new ClientGrab;
    wl_resource_set_implementation(res, &hawaii_grab_implementation, grab, [](wl_resource *) {});

    if (!grab)
        return;

    weston_seat *seat = container_of(compositor()->seat_list.next, weston_seat, link);
    grab->pointer = seat->pointer;
    grab->resource = res;
    grab->shell = this;
    grab->pressed = seat->pointer->button_count > 0;

    ShellSeat::shellSeat(seat)->endPopupGrab();

    wl_fixed_t sx, sy;
    struct weston_surface *surface = weston_compositor_pick_surface(compositor(),
                                                                    grab->pointer->x, grab->pointer->y,
                                                                    &sx, &sy);
    weston_pointer_set_focus(grab->pointer, surface, sx, sy);
    grab->focus = surface;
    wl_hawaii_grab_send_focus(grab->resource, surface->resource, sx, sy);

    grab->grab.interface = &client_grab_interface;
    weston_pointer_start_grab(grab->pointer, &grab->grab);
}

const struct wl_hawaii_shell_interface DesktopShell::m_desktopShellImpl = {
    wrapInterface(&DesktopShell::addKeyBinding),
    wrapInterface(&DesktopShell::setAvailableGeometry),
    wrapInterface(&DesktopShell::setBackground),
    wrapInterface(&DesktopShell::setPanel),
    wrapInterface(&DesktopShell::setLauncher),
    wrapInterface(&DesktopShell::setSpecial),
    wrapInterface(&DesktopShell::setOverlay),
    wrapInterface(&DesktopShell::setPopup),
    wrapInterface(&DesktopShell::setDialog),
    wrapInterface(&DesktopShell::setPosition),
    wrapInterface(&DesktopShell::setLockSurface),
    wrapInterface(&DesktopShell::quit),
    wrapInterface(&DesktopShell::lock),
    wrapInterface(&DesktopShell::unlock),
    wrapInterface(&DesktopShell::setGrabSurface),
    wrapInterface(&DesktopShell::desktopReady),
    wrapInterface(&Shell::minimizeWindows),
    wrapInterface(&Shell::restoreWindows),
    wrapInterface(&DesktopShell::addWorkspace),
    wrapInterface(&DesktopShell::selectWorkspace),
    wrapInterface(&DesktopShell::createGrab)
};

void DesktopShell::notificationConfigure(weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height)
{
    if (width == 0)
        return;

    es->geometry.width = weston_surface_buffer_width(es);
    es->geometry.height = weston_surface_buffer_height(es);

    mapNotificationSurfaces();

    weston_compositor_schedule_repaint(m_compositor);
}

void DesktopShell::mapNotificationSurfaces()
{
    bool isRight = m_notificationsCornerAnchor & DesktopShell::EdgeRight;
    bool isBottom = m_notificationsCornerAnchor & DesktopShell::EdgeBottom;

    IRect2D availableGeometry = windowsArea(getDefaultOutput());
    int32_t left = availableGeometry.x;
    int32_t top = availableGeometry.y;
    int32_t right = availableGeometry.x + availableGeometry.width;
    int32_t bottom = availableGeometry.y + availableGeometry.height;
    int32_t x, y;

    if (isRight)
        x = right - m_notificationsMargin;
    else
        x = left + m_notificationsMargin;
    if (isBottom)
        y = bottom - m_notificationsMargin;
    else
        y = top + m_notificationsMargin;

    for (struct weston_surface *surface: m_notificationsLayer) {
        if (isBottom)
            y -= surface->geometry.height;
        if (isRight)
            x -= surface->geometry.width;
        weston_surface_set_position(surface, x, y);
        if (isRight)
            x += surface->geometry.width;
        if (isBottom)
            y -= m_notificationsMargin;
        else
            y += surface->geometry.height + m_notificationsMargin;
    }
}

void DesktopShell::addNotificationSurface(wl_client *client, wl_resource *resource,
                                          wl_resource *surface_resource)
{
    struct weston_surface *surface = static_cast<weston_surface *>(surface_resource->data);
    if (surface->configure) {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "surface role already assigned");
        return;
    }

    surface->geometry.width = weston_surface_buffer_width(surface);
    surface->geometry.height = weston_surface_buffer_height(surface);

    surface->configure = [](struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height) {
        static_cast<DesktopShell *>(es->configure_private)->notificationConfigure(es, sx, sy, width, height); };
    surface->configure_private = this;
    surface->output = getDefaultOutput();

    if (m_notificationsOrder == DesktopShell::OrderNewestFirst)
        m_notificationsLayer.addSurface(surface);
    else
        m_notificationsLayer.prependSurface(surface);
    mapNotificationSurfaces();

    weston_compositor_schedule_repaint(m_compositor);
}

const struct wl_notification_daemon_interface DesktopShell::m_notificationDaemonImpl = {
    wrapInterface(&DesktopShell::addNotificationSurface)
};

int DesktopShell::screenSaverTimeout()
{
    weston_log("screensaver timeout...\n");
    fadeOut();
    return 1;
}

void DesktopShell::screenSaverSigChild(int status)
{
    m_screenSaverChild.process.pid = 0;
    m_screenSaverChild.client = nullptr; // already destroyed by wayland

    if (m_locked)
        weston_compositor_sleep(m_compositor);
}

void DesktopShell::launchScreenSaverProcess()
{
    if (m_screenSaverBinding)
        return;

    if (m_screenSaverPath.empty()) {
        weston_compositor_sleep(m_compositor);
        return;
    }

    if (m_screenSaverChild.process.pid != 0) {
        weston_log("old screensaver still running\n");
        return;
    }

    m_screenSaverChild.client = weston_client_launch(m_compositor,
                                                     &m_screenSaverChild.process,
                                                     m_screenSaverPath.c_str(),
                                                     [](struct weston_process *process, int status) {
        ScreenSaverChild *child = container_of(process, ScreenSaverChild, process);
        child->shell->screenSaverSigChild(status);
    });

    if (!m_screenSaverChild.client)
        weston_log("not able to start %s\n", m_screenSaverPath.c_str());
}

void DesktopShell::terminateScreenSaverProcess()
{
    if (m_screenSaverChild.process.pid == 0)
        return;

    ::kill(m_screenSaverChild.process.pid, SIGTERM);
}

void DesktopShell::screenSaverConfigure(weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height)
{
    if (width == 0)
        return;

    // Starting screensaver beforehand doesn't work
    if (m_locked)
        return;

    centerSurfaceOnOutput(es, es->output);

    if (wl_list_empty(&es->layer_link)) {
        m_lockLayer.prependSurface(es);
        weston_surface_update_transform(es);
        wl_event_source_timer_update(m_screenSaverTimer, m_screenSaverDuration);
        fadeIn();
    }
}

void DesktopShell::setScreenSaverSurface(wl_client *client, wl_resource *resource,
                                         wl_resource *surface_resource,
                                         wl_resource *output_resource)
{
    struct weston_surface *surface = static_cast<weston_surface *>(surface_resource->data);
    struct weston_output *output = static_cast<weston_output *>(output_resource->data);

    surface->configure = [](struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height) {
        static_cast<DesktopShell *>(es->configure_private)->screenSaverConfigure(es, sx, sy, width, height); };
    surface->configure_private = this;
    surface->output = output;
}

const struct screensaver_interface DesktopShell::m_screenSaverImpl = {
    wrapInterface(&DesktopShell::setScreenSaverSurface)
};

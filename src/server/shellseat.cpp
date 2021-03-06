/****************************************************************************
 * This file is part of Hawaii Shell.
 *
 * Copyright (C) 2013 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 * Copyright (C) 2013 Giulio Camuffo <giuliocamuffo@gmail.com>
 *
 * Author(s):
 *    Giulio Camuffo
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

#include <weston/compositor.h>

#include "shellseat.h"
#include "shellsurface.h"
#include "workspace.h"
#include "shell.h"
#include "weston-version.h"

class FocusState {
public:
    FocusState(ShellSeat *seat)
        : seat(seat)
        , surface(nullptr)
    {
    }

    void setFocus(ShellSurface *surf) {
        if (surface) {
            surface->destroyedSignal.disconnect(this, &FocusState::surfaceDestroyed);
            surface->setActive(false);
        }
        if (surf) {
            surf->destroyedSignal.connect(this, &FocusState::surfaceDestroyed);
            surf->setActive(true);
        }
        surface = surf;
    }

    void surfaceDestroyed() {
        if (surface->workspace()) {
            for (const weston_surface *surf: surface->workspace()->layer()) {
                if (surf != surface->weston_surface()) {
                    ShellSurface *shsurf = Shell::getShellSurface(surf);
                    if (shsurf) {
                        seat->activate(shsurf);
                        return;
                    }
                }
            }
        }
        surface = nullptr;
    }

    ShellSeat *seat;
    ShellSurface *surface;
};

ShellSeat::ShellSeat(struct weston_seat *seat)
         : m_seat(seat)
         , m_focusState(new FocusState(this))
{
    m_popupGrab.client = nullptr;
    m_popupGrab.seat = this;
    m_listeners.seat = this;

    m_listeners.seatDestroy.notify = seatDestroyed;
    wl_signal_add(&seat->destroy_signal, &m_listeners.seatDestroy);

    if (seat->pointer) {
        m_listeners.focus.notify = pointerFocus;
        wl_signal_add(&seat->pointer->focus_signal, &m_listeners.focus);
    } else {
        wl_list_init(&m_listeners.focus.link);
    }
}

ShellSeat::~ShellSeat()
{
    if (m_popupGrab.client) {
        weston_pointer_end_grab(m_popupGrab.grab.pointer);
    }
    wl_list_remove(&m_listeners.seatDestroy.link);
    wl_list_remove(&m_listeners.focus.link);
}

ShellSeat *ShellSeat::shellSeat(struct weston_seat *seat)
{
    struct wl_listener *listener = wl_signal_get(&seat->destroy_signal, ShellSeat::seatDestroyed);
    if (!listener) {
        return new ShellSeat(seat);
    }

    return static_cast<Wrapper *>(container_of(listener, Wrapper, seatDestroy))->seat;
}

void ShellSeat::activate(ShellSurface *shsurf)
{
    weston_surface_activate(shsurf ? shsurf->weston_surface() : nullptr, m_seat);
    if (shsurf && shsurf->workspace()) {
        shsurf->workspace()->restack(shsurf);
    }
    m_focusState->setFocus(shsurf);
}

void ShellSeat::activate(weston_surface *surf)
{
    weston_surface_activate(surf, m_seat);
    ShellSurface *shsurf = surf ? Shell::getShellSurface(surf) : nullptr;
    if (shsurf && shsurf->workspace()) {
        shsurf->workspace()->restack(shsurf);
    }
    m_focusState->setFocus(shsurf);
}

ShellSurface *ShellSeat::currentFocus() const
{
    return  m_focusState->surface;
}

void ShellSeat::seatDestroyed(struct wl_listener *listener, void *data)
{
    ShellSeat *shseat = static_cast<Wrapper *>(container_of(listener, Wrapper, seatDestroy))->seat;
    delete shseat;
}

void ShellSeat::pointerFocus(struct wl_listener *listener, void *data)
{
    ShellSeat *shseat = static_cast<Wrapper *>(container_of(listener, Wrapper, focus))->seat;
    struct weston_pointer *pointer = static_cast<weston_pointer *>(data);
    shseat->pointerFocusSignal(shseat, pointer);
}

void ShellSeat::popup_grab_focus(struct weston_pointer_grab *grab)
{
    struct weston_pointer *pointer = grab->pointer;
    ShellSeat *shseat = static_cast<PopupGrab *>(container_of(grab, PopupGrab, grab))->seat;

    wl_fixed_t sx, sy;
    struct weston_surface *surface = weston_compositor_pick_surface(pointer->seat->compositor,
                                                                    pointer->x, pointer->y,
                                                                    &sx, &sy);

    if (surface && wl_resource_get_client(surface->resource) == shseat->m_popupGrab.client) {
        weston_pointer_set_focus(pointer, surface, sx, sy);
    } else {
        weston_pointer_set_focus(pointer, NULL, wl_fixed_from_int(0), wl_fixed_from_int(0));
    }
}

static void popup_grab_motion(struct weston_pointer_grab *grab,  uint32_t time)
{
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

void ShellSeat::popup_grab_button(struct weston_pointer_grab *grab, uint32_t time, uint32_t button, uint32_t state_w)
{
    ShellSeat *shseat = static_cast<PopupGrab *>(container_of(grab, PopupGrab, grab))->seat;
    struct wl_display *display = shseat->m_seat->compositor->wl_display;

#if (WESTON_VERSION_NUMBER >= WESTON_VERSION_CHECK(1, 3, 0))
    struct wl_list *resource_list = &grab->pointer->focus_resource_list;
    if (!wl_list_empty(resource_list)) {
        struct wl_resource *resource;
#else
    struct wl_resource *resource = grab->pointer->focus_resource;
    if (resource) {
#endif
        uint32_t serial = wl_display_get_serial(display);
#if (WESTON_VERSION_NUMBER >= WESTON_VERSION_CHECK(1, 3, 0))
        wl_resource_for_each(resource, resource_list)
            wl_pointer_send_button(resource, serial, time, button, state_w);
#else
        wl_pointer_send_button(resource, serial, time, button, state_w);
#endif
    } else if (state_w == WL_POINTER_BUTTON_STATE_RELEASED &&
              (shseat->m_popupGrab.initial_up || time - shseat->m_seat->pointer->grab_time > 500)) {
        shseat->endPopupGrab();
    }

    if (state_w == WL_POINTER_BUTTON_STATE_RELEASED)
        shseat->m_popupGrab.initial_up = 1;
}

const struct weston_pointer_grab_interface ShellSeat::popup_grab_interface = {
    ShellSeat::popup_grab_focus,
    popup_grab_motion,
    ShellSeat::popup_grab_button,
};

bool ShellSeat::addPopupGrab(ShellSurface *surface, uint32_t serial)
{
    if (serial == m_seat->pointer->grab_serial) {
        if (m_popupGrab.surfaces.empty()) {
            m_popupGrab.client = surface->client();
            m_popupGrab.grab.interface = &popup_grab_interface;
            /* We must make sure here that this popup was opened after
            * a mouse press, and not just by moving around with other
            * popups already open. */
            if (m_seat->pointer->button_count > 0) {
                m_popupGrab.initial_up = 0;
            }

            weston_pointer_start_grab(m_seat->pointer, &m_popupGrab.grab);
        }
        m_popupGrab.surfaces.push_back(surface);

        return true;
    }

    m_popupGrab.client = nullptr;
    return false;
}

void ShellSeat::removePopupGrab(ShellSurface *surface)
{
    m_popupGrab.surfaces.remove(surface);
    if (m_popupGrab.surfaces.empty()) {
        if (m_popupGrab.client)
            weston_pointer_end_grab(m_popupGrab.grab.pointer);
        m_popupGrab.client = nullptr;
    }
}

void ShellSeat::endPopupGrab()
{
    struct weston_pointer *pointer = m_popupGrab.grab.pointer;
    if (m_popupGrab.client) {
        weston_pointer_end_grab(pointer->grab->pointer);
        m_popupGrab.client = nullptr;
        /* Send the popup_done event to all the popups open */
        for (ShellSurface *shsurf: m_popupGrab.surfaces) {
            shsurf->popupDone();
        }
        m_popupGrab.surfaces.clear();
    }
}

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

#ifndef GRIDDESKTOPS_H
#define GRIDDESKTOPS_H

#include <list>

#include "effect.h"

class ShellGrab;
class Animation;
class Binding;

class GridDesktops : public Effect
{
public:
    GridDesktops(Shell *shell);
    ~GridDesktops();

    virtual void run(struct weston_seat *seat, uint32_t time, uint32_t key);
    void end(ShellSurface *surface);

private:
    void run(struct weston_seat *ws);

    bool m_scaled;
    struct weston_seat *m_seat;
    struct Grab *m_grab;
    Binding *m_binding;
    int m_setWs;

    static void grab_focus(struct weston_pointer_grab *grab);
    static void grab_button(struct weston_pointer_grab *base, uint32_t time, uint32_t button, uint32_t state_w);
    static const struct weston_pointer_grab_interface grab_interface;
};

#endif

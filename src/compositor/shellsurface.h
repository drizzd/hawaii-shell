/****************************************************************************
 * This file is part of Hawaii Shell.
 *
 * Copyright (C) 2012-2013 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 *
 * Author(s):
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

#ifndef SHELLSURFACE_H
#define SHELLSURFACE_H

#include <QtCompositor/private/qwlinputdevice_p.h>

#include "qwayland-server-hawaii.h"
#include "popupgrabber.h"
#include "shell.h"

class ShellSurface : public QtWaylandServer::wl_hawaii_shell_surface
{
public:
    ShellSurface(Compositor::ShellWindowRole role, QWaylandSurface *surface);
    ~ShellSurface();

    Compositor::ShellWindowRole role() const;

    QWaylandSurface *surface() const;
    void setSurface(QWaylandSurface *surface);

    PopupGrabber *popupGrabber() const;
    void setPopupGrabber(PopupGrabber *grabber);

private:
    Q_DISABLE_COPY(ShellSurface)

    Compositor::ShellWindowRole m_role;
    QWaylandSurface *m_surface;
    PopupGrabber *m_popupGrabber;

    void hawaii_shell_surface_dismiss(Resource *resource) Q_DECL_OVERRIDE;
};

#endif // SHELLSURFACE_H

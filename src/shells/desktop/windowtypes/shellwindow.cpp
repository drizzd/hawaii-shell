/****************************************************************************
 * This file is part of Hawaii Shell.
 *
 * Copyright (C) 2013 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
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

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#include <qpa/qplatformnativeinterface.h>

#include "shellwindow.h"
#include "desktopshell.h"
#include "desktopshell_p.h"
#include "shellui.h"

ShellWindow::ShellWindow(ElementType type)
    : QQuickWindow()
    , m_type(type)
    , m_typeAlreadySet(false)
    , m_posSent(false)
{
    // Transparent
    setColor(Qt::transparent);

    // Set custom window type
    setFlags(Qt::BypassWindowManagerHint);

    // Compositor
    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
    m_compositor = static_cast<wl_compositor *>(
                native->nativeResourceForIntegration("compositor"));

    // Keep QML posted on screen geometry changes
    connect(this, &ShellWindow::screenChanged, [=](QScreen *screen) {
        connect(screen, SIGNAL(screenGeometryChanged(QRect)),
                this, SIGNAL(screenGeometryChanged(QRect)));
    });

    // Create platform window
    create();
    setWindowType();

    // Change input region and position when geometry changes
    connect(this, &ShellWindow::xChanged, [=]() {
        setSurfacePosition(position());
    });
    connect(this, &ShellWindow::yChanged, [=]() {
        setSurfacePosition(position());
    });
}

ShellWindow::ElementType ShellWindow::elementType() const
{
    return m_type;
}

QRect ShellWindow::screenGeometry() const
{
    return screen()->geometry();
}

void ShellWindow::setWindowType()
{
    if (m_typeAlreadySet)
        return;

    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();

    wl_output *output = static_cast<wl_output *>(
                native->nativeResourceForScreen("output", screen()));
    wl_surface *surface = static_cast<wl_surface *>(
                native->nativeResourceForWindow("surface", this));

    DesktopShellImpl *shell = DesktopShell::instance()->d_ptr->shell;
    switch (elementType()) {
    case ShellWindow::Background:
        shell->set_background(output, surface);
        break;
    case ShellWindow::Panel:
        shell->set_panel(output, surface);
        break;
    case ShellWindow::Launcher:
        shell->set_launcher(output, surface);
        break;
    case ShellWindow::Overlay:
        shell->set_overlay(output, surface);
        break;
    case ShellWindow::Popup:
        shell->set_special(output, surface);
        break;
    default:
        break;
    }

    m_typeAlreadySet = true;
}

void ShellWindow::setSurfacePosition(const QPoint &pt)
{
    setWindowType();

    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();

    wl_surface *surface = static_cast<wl_surface *>(
                native->nativeResourceForWindow("surface", this));

    DesktopShellImpl *shell = DesktopShell::instance()->d_ptr->shell;
    shell->set_position(surface, pt.x(), pt.y());

    m_posSent = true;
}

void ShellWindow::setInputRegion(const QRect &r)
{
    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();

    wl_surface *surface = static_cast<wl_surface *>(
                native->nativeResourceForWindow("surface", this));

    wl_region *region = wl_compositor_create_region(m_compositor);
    wl_region_add(region, r.x(), r.y(), r.width(), r.height());
    wl_surface_set_input_region(surface, region);
    wl_region_destroy(region);
}

void ShellWindow::setOpaqueRegion(const QRect &r)
{
    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();

    wl_surface *surface = static_cast<wl_surface *>(
                native->nativeResourceForWindow("surface", this));

    wl_region *region = wl_compositor_create_region(m_compositor);
    wl_region_add(region, r.x(), r.y(), r.width(), r.height());
    wl_surface_set_opaque_region(surface, region);
    wl_region_destroy(region);
}

#include "moc_shellwindow.cpp"

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

#include <QtCore/QDebug>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtGui/QScreen>

#include <qpa/qplatformnativeinterface.h>

#include "backgroundwindow.h"
#include "hawaiishell.h"
#include "hawaiishell_p.h"
#include "shellscreen.h"

BackgroundWindow::BackgroundWindow(ShellScreen *screen)
    : QQuickView(HawaiiShell::instance()->engine(), new QWindow(screen->screen()))
    , m_surface(0)
{
    // Set custom window type
    setFlags(flags() | Qt::BypassWindowManagerHint);

    // Set Wayland window type
    create();
    setWindowType();

    // Load QML component
    setSource(QUrl("qrc:/qml/Background.qml"));

    // Resize view to actual size and thus resize the root object
    setResizeMode(QQuickView::SizeRootObjectToView);
    geometryChanged(screen->screen()->geometry());

    // React to screen size changes
    connect(screen->screen(), SIGNAL(geometryChanged(QRect)),
            this, SLOT(geometryChanged(QRect)));

    // Debugging message
    qDebug() << "-> Created Background with geometry"
             << geometry();
}

wl_surface *BackgroundWindow::surface() const
{
    return m_surface;
}

void BackgroundWindow::geometryChanged(const QRect &rect)
{
    // Resize view to actual size
    setGeometry(rect);

    // Set surface position
    setSurfacePosition();
}

void BackgroundWindow::setWindowType()
{
    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();

    wl_output *output = static_cast<struct wl_output *>(
                native->nativeResourceForScreen("output", screen()));
    m_surface = static_cast<struct wl_surface *>(
                native->nativeResourceForWindow("surface", this));

    HawaiiShell::instance()->d_ptr->shell->set_background(output, m_surface);
}

void BackgroundWindow::setSurfacePosition()
{
    HawaiiShellImpl *shell = HawaiiShell::instance()->d_ptr->shell;
    shell->set_position(m_surface, geometry().x(), geometry().y());
}

#include "moc_backgroundwindow.cpp"

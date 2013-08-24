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

#ifndef SHELLUI_H
#define SHELLUI_H

#include <QtCore/QObject>
#include <QtQml/QQmlComponent>

#include <wayland-client.h>

#include "grabwindow.h"
#include "lockscreenwindow.h"

class ShellWindow;

class ShellUi : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRect availableGeometry READ availableGeometry NOTIFY availableGeometryChanged)
public:
    explicit ShellUi(QQmlEngine *engine, QScreen *screen, QObject *parent = 0);
    ~ShellUi();

    QQmlEngine *engine() const;

    QScreen *screen() const;
    wl_output *output() const;

    QRect availableGeometry() const;

    GrabWindow *grabWindow() const;

    LockScreenWindow *lockScreenWindow() const;

    void show();
    void hide();

Q_SIGNALS:
    void availableGeometryChanged(const QRect &rect);

public Q_SLOTS:
    void updateScreenGeometry(const QRect &rect);

    void createLockScreenWindow();
    void closeLockScreenWindow();

    void setNumWorkspaces(int num);

private:
    QQmlEngine *m_engine;
    QScreen *m_screen;
    wl_output *m_output;
    QRect m_availableGeometry;
    GrabWindow *m_grabWindow;
    LockScreenWindow *m_lockScreenWindow;
    QQmlComponent *m_component;
    QObject *m_rootObject;
    QList<ShellWindow *> m_windows;
    int m_numWorkspaces;
};

#endif // SHELLUI_H

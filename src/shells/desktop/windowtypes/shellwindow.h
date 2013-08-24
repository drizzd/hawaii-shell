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

#ifndef SHELLWINDOW_H
#define SHELLWINDOW_H

#include <QtQuick/QQuickWindow>

struct wl_compositor;

class ShellWindow : public QQuickWindow
{
    Q_OBJECT
    Q_PROPERTY(QRect screenGeometry READ screenGeometry NOTIFY screenGeometryChanged)
    Q_ENUMS(ElementType)
public:
    enum ElementType {
        Background = 0,
        Panel,
        Launcher,
        Overlay,
        Popup,
        Window
    };

    ShellWindow(ElementType type);

    ElementType elementType() const;

    QRect screenGeometry() const;

    bool isSurfaceVisible() const;
    void setSurfaceVisible(bool value);

Q_SIGNALS:
    void screenGeometryChanged(const QRect &geometry);
    void surfaceVisibleChanged(bool value);

private:
    ElementType m_type;
    wl_compositor *m_compositor;
    bool m_typeAlreadySet;
    bool m_posSent;

private Q_SLOTS:
    void setWindowType();
    void setSurfacePosition(const QPoint &pt);
    void setInputRegion(const QRect &r);
    void setOpaqueRegion(const QRect &r);
};

#endif // SHELLWINDOW_H

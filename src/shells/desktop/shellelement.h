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

#ifndef SHELLELEMENT_H
#define SHELLELEMENT_H

#include <QtQuick/QQuickItem>

class ShellElement : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(Type type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QRect screenGeometry READ screenGeometry NOTIFY screenGeometryChanged)
    Q_PROPERTY(qreal elementX READ elementX WRITE setElementX NOTIFY elementXChanged)
    Q_PROPERTY(qreal elementY READ elementY WRITE setElementY NOTIFY elementYChanged)
    Q_ENUMS(Type)
public:
    enum Type {
        Unknown = 0,
        Background,
        Panel,
        Launcher,
        Overlay,
        Popup,
        Window
    };

    ShellElement(QQuickItem *parent = 0);

    Type type() const;
    void setType(Type type);

    QScreen *screen() const;
    void setScreen(QScreen *screen);

    QRect screenGeometry() const;

    QPointF elementPosition() const;

    qreal elementX() const;
    void setElementX(qreal x);

    qreal elementY() const;
    void setElementY(qreal y);

Q_SIGNALS:
    void typeChanged(ShellElement::Type type);
    void screenGeometryChanged(const QRect &geometry);
    void elementXChanged(qreal x);
    void elementYChanged(qreal y);

public Q_SLOTS:
    void show();
    void hide();

private:
    Type m_type;
    QScreen *m_screen;
    QPointF m_pos;
};

#endif // SHELLELEMENT_H

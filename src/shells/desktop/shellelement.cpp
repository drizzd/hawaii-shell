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

#include <QDebug>
#include <QtGui/QScreen>
#include <QtQuick/QQuickWindow>

#include "shellelement.h"

ShellElement::ShellElement(QQuickItem *parent)
    : QQuickItem(parent)
    , m_type(ShellElement::Unknown)
    , m_screen(0)
{
}

ShellElement::Type ShellElement::type() const
{
    return m_type;
}

void ShellElement::setType(Type type)
{
    if (m_type != type) {
        m_type = type;
        Q_EMIT typeChanged(m_type);
    }
}

QScreen *ShellElement::screen() const
{
    return m_screen;
}

void ShellElement::setScreen(QScreen *screen)
{
    if (!m_screen || m_screen->geometry() != screen->geometry()) {
        m_screen = screen;
        connect(m_screen, SIGNAL(geometryChanged(QRect)),
                this, SIGNAL(screenGeometryChanged(QRect)));
        Q_EMIT screenGeometryChanged(m_screen->geometry());
    }
}

QRect ShellElement::screenGeometry() const
{
    if (m_screen)
        return m_screen->geometry();
    return QRect(0, 0, 1, 1);
}

QPointF ShellElement::elementPosition() const
{
    return m_pos;
}

qreal ShellElement::elementX() const
{
    return m_pos.x();
}

void ShellElement::setElementX(qreal x)
{
    if (m_pos.x() != x) {
        m_pos.setX(x);
        Q_EMIT elementXChanged(x);
    }
}

qreal ShellElement::elementY() const
{
    return m_pos.y();
}

void ShellElement::setElementY(qreal y)
{
    if (m_pos.y() != y) {
        m_pos.setY(y);
        Q_EMIT elementYChanged(y);
    }
}

void ShellElement::show()
{
}

void ShellElement::hide()
{
}

#include "moc_shellelement.cpp"

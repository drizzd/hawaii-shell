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

#ifndef SHORTCUT_H
#define SHORTCUT_H

#include <QtCore/QObject>

class ShortcutPrivate;

class Shortcut : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant key READ key WRITE setKey NOTIFY keyChanged)
public:
    Shortcut(QObject *parent = 0);
    ~Shortcut();

    QVariant key() const;
    void setKey(const QVariant &value);

Q_SIGNALS:
    void keyChanged();
    void triggered();

private:
    Q_DECLARE_PRIVATE(Shortcut)
    class ShortcutPrivate *const d_ptr;
};

#endif // SHORTCUT_H

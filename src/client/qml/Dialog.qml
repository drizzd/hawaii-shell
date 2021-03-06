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

import QtQuick 2.1
import QtQuick.Window 2.1
import QtQuick.Layouts 1.0
import Hawaii.Shell 1.0
import Hawaii.Shell.Styles 1.0

DialogWindow {
    id: dialogWindow

    default property alias content: mainLayout.data

    StyledItem {
        readonly property int margin: 11

        id: styledItem
        style: Qt.createComponent(StyleSettings.path + "/DialogStyle.qml", styledItem)
        width: mainLayout.implicitWidth + __style.padding.left + __style.padding.right + (2 * margin)
        height: mainLayout.implicitHeight + __style.padding.top + __style.padding.bottom + (2 * margin)

        ColumnLayout {
            id: mainLayout
            anchors {
                fill: parent
                leftMargin: styledItem.__style.padding.left + styledItem.margin
                topMargin: styledItem.__style.padding.top + styledItem.margin
                rightMargin: styledItem.__style.padding.right + styledItem.margin
                bottomMargin: styledItem.__style.padding.bottom + styledItem.margin
            }
        }
    }
}

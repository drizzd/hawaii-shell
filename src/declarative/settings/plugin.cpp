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

#include <QtQml/QQmlExtensionPlugin>
#include <QtQml/QQmlComponent>

#include <libhawaiicore/settings/backgroundsettings.h>
#include <libhawaiicore/settings/launchersettings.h>
#include <libhawaiicore/settings/shellsettings.h>

class HawaiiShellSettingsPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface/1.0")
public:
    void registerTypes(const char *uri);
};

void HawaiiShellSettingsPlugin::registerTypes(const char *uri)
{
    // @uri Hawaii.Shell.Settings
    qmlRegisterType<BackgroundSettings>(uri, 1, 0, "BackgroundSettings");
    qmlRegisterType<LauncherSettings>(uri, 1, 0, "LauncherSettings");
    qmlRegisterType<ShellSettings>(uri, 1, 0, "ShellSettings");
}

#include "plugin.moc"

/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "version.h"

#include "commandline.h"

#include <core/application.h>
#include <gui/guiapplication.h>

#include <kdsingleapplication.h>

#include <QApplication>
#include <QLoggingCategory>

int main(int argc, char** argv)
{
    Q_INIT_RESOURCE(data);
    Q_INIT_RESOURCE(icons);

    QCoreApplication::setApplicationName(QStringLiteral("fooyin"));
    QCoreApplication::setApplicationVersion(QStringLiteral(VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.fooyin.fooyin"));
    QGuiApplication::setQuitOnLastWindowClosed(false);

    CommandLine commandLine{argc, argv};

    auto checkInstance = [&commandLine](KDSingleApplication& instance) {
        if(instance.isPrimaryInstance()) {
            return true;
        }

        if(commandLine.empty()) {
            QLoggingCategory log{"Main"};
            qCInfo(log) << "fooyin already running";
            instance.sendMessage({});
        }
        else {
            instance.sendMessage(commandLine.saveOptions());
        }

        return commandLine.skipSingleApp();
    };

    {
        const QCoreApplication app{argc, argv};
        KDSingleApplication instance{QCoreApplication::applicationName(),
                                     KDSingleApplication::Option::IncludeUsernameInSocketName};
        if(!commandLine.parse()) {
            return 1;
        }
        if(!checkInstance(instance)) {
            return 0;
        }
    }

    const QApplication app{argc, argv};
    KDSingleApplication instance{QCoreApplication::applicationName(),
                                 KDSingleApplication::Option::IncludeUsernameInSocketName};
    if(!checkInstance(instance)) {
        return 0;
    }

    // Startup
    Fooyin::Application coreApp;
    Fooyin::GuiApplication guiApp{&coreApp};

    if(!commandLine.empty()) {
        guiApp.openFiles(commandLine.files());
    }

    QObject::connect(&instance, &KDSingleApplication::messageReceived, &guiApp, [&guiApp](const QByteArray& options) {
        CommandLine command;
        command.loadOptions(options);
        if(!command.empty()) {
            guiApp.openFiles(command.files());
        }
        else {
            guiApp.raise();
        }
    });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &coreApp, [&coreApp, &guiApp]() {
        guiApp.shutdown();
        coreApp.shutdown();
    });

    return QCoreApplication::exec();
}

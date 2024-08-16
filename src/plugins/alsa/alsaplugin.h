/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#pragma once

#include <core/engine/outputplugin.h>
#include <core/plugins/plugin.h>

namespace Fooyin::Alsa {
class AlsaPlugin : public QObject,
                   public Plugin,
                   public OutputPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "alsa.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::OutputPlugin)

public:
    [[nodiscard]] QString name() const override;
    [[nodiscard]] OutputCreator creator() const override;

    [[nodiscard]] bool hasSettings() const override;
    void showSettings(QWidget* parent) override;
};
} // namespace Fooyin::Alsa

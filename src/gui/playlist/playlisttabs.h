/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <gui/fywidget.h>

namespace Fy {

namespace Utils {
class ActionManager;
class SettingsManager;
} // namespace Utils

namespace Core::Playlist {
class Playlist;
} // namespace Core::Playlist

namespace Gui::Widgets {
class WidgetFactory;

namespace Playlist {
class PlaylistController;

class PlaylistTabs : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistTabs(Utils::ActionManager* actionManager, Widgets::WidgetFactory* widgetFactory,
                          PlaylistController* controller, Utils::SettingsManager* settings, QWidget* parent = nullptr);
    ~PlaylistTabs() override;

    void setupTabs();

    int addPlaylist(const Core::Playlist::Playlist& playlist, bool switchTo);
    void removePlaylist(const Core::Playlist::Playlist& playlist);
    int addNewTab(const QString& name, const QIcon& icon = {});

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayout(QJsonArray& array) override;
    void loadLayout(const QJsonObject& object) override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Playlist
} // namespace Gui::Widgets
} // namespace Fy

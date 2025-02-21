/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "layoutmenu.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/layoutprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QMenu>

namespace Fooyin {
LayoutMenu::LayoutMenu(ActionManager* actionManager, LayoutProvider* layoutProvider, SettingsManager* settings,
                       QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_layoutProvider{layoutProvider}
    , m_settings{settings}
    , m_layoutMenu{nullptr}
    , m_layoutEditing{nullptr}
{ }

void LayoutMenu::setup()
{
    m_layoutMenu = m_actionManager->actionContainer(Constants::Menus::Layout);
    m_layoutMenu->clear();

    if(!m_layoutEditing) {
        m_layoutEditing = new QAction(tr("&Editing mode"), this);
        m_layoutEditing->setStatusTip(tr("Toggle layout editing mode"));
        m_layoutEditingCmd = m_actionManager->registerAction(m_layoutEditing, Constants::Actions::LayoutEditing);
        QObject::connect(m_layoutEditing, &QAction::triggered, this,
                         [this](bool checked) { m_settings->set<Settings::Gui::LayoutEditing>(checked); });
        m_settings->subscribe<Settings::Gui::LayoutEditing>(m_layoutEditing, &QAction::setChecked);
        m_layoutEditing->setCheckable(true);
        m_layoutEditing->setChecked(m_settings->value<Settings::Gui::LayoutEditing>());
        m_settings->subscribe<Settings::Gui::LayoutEditing>(
            this, [this](bool enabled) { m_layoutEditing->setChecked(enabled); });
    }

    m_layoutMenu->addAction(m_layoutEditingCmd, Actions::Groups::One);

    auto* importLayout = new QAction(tr("&Import layout…"), m_layoutMenu->menu());
    importLayout->setStatusTip(tr("Add the layout from the specified file"));
    QObject::connect(importLayout, &QAction::triggered, this, &LayoutMenu::importLayout);
    auto* exportLayout = new QAction(tr("E&xport layout…"), m_layoutMenu->menu());
    exportLayout->setStatusTip(tr("Save the current layout to the specified file"));
    QObject::connect(exportLayout, &QAction::triggered, this, &LayoutMenu::exportLayout);

    m_layoutMenu->addAction(importLayout);
    m_layoutMenu->addAction(exportLayout);

    m_layoutMenu->addSeparator();

    const auto layouts = m_layoutProvider->layouts();
    for(const auto& layout : layouts) {
        addLayout(layout.name());
    }

    QObject::connect(m_layoutProvider, &LayoutProvider::layoutAdded, this,
                     [this](const FyLayout& layout) { addLayout(layout.name()); });
}

void LayoutMenu::addLayout(const QString& name)
{
    if(!m_layoutMenu) {
        return;
    }

    auto* layoutAction = new QAction(name, m_layoutMenu->menu());
    layoutAction->setStatusTip(tr("Replace the current layout"));
    auto* layoutCmd = m_actionManager->registerAction(layoutAction, Id{QStringLiteral("Layout.Switch.%1").arg(name)});
    QObject::connect(layoutAction, &QAction::triggered, this, [this, name]() {
        const auto layout = m_layoutProvider->layoutByName(name);
        if(layout.isValid()) {
            emit changeLayout(layout);
        }
    });
    m_layoutMenu->addAction(layoutCmd->action());
}
} // namespace Fooyin

#include "moc_layoutmenu.cpp"

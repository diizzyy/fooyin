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

#include "filtercolumnregistry.h"

namespace Fooyin::Filters {
FilterColumnRegistry::FilterColumnRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{QStringLiteral("Filters/FilterColumns"), settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        const auto field = itemById(id);
        emit columnChanged(field);
    });

    loadItems();
}

void FilterColumnRegistry::loadDefaults()
{
    addDefaultItem({.name = tr("Genre"), .field = QStringLiteral("%<genre>%")});
    addDefaultItem({.name = tr("Album Artist"), .field = QStringLiteral("%<albumartist>%")});
    addDefaultItem({.name = tr("Artist"), .field = QStringLiteral("%<artist>%")});
    addDefaultItem({.name = tr("Album"), .field = QStringLiteral("%album%")});
    addDefaultItem({.name = tr("Date"), .field = QStringLiteral("%date%")});
}
} // namespace Fooyin::Filters

#include "moc_filtercolumnregistry.cpp"

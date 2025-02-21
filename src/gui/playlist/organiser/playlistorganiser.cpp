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

#include "playlistorganiser.h"

#include "playlist/playlistcontroller.h"
#include "playlist/playlistinteractor.h"
#include "playlistorganiserdelegate.h"
#include "playlistorganisermodel.h"

#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QContextMenuEvent>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

#include <stack>

constexpr auto OrganiserModel = "PlaylistOrganiser/Model";
constexpr auto OrganiserState = "PlaylistOrganiser/State";

namespace {
QByteArray saveExpandedState(QTreeView* view, QAbstractItemModel* model)
{
    QByteArray data;
    QDataStream stream{&data, QIODeviceBase::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    std::stack<QModelIndex> indexesToSave;
    indexesToSave.emplace();

    while(!indexesToSave.empty()) {
        const QModelIndex index = indexesToSave.top();
        indexesToSave.pop();

        if(model->hasChildren(index)) {
            if(index.isValid()) {
                stream << view->isExpanded(index);
            }
            const int childCount = model->rowCount(index);
            for(int row{childCount - 1}; row >= 0; --row) {
                indexesToSave.push(model->index(row, 0, index));
            }
        }
    }

    data = qCompress(data, 9);

    return data;
}

void restoreExpandedState(QTreeView* view, QAbstractItemModel* model, QByteArray data)
{
    if(data.isEmpty()) {
        return;
    }

    data = qUncompress(data);

    QDataStream stream{&data, QIODeviceBase::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    std::stack<QModelIndex> indexesToSave;
    indexesToSave.emplace();

    while(!indexesToSave.empty()) {
        const QModelIndex index = indexesToSave.top();
        indexesToSave.pop();

        if(model->hasChildren(index)) {
            if(index.isValid()) {
                bool expanded;
                stream >> expanded;
                view->setExpanded(index, expanded);
            }
            const int childCount = model->rowCount(index);
            for(int row{childCount - 1}; row >= 0; --row) {
                indexesToSave.push(model->index(row, 0, index));
            }
        }
    }
}
} // namespace

namespace Fooyin {
PlaylistOrganiser::PlaylistOrganiser(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                                     SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
    , m_playlistInteractor{playlistInteractor}
    , m_playerController{playlistInteractor->playlistController()->playerController()}
    , m_organiserTree{new QTreeView(this)}
    , m_model{new PlaylistOrganiserModel(playlistInteractor->handler(), m_playerController)}
    , m_context{new WidgetContext(this, Context{Id{"Context.PlaylistOrganiser."}.append(Utils::generateUniqueHash())},
                                  this)}
    , m_removePlaylist{new QAction(tr("Remove"), this)}
    , m_removeCmd{actionManager->registerAction(m_removePlaylist, Constants::Actions::Remove, m_context->context())}
    , m_renamePlaylist{new QAction(tr("Rename"), this)}
    , m_renameCmd{actionManager->registerAction(m_renamePlaylist, Constants::Actions::Rename, m_context->context())}
    , m_newGroup{new QAction(tr("New group"), this)}
    , m_newGroupCmd{actionManager->registerAction(m_newGroup, "PlaylistOrganiser.NewGroup", m_context->context())}
    , m_newPlaylist{new QAction(tr("Create playlist"), this)}
    , m_newPlaylistCmd{
          actionManager->registerAction(m_newPlaylist, "PlaylistOrganiser.NewPlaylist", m_context->context())}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_organiserTree);

    m_organiserTree->setHeaderHidden(true);
    m_organiserTree->setUniformRowHeights(true);
    m_organiserTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_organiserTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_organiserTree->setDragEnabled(true);
    m_organiserTree->setAcceptDrops(true);
    m_organiserTree->setDragDropMode(QAbstractItemView::DragDrop);
    m_organiserTree->setDefaultDropAction(Qt::MoveAction);
    m_organiserTree->setDropIndicatorShown(true);
    m_organiserTree->setAllColumnsShowFocus(true);

    m_organiserTree->setModel(m_model);
    m_organiserTree->setItemDelegate(new PlaylistOrganiserDelegate(this));

    actionManager->addContextObject(m_context);

    m_removePlaylist->setStatusTip(tr("Remove the selected playlist(s)"));
    m_removeCmd->setAttribute(ProxyAction::UpdateText);
    m_removeCmd->setDefaultShortcut(QKeySequence::Delete);

    m_renamePlaylist->setStatusTip(tr("Rename the selected playlist"));
    m_renameCmd->setAttribute(ProxyAction::UpdateText);
    m_renameCmd->setDefaultShortcut(Qt::Key_F2);

    m_newGroup->setStatusTip(tr("Create a new empty group"));
    m_newGroupCmd->setAttribute(ProxyAction::UpdateText);
    m_newGroupCmd->setDefaultShortcut(QKeySequence::AddTab);

    m_newPlaylist->setStatusTip(tr("Create a new empty playlist"));
    m_newPlaylistCmd->setAttribute(ProxyAction::UpdateText);
    m_newPlaylistCmd->setDefaultShortcut(QKeySequence::New);

    QAction::connect(m_newGroup, &QAction::triggered, this, [this]() {
        const auto indexes = m_organiserTree->selectionModel()->selectedIndexes();
        createGroup(indexes.empty() ? QModelIndex{} : indexes.front());
    });
    QObject::connect(m_removePlaylist, &QAction::triggered, this,
                     [this]() { m_model->removeItems(m_organiserTree->selectionModel()->selectedIndexes()); });
    QObject::connect(m_renamePlaylist, &QAction::triggered, this, [this]() {
        const auto indexes = m_organiserTree->selectionModel()->selectedIndexes();
        m_organiserTree->edit(indexes.empty() ? QModelIndex{} : indexes.front());
    });
    QObject::connect(m_newPlaylist, &QAction::triggered, this, [this]() {
        const auto indexes = m_organiserTree->selectionModel()->selectedIndexes();
        createPlaylist(indexes.empty() ? QModelIndex{} : indexes.front());
    });

    QObject::connect(m_model, &QAbstractItemModel::rowsMoved, this,
                     [this](const QModelIndex& /*source*/, int /*first*/, int /*last*/, const QModelIndex& target) {
                         if(target.isValid()) {
                             m_organiserTree->expand(target);
                         }
                     });
    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex& index) {
        if(index.isValid()) {
            m_organiserTree->expand(index);
        }
    });
    QObject::connect(m_model, &PlaylistOrganiserModel::filesDroppedOnPlaylist, this,
                     &PlaylistOrganiser::filesToPlaylist);
    QObject::connect(m_model, &PlaylistOrganiserModel::filesDroppedOnGroup, this, &PlaylistOrganiser::filesToGroup);
    QObject::connect(m_model, &PlaylistOrganiserModel::tracksDroppedOnPlaylist, this,
                     &PlaylistOrganiser::tracksToPlaylist);
    QObject::connect(m_model, &PlaylistOrganiserModel::tracksDroppedOnGroup, this, &PlaylistOrganiser::tracksToGroup);
    QObject::connect(m_organiserTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { selectionChanged(); });

    QObject::connect(
        m_playlistInteractor->handler(), &PlaylistHandler::playlistAdded, this, [this](Playlist* playlist) {
            if(!m_creatingPlaylist) {
                QMetaObject::invokeMethod(
                    m_model, [this, playlist]() { m_model->playlistAdded(playlist); }, Qt::QueuedConnection);
            }
        });
    QObject::connect(m_playlistInteractor->handler(), &PlaylistHandler::playlistRemoved, m_model,
                     &PlaylistOrganiserModel::playlistRemoved);
    QObject::connect(m_playlistInteractor->handler(), &PlaylistHandler::playlistRenamed, m_model,
                     &PlaylistOrganiserModel::playlistRenamed);
    QObject::connect(
        m_playlistInteractor->playlistController(), &PlaylistController::currentPlaylistChanged, this,
        [this]() { QMetaObject::invokeMethod(m_model, [this]() { selectCurrentPlaylist(); }, Qt::QueuedConnection); });
    QObject::connect(m_playlistInteractor->playlistController(), &PlaylistController::playlistsLoaded, this,
                     [this]() { selectCurrentPlaylist(); });

    if(m_model->restoreModel(m_settings->fileValue(OrganiserModel).toByteArray())) {
        const auto state = m_settings->fileValue(OrganiserState).toByteArray();
        restoreExpandedState(m_organiserTree, m_model, state);
        m_model->populateMissing();
    }
    else {
        m_model->populate();
    }

    selectCurrentPlaylist();
}

PlaylistOrganiser::~PlaylistOrganiser()
{
    m_settings->fileSet(OrganiserModel, m_model->saveModel());
    m_settings->fileSet(OrganiserState, saveExpandedState(m_organiserTree, m_model));
}

QString PlaylistOrganiser::name() const
{
    return tr("Playlist Organiser");
}

QString PlaylistOrganiser::layoutName() const
{
    return QStringLiteral("PlaylistOrganiser");
}

void PlaylistOrganiser::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const QPoint point      = m_organiserTree->viewport()->mapFrom(this, event->pos());
    const QModelIndex index = m_organiserTree->indexAt(point);

    m_removePlaylist->setEnabled(index.isValid());
    m_renamePlaylist->setEnabled(index.isValid());

    menu->addAction(m_newPlaylistCmd->action());
    menu->addAction(m_newGroupCmd->action());

    if(index.data(PlaylistOrganiserItem::ItemType).toInt() == PlaylistOrganiserItem::PlaylistItem) {
        if(auto* savePlaylist = m_actionManager->command(Constants::Actions::SavePlaylist)) {
            menu->addSeparator();
            menu->addAction(savePlaylist->action());
        }
    }

    menu->addSeparator();
    menu->addAction(m_renameCmd->action());
    menu->addAction(m_removeCmd->action());

    menu->popup(event->globalPos());
}

void PlaylistOrganiser::selectionChanged()
{
    const QModelIndexList selectedIndexes = m_organiserTree->selectionModel()->selectedIndexes();
    if(selectedIndexes.empty()) {
        return;
    }

    const QModelIndex firstIndex = selectedIndexes.front();
    if(firstIndex.data(PlaylistOrganiserItem::ItemType).toInt() != PlaylistOrganiserItem::PlaylistItem) {
        return;
    }

    auto* playlist       = firstIndex.data(PlaylistOrganiserItem::PlaylistData).value<Playlist*>();
    const UId playlistId = playlist->id();

    if(std::exchange(m_currentPlaylistId, playlistId) != playlistId) {
        m_playlistInteractor->playlistController()->changeCurrentPlaylist(playlist);
    }
}

void PlaylistOrganiser::selectCurrentPlaylist()
{
    auto* playlist = m_playlistInteractor->playlistController()->currentPlaylist();
    if(!playlist) {
        return;
    }

    const UId playlistId = playlist->id();
    if(std::exchange(m_currentPlaylistId, playlistId) != playlistId) {
        const QModelIndex index = m_model->indexForPlaylist(playlist);
        if(index.isValid()) {
            m_organiserTree->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
        }
    }
}

void PlaylistOrganiser::createGroup(const QModelIndex& index) const
{
    QModelIndex parent{index};
    if(parent.data(PlaylistOrganiserItem::ItemType).toInt() == PlaylistOrganiserItem::PlaylistItem) {
        parent = parent.parent();
    }
    const QModelIndex groupIndex = m_model->createGroup(parent);
    m_organiserTree->edit(groupIndex);
}

void PlaylistOrganiser::createPlaylist(const QModelIndex& index)
{
    m_creatingPlaylist = true;

    if(auto* playlist = m_playlistInteractor->handler()->createEmptyPlaylist()) {
        QModelIndex parent{index};
        if(parent.data(PlaylistOrganiserItem::ItemType).toInt() == PlaylistOrganiserItem::PlaylistItem) {
            parent = parent.parent();
        }
        const QModelIndex playlistIndex = m_model->createPlaylist(playlist, parent);
        m_organiserTree->edit(playlistIndex);
    }

    m_creatingPlaylist = false;
}

void PlaylistOrganiser::filesToPlaylist(const QList<QUrl>& urls, const UId& id)
{
    if(urls.empty()) {
        return;
    }

    m_playlistInteractor->filesToPlaylist(urls, id);
}

void PlaylistOrganiser::filesToGroup(const QList<QUrl>& urls, const QString& group, int index)
{
    if(urls.empty()) {
        return;
    }

    m_playlistInteractor->filesToTracks(urls, [this, group, index](const TrackList& tracks) {
        const QSignalBlocker block{m_playlistInteractor->handler()};
        const QString name = Track::findCommonField(tracks);
        if(auto* playlist = m_playlistInteractor->handler()->createNewPlaylist(name, tracks)) {
            m_model->playlistInserted(playlist, group, index);
        }
    });
}

void PlaylistOrganiser::tracksToPlaylist(const std::vector<int>& trackIds, const UId& id)
{
    const auto tracks = m_playlistInteractor->library()->tracksForIds(trackIds);
    if(tracks.empty()) {
        return;
    }

    if(m_playlistInteractor->handler()->playlistById(id)) {
        m_playlistInteractor->handler()->appendToPlaylist(id, tracks);
    }
}

void PlaylistOrganiser::tracksToGroup(const std::vector<int>& trackIds, const QString& group, int index)
{
    const auto tracks = m_playlistInteractor->library()->tracksForIds(trackIds);
    if(tracks.empty()) {
        return;
    }

    const QSignalBlocker block{m_playlistInteractor->handler()};

    const QString name = Track::findCommonField(tracks);
    if(auto* playlist = m_playlistInteractor->handler()->createNewPlaylist(name, tracks)) {
        m_model->playlistInserted(playlist, group, index);
    }
}
} // namespace Fooyin

#include "moc_playlistorganiser.cpp"

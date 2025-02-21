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

#include "m3uparser.h"

#include <core/track.h>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTextStream>

Q_LOGGING_CATEGORY(M3U, "fy.m3u")

namespace {
enum class Type
{
    Standard,
    Extended,
    Dir // TODO
};

struct Metadata
{
    QString title;
    QString artist;
    uint64_t duration{0};
};

bool processMetadata(const QString& line, Metadata& metadata)
{
    const QString info = line.section(u':', 1);
    const QString dur  = info.section(u',', 0, 0);

    bool isNum{false};
    const int duration = dur.toInt(&isNum);
    if(!isNum) {
        return false;
    }

    metadata.duration = static_cast<uint64_t>(duration) * 1000;

    const QString trackSection = info.section(u',', 1);
    const QStringList list     = trackSection.split(QStringLiteral(" - "));

    if(list.size() <= 1) {
        metadata.title = trackSection;
        return true;
    }

    metadata.artist = list[0].trimmed();
    metadata.title  = list[1].trimmed();

    return true;
}

int endingSubsong(QString* filepath)
{
    static const QRegularExpression regex{QStringLiteral(R"(#(\d+)$)")};
    const QRegularExpressionMatch match = regex.match(*filepath);
    if(match.hasMatch()) {
        const int subsong = match.captured(1).toInt();
        filepath->remove(match.capturedStart(), match.capturedLength());
        return subsong;
    }
    return -1;
}
} // namespace

namespace Fooyin {
QString M3uParser::name() const
{
    return QStringLiteral("M3U");
}

QStringList M3uParser::supportedExtensions() const
{
    static const QStringList extensions{QStringLiteral("m3u"), QStringLiteral("m3u8")};
    return extensions;
}

bool M3uParser::saveIsSupported() const
{
    return true;
}

TrackList M3uParser::readPlaylist(QIODevice* device, const QString& /*filepath*/, const QDir& dir, bool skipNotFound)
{
    Type type{Type::Standard};
    Metadata metadata;

    QByteArray m3u = toUtf8(device);
    QBuffer buffer{&m3u};
    if(!buffer.open(QIODevice::ReadOnly)) {
        return {};
    }

    TrackList tracks;

    while(!buffer.atEnd()) {
        const QString line = QString::fromUtf8(buffer.readLine()).trimmed();

        if(line.startsWith(u"#EXTM3U")) {
            type = Type::Extended;
            continue;
        }

        if(line.startsWith(u'#')) {
            if(type == Type::Extended && line.startsWith(u"#EXT")) {
                if(!processMetadata(line, metadata)) {
                    qCWarning(M3U) << "Failed to process metadata:" << line;
                }
            }
        }
        else if(!line.isEmpty()) {
            QString path;

            if(dir.exists()) {
                if(QDir::isAbsolutePath(line) || line.left(9) == u"unpack://") {
                    path = line;
                }
                else {
                    path = QDir::cleanPath(dir.absoluteFilePath(line));
                }
            }

            const int subsong = endingSubsong(&path);
            Track track{path};

            if(subsong > 0) {
                track.setSubsong(subsong);
            }

            Track readTrack = PlaylistParser::readMetadata(track);
            if(readTrack.isValid() || !skipNotFound) {
                if(readTrack.title().isEmpty() && !metadata.title.isEmpty()) {
                    readTrack.setTitle(metadata.title);
                }
                if(readTrack.artists().empty() && !metadata.artist.isEmpty()) {
                    readTrack.setArtists({metadata.artist});
                }
                tracks.push_back(readTrack);
            }
        }
    }

    buffer.close();

    return tracks;
}

void M3uParser::savePlaylist(QIODevice* device, const QString& extension, const TrackList& tracks, const QDir& dir,
                             PathType type, bool writeMetdata)
{
    QTextStream stream{device};
    if(extension == u"m3u") {
        stream.setEncoding(QStringConverter::System);
    }

    if(writeMetdata) {
        stream << "#EXTM3U\n";
    }

    for(const Track& track : tracks) {
        if(writeMetdata) {
            stream << QStringLiteral("#EXTINF:%1,%2 - %3\n")
                          .arg(track.duration() / 1000)
                          .arg(track.artist(), track.title());
        }
        QString path = track.filepath();
        if(track.subsong() > 0) {
            path += QStringLiteral("#%1").arg(track.subsong());
        }
        stream << PlaylistParser::determineTrackPath(QUrl::fromLocalFile(path), dir, type) << "\n";
    }
}
} // namespace Fooyin

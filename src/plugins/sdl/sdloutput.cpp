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

#include "sdloutput.h"

#include <SDL2/SDL.h>

#include <QDebug>

namespace {
SDL_AudioFormat findFormat(Fooyin::SampleFormat format)
{
    switch(format) {
        case(Fooyin::SampleFormat::U8):
            return AUDIO_U8;
        case(Fooyin::SampleFormat::S16):
            return AUDIO_S16SYS;
        case(Fooyin::SampleFormat::S24):
        case(Fooyin::SampleFormat::S32):
            return AUDIO_S32SYS;
        case(Fooyin::SampleFormat::Float):
            return AUDIO_F32SYS;
        case(Fooyin::SampleFormat::Unknown):
        default:
            return AUDIO_S16;
    }
}
} // namespace

namespace Fooyin::Sdl {
SdlOutput::SdlOutput()
    : m_bufferSize{4096}
    , m_initialised{false}
    , m_device{QStringLiteral("default")}
{
#ifdef Q_OS_WIN32
    SDL_setenv("SDL_AUDIODRIVER", "directsound", true); // WASAPI driver (default) is broken
#endif
}

bool SdlOutput::init(const AudioFormat& format)
{
    m_format = format;

    SDL_Init(SDL_INIT_AUDIO);

    m_desiredSpec.freq     = format.sampleRate();
    m_desiredSpec.format   = findFormat(format.sampleFormat());
    m_desiredSpec.channels = format.channelCount();
    m_desiredSpec.samples  = m_bufferSize;
    m_desiredSpec.callback = nullptr;

    if(m_device == QStringLiteral("default")) {
        m_audioDeviceId = SDL_OpenAudioDevice(nullptr, 0, &m_desiredSpec, &m_obtainedSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    }
    else {
        m_audioDeviceId = SDL_OpenAudioDevice(m_device.toLocal8Bit().constData(), 0, &m_desiredSpec, &m_obtainedSpec,
                                              SDL_AUDIO_ALLOW_ANY_CHANGE);
    }

    if(m_audioDeviceId == 0) {
        qDebug() << "[SDL] Error opening audio device: " << SDL_GetError();
        return false;
    }

    m_initialised = true;
    return true;
}

void SdlOutput::uninit()
{
    SDL_CloseAudioDevice(m_audioDeviceId);
    SDL_Quit();

    m_initialised = false;
}

void SdlOutput::reset()
{
    SDL_PauseAudioDevice(m_audioDeviceId, 1);
    SDL_ClearQueuedAudio(m_audioDeviceId);
}

void SdlOutput::start()
{
    if(SDL_GetAudioStatus() != SDL_AUDIO_PLAYING) {
        SDL_PauseAudioDevice(m_audioDeviceId, 0);
    }
}

bool SdlOutput::initialised() const
{
    return m_initialised;
}

QString SdlOutput::device() const
{
    return m_device;
}

bool SdlOutput::canHandleVolume() const
{
    return false;
}

int SdlOutput::bufferSize() const
{
    return m_bufferSize;
}

OutputState SdlOutput::currentState()
{
    OutputState state;

    state.queuedSamples = static_cast<int>(SDL_GetQueuedAudioSize(m_audioDeviceId) / m_format.bytesPerFrame());
    state.freeSamples   = m_bufferSize - state.queuedSamples;

    return state;
}

OutputDevices SdlOutput::getAllDevices() const
{
    OutputDevices devices;
    bool tempInit{false};

    if(!SDL_WasInit(SDL_INIT_AUDIO)) {
        tempInit = true;
        SDL_Init(SDL_INIT_AUDIO);
    }

    devices.emplace_back(QStringLiteral("default"), QStringLiteral("Default"));

    const int num = SDL_GetNumAudioDevices(0);
    for(int i = 0; i < num; ++i) {
        const QString devName = QString::fromLatin1(SDL_GetAudioDeviceName(i, 0));
        if(!devName.isNull()) {
            devices.emplace_back(devName, devName);
        }
    }

    if(tempInit) {
        SDL_Quit();
    }

    return devices;
}

int SdlOutput::write(const AudioBuffer& buffer)
{
    if(SDL_QueueAudio(m_audioDeviceId, buffer.constData().data(), buffer.byteCount()) == 0) {
        return buffer.sampleCount();
    }

    return 0;
}

void SdlOutput::setPaused(bool pause)
{
    SDL_PauseAudioDevice(m_audioDeviceId, pause);
}

void SdlOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        m_device = device;
    }
}
} // namespace Fooyin::Sdl

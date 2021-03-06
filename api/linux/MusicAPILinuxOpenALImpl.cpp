/*
    This file is part of Soupe Au Caillou.

    @author Soupe au Caillou - Jordane Pelloux-Prayer
    @author Soupe au Caillou - Gautier Pelloux-Prayer
    @author Soupe au Caillou - Pierre-Eric Pelloux-Prayer

    Soupe Au Caillou is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3.

    Soupe Au Caillou is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Soupe Au Caillou.  If not, see <http://www.gnu.org/licenses/>.
*/



#include "MusicAPILinuxOpenALImpl.h"
#include "base/Log.h"

#if SAC_DARWIN || SAC_IOS
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include <vector>
#if SAC_EMSCRIPTEN
#include "base/TimeUtil.h"
#endif

#if SAC_DEBUG || SAC_EMSCRIPTEN
    static const char* errToString(ALenum err);
    static void check_AL_errors(const char* func, const char* context);
    #define AL_OPERATION(x)  \
         (x); \
         check_AL_errors(__FUNCTION__, #x);
#else
    #define AL_OPERATION(x) (x);
#endif

static std::vector<ALuint> unusedSources;
static std::vector<ALuint> unusedBuffers;

struct OpenALOpaqueMusicPtr : public OpaqueMusicPtr {
    ALuint source;
    std::vector<ALuint> queuedBuffers;
    std::vector<int> queuedSamples;
    int samplesPlayed;
    #if SAC_EMSCRIPTEN
    float startTime;
    int sampleRate;
    #endif
};

void MusicAPILinuxOpenALImpl::init() {
    ALCdevice* device = alcOpenDevice(0);
    ALCcontext* context = alcCreateContext(device, 0);
    if (!(device && context && alcMakeContextCurrent(context)))
        LOGE("Could not init AL library");
}

OpaqueMusicPtr* MusicAPILinuxOpenALImpl::createPlayer(int) {
    OpenALOpaqueMusicPtr* result = new OpenALOpaqueMusicPtr();
    result->samplesPlayed = 0;

    size_t s = unusedSources.size();
    if (s == 0) {
        // create source
        AL_OPERATION(alGenSources(1, &result->source))
    } else {
        result->source = unusedSources[s - 1];
        unusedSources.resize(s - 1);
    }
    return result;
}

void MusicAPILinuxOpenALImpl::queueMusicData(OpaqueMusicPtr* ptr, const short* data, int count, int sampleRate) {
    OpenALOpaqueMusicPtr* openalptr = static_cast<OpenALOpaqueMusicPtr*> (ptr);

    /* remove processed buffers */
    int done = 0;
    AL_OPERATION(alGetSourcei(openalptr->source, AL_BUFFERS_PROCESSED, &done))
    if (done > 0) {
        AL_OPERATION(alSourceUnqueueBuffers(openalptr->source, done, &openalptr->queuedBuffers[0]))
        unusedBuffers.insert(unusedBuffers.end(), openalptr->queuedBuffers.begin(), openalptr->queuedBuffers.begin() + done);
        openalptr->queuedBuffers.erase(openalptr->queuedBuffers.begin(), openalptr->queuedBuffers.begin() + done);
        for (int i=0; i<done; i++) {
            openalptr->samplesPlayed += openalptr->queuedSamples[i];
        }
        openalptr->queuedSamples.erase(openalptr->queuedSamples.begin(), openalptr->queuedSamples.begin() + done);
    }

    /* reuse buffer if possible */
    ALuint buffer;

    if (!unusedBuffers.empty()) {
        buffer = unusedBuffers[0];
        unusedBuffers.erase(unusedBuffers.begin());
    } else {
        AL_OPERATION(alGenBuffers(1, &buffer))
    }

    // create buffer
    AL_OPERATION(alBufferData(buffer, AL_FORMAT_MONO16, data, count * 2, sampleRate))
    AL_OPERATION(alSourceQueueBuffers(openalptr->source, 1, &buffer))
    openalptr->queuedBuffers.push_back(buffer);
    openalptr->queuedSamples.push_back(count);

#if SAC_EMSCRIPTEN
    openalptr->sampleRate = sampleRate;
#endif
}

void MusicAPILinuxOpenALImpl::startPlaying(OpaqueMusicPtr* ptr, OpaqueMusicPtr* master, int offset) {
    OpenALOpaqueMusicPtr* openalptr = static_cast<OpenALOpaqueMusicPtr*> (ptr);

#if SAC_EMSCRIPTEN
    openalptr->startTime = TimeUtil::GetTime();
#endif
    if (master) {
        int pos = getPosition(openalptr);
        setPosition(ptr, pos + offset);
    }
    AL_OPERATION(alSourcePlay(openalptr->source))
    LOGW_IF(!isPlaying(ptr), "Source was started but is not playing");
}

void MusicAPILinuxOpenALImpl::stopPlayer(OpaqueMusicPtr* ptr) {
    OpenALOpaqueMusicPtr* openalptr = static_cast<OpenALOpaqueMusicPtr*> (ptr);
    AL_OPERATION(alSourceStop(openalptr->source))
}

void MusicAPILinuxOpenALImpl::pausePlayer(OpaqueMusicPtr* ptr) {
    OpenALOpaqueMusicPtr* openalptr = static_cast<OpenALOpaqueMusicPtr*> (ptr);
    AL_OPERATION(alSourcePause(openalptr->source))
}

int MusicAPILinuxOpenALImpl::getPosition(OpaqueMusicPtr* ptr) {
    OpenALOpaqueMusicPtr* openalptr = static_cast<OpenALOpaqueMusicPtr*> (ptr);
    int pos = 0;
    #if SAC_EMSCRIPTEN
    LOGT_EVERY_N(100, "AL_SAMPLE_OFFSET support missing in emscripten");
    float elapsed = TimeUtil::GetTime() - openalptr->startTime;
    return elapsed * openalptr->sampleRate;
    #else
    AL_OPERATION(alGetSourcei(openalptr->source, AL_SAMPLE_OFFSET, &pos))
    pos += openalptr->samplesPlayed;
    #endif
    return pos;
}

void MusicAPILinuxOpenALImpl::setPosition(OpaqueMusicPtr* ptr, int pos) {
    OpenALOpaqueMusicPtr* openalptr = static_cast<OpenALOpaqueMusicPtr*> (ptr);
    #if SAC_EMSCRIPTEN
    LOGT("AL_SAMPLE_OFFSET support missing in emscripten");
    #else
    AL_OPERATION(alSourcei(openalptr->source, AL_SAMPLE_OFFSET, pos - openalptr->samplesPlayed))
    #endif
}

void MusicAPILinuxOpenALImpl::setVolume(OpaqueMusicPtr* ptr, float volume) {
    OpenALOpaqueMusicPtr* openalptr = static_cast<OpenALOpaqueMusicPtr*> (ptr);
    AL_OPERATION(alSourcef(openalptr->source, AL_GAIN, volume))
}

bool MusicAPILinuxOpenALImpl::isPlaying(OpaqueMusicPtr* ptr) {
    OpenALOpaqueMusicPtr* openalptr = static_cast<OpenALOpaqueMusicPtr*> (ptr);

    ALint state;
    AL_OPERATION(alGetSourcei(openalptr->source, AL_SOURCE_STATE, &state))
    return state == AL_PLAYING;
}

void MusicAPILinuxOpenALImpl::deletePlayer(OpaqueMusicPtr* ptr) {
    OpenALOpaqueMusicPtr* openalptr = static_cast<OpenALOpaqueMusicPtr*> (ptr);
    stopPlayer(ptr);
    // destroy buffers
    int count = (int)openalptr->queuedBuffers.size();
    AL_OPERATION(alSourceUnqueueBuffers(openalptr->source, count, &openalptr->queuedBuffers[0]))
    unusedBuffers.insert(unusedBuffers.end(), openalptr->queuedBuffers.begin(), openalptr->queuedBuffers.end());
    openalptr->queuedBuffers.clear();
    AL_OPERATION(alSourcei(openalptr->source, AL_BUFFER, 0))
    // destroy source
    unusedSources.push_back(openalptr->source);

    delete ptr;
}

#if SAC_DEBUG || SAC_EMSCRIPTEN
static void check_AL_errors(const char* func, const char* context) {
    int maxIterations=10;
    ALenum error;
    bool err = false;
    while (((error = alGetError()) != AL_NO_ERROR) && maxIterations > 0) {
        LOGW("OpenAL error during '" << func << ':' << context << "' -> " << errToString(error));
        maxIterations--;
        err = true;
    }
    LOGF_IF(err, "OpenAL error");
}

static const char* errToString(ALenum err) {
    switch (err) {
    case AL_NO_ERROR: return "AL(No error)";
    case AL_INVALID_NAME: return "AL(Invalid name)";
    case AL_INVALID_VALUE: return "AL(Invalid value)";
    case AL_INVALID_ENUM: return "AL(Invalid enum)";
    case AL_INVALID_OPERATION: return "AL(Invalid operation)";
    case AL_OUT_OF_MEMORY: return "AL(Out of memory)";
    default: return "AL(Unknown)";
    }
}
#endif

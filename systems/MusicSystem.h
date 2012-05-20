#pragma once

#include <map>

#include "base/MathUtil.h"
#include "base/Log.h"

#include "tremor/ivorbisfile.h"

#include "System.h"
#include "../api/AssetAPI.h"
#include "../api/MusicAPI.h"
#include <pthread.h>

typedef int MusicRef;

#define InvalidMusicRef -1

// #define MUSIC_VISU

struct MusicComponent {
	MusicComponent() : music(InvalidMusicRef), loopNext(InvalidMusicRef), positionI(0), positionF(0), master(0), volume(1), control(Stop) {
		opaque[0] = opaque[1] = 0;
	}

    MusicRef music, loopNext;
    MusicComponent* master;
    float loopAt; // sec
    int positionI; // in [0,1]
    float positionF;
    float fadeOut; // sec
    float volume, currentVolume;
    bool looped;
    enum {
        Start, Stop
    } control;

    // 2 opaque structure to allow overlapping looping
    struct OpaqueMusicPtr* opaque[2];
};

#define theMusicSystem MusicSystem::GetInstance()
#define MUSIC(e) theMusicSystem.Get(e)

UPDATABLE_SYSTEM(Music)

public:
void init();
bool isMuted() const { return muted; }

MusicRef loadMusicFile(const std::string& assetName);

void toggleMute(bool enable);

private:
/* textures cache */
MusicRef nextValidRef;

struct MusicInfo {
    MusicInfo() : ovf(0) {}
    OggVorbis_File* ovf;
    // track info
    float totalTime;
    int nbSamples;
    int sampleRate;
    // raw data
    int pcmBufferSize;
    int8_t* nextPcmBuffer;
    bool newBufferRequested;
};

std::map<MusicRef, MusicInfo> musics;
bool muted;
pthread_t oggDecompressionThread;
pthread_mutex_t mutex;
pthread_cond_t cond;
// map<filename, audio_compressed_content>
std::map<std::string, FileBuffer> name2buffer;

#ifdef MUSIC_VISU
std::map<Entity, Entity> visualisationEntities;
#endif

int decompressNextChunk(OggVorbis_File* file, int8_t* data, int chunkSize);
bool feed(OpaqueMusicPtr* ptr, MusicRef m, int forceCount);
OpaqueMusicPtr* startOpaque(MusicComponent* m, MusicRef r, MusicComponent* master, int offset);
void stopMusic(MusicComponent* m);
void clearAndRemoveInfo(MusicRef ref);
public:
MusicAPI* musicAPI;
AssetAPI* assetAPI;

void oggDecompRunLoop();
};


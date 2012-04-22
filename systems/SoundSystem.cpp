#include "SoundSystem.h"

INSTANCE_IMPL(SoundSystem);

SoundSystem::SoundSystem() : ComponentSystemImpl<SoundComponent>("sound"), nextValidRef(1), mute(false) {
}

void SoundSystem::init() {
	#ifndef ANDROID
	ALCdevice* Device = alcOpenDevice(NULL);
	ALCcontext* Context = alcCreateContext(Device, NULL);
	if (!(Device && Context && alcMakeContextCurrent(Context)))
		LOGI("probleme initialisation du son");
	for (int i=0; i<16; i++) {
		ALuint source;
		alGenSources(1, &source);
		availableSources.push_back(source);
		LOGI("AL source #%d created: %d", i, source);
	}
	#endif
}

SoundRef SoundSystem::loadSoundFile(const std::string& assetName, bool music) {
	#ifdef ANDROID
	if (!music && assetSounds.find(assetName) != assetSounds.end())
	#else
	if (assetSounds.find(assetName) != assetSounds.end())
	#endif
		return assetSounds[assetName];

	#ifdef ANDROID
	int soundID = androidSoundAPI->loadSound(assetName, music);
	#else
	ALuint soundID;
	if (!linuxSoundAPI->loadSound(assetName, &soundID)) {
		LOGW("Error loading sound: '%s'", assetName.c_str());
		return InvalidSoundRef;
	}
	#endif

	sounds[nextValidRef] = soundID;
	assetSounds[assetName] = nextValidRef;
	LOGW("Sound : %s -> %d -> %d", assetName.c_str(), nextValidRef, soundID);

	return nextValidRef++;
}

void SoundSystem::DoUpdate(float dt) {
	if (mute) {
		LOGW("Sondsystem muted");
		#ifdef ANDROID
		for(ComponentIt it=components.begin(); it!=components.end(); ++it) {
			Entity a = (*it).first;
			SoundComponent* rc = (*it).second;
			if (rc->type == SoundComponent::EFFECT) { //on supprime tous les sons (pas les musics)
				rc->sound = InvalidSoundRef;
				rc->started = false;
			}
		}
		androidSoundAPI->pauseAll();
		#else
		for (std::list<ALuint>::iterator it=activeSources.begin(); it!=activeSources.end();) {
			std::list<ALuint>::iterator jt = it++;
			ALuint source = *jt;
			for(ComponentIt it=components.begin(); it!=components.end(); ++it) {
				Entity a = (*it).first;
				SoundComponent* rc = (*it).second;
				if (rc->source==source) {
					if (rc->type == SoundComponent::MUSIC) {
						alSourcePause(source);
					} else {
						availableSources.push_back(source);
						activeSources.erase(jt);
					}
					break;
				}
			}
		}
		#endif
	} else {
		#ifdef ANDROID
		androidSoundAPI->resumeAll();
		#else
		for (std::list<ALuint>::iterator it=activeSources.begin(); it!=activeSources.end();) {
			std::list<ALuint>::iterator jt = it++;
			ALuint source = *jt;
			ALint v;
			alGetSourcei(source, AL_SOURCE_STATE, &v);
			if (v == AL_PAUSED	) {
				alSourcePlay(source);
			}
		}
		#endif
	}
	/* play component with a valid sound ref */
	for(ComponentIt it=components.begin(); it!=components.end(); ++it) {
		Entity a = (*it).first;
		SoundComponent* rc = (*it).second;
		if (rc->sound != InvalidSoundRef && !mute ) {
			if (!rc->started && !rc->stop) {
				LOGW("sound(%d) started (%d) at %f", a, rc->sound, rc->position);
				#ifdef ANDROID
				androidSoundAPI->play(sounds[rc->sound], (rc->type == SoundComponent::MUSIC));
				#else
				// use 1st available source
				if (availableSources.size() > 0) {
					rc->source = linuxSoundAPI->play(sounds[rc->sound], availableSources.front(), rc->position);
					rc->position = linuxSoundAPI->musicPos(rc->source, sounds[rc->sound]);
					if (rc->masterTrack) {
						ALint pos;
						alGetSourcei(rc->masterTrack->source, AL_BYTE_OFFSET, &pos);
						alSourcei(rc->source, AL_BYTE_OFFSET, pos);
					}
					activeSources.push_back(availableSources.front());
					availableSources.pop_front();
				}
				#endif
				rc->started = true;
			} else if (rc->type == SoundComponent::MUSIC) {
				 #ifdef ANDROID
				float newPos = androidSoundAPI->musicPos(sounds[rc->sound]);
				#else
				ALfloat newPos = linuxSoundAPI->musicPos(rc->source, sounds[rc->sound]);
				#endif
				if (newPos >= 0.995 || (rc->stop && newPos!=0)) {
					LOGW("sound ended (%d)", rc->sound);
					rc->position = 0;
					if (!rc->repeat) {
						#ifndef ANDROID
						if (rc->stop && newPos!=0) {
							alSourceStop(rc->source);
						}
						activeSources.remove(rc->source);
						availableSources.push_back(rc->source);
						rc->source = 0;
                        #else
                        if (rc->stop) {
                            androidSoundAPI->stop(sounds[rc->sound], true);
                        }
						#endif
						rc->sound = InvalidSoundRef;
						rc->started = false;
						rc->stop = false;
					}
				} else {
					rc->position = newPos;
				}
			} else if (rc->type == SoundComponent::EFFECT) {
				rc->sound = InvalidSoundRef;
				rc->started = false;
			}
		}
	}

	#ifndef ANDROID
	// browse active source and destroy ended sounds
	for (std::list<ALuint>::iterator it=activeSources.begin(); it!=activeSources.end();) {
		std::list<ALuint>::iterator jt = it++;
		ALuint source = *jt;
		ALint v;
		alGetSourcei(source, AL_SOURCE_STATE, &v);
		if (v != AL_PLAYING && v != AL_PAUSED) {
			availableSources.push_back(source);
			activeSources.erase(jt);
		}
	}
	#endif
	
	//debug test
	//~ #ifndef ANDROID
	//~ int cpt;
	//~ for(ComponentIt it=components.begin(); it!=components.end(); ++it) {
		//~ if 	(linuxSoundAPI->musicPos(it->second->source, sounds[it->second->sound])>0.f && !mute) 
			//~ cpt++;
	//~ }
	//~ LOGI("%d playing (%d busy - %d availables sources) from %d entities", cpt, activeSources.size(), availableSources.size(), components.size());
	//~ #endif
}

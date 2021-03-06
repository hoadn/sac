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

#pragma once

#if SAC_LINUX && SAC_DESKTOP
#define SAC_RECORDER 1
#else
#undef SAC_RECORDER
#endif

#if SAC_RECORDER
#include <cstdlib>
#include <cstdio>
#include <queue>

#include <thread>
#include <mutex>
#include <condition_variable>

#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>

#include "systems/opengl/OpenglHelper.h"
#endif

#include <glm/glm.hpp>

class Recorder {
    public:
    static const int PBO_COUNT = 2;
    static const int CHANNEL_COUNT = 4;

    static Recorder& Instance();
    void deinit();

    ~Recorder();

    void init(const glm::vec2& captureOffset = glm::vec2(0.0f),
              const glm::vec2& captureSize = glm::vec2(0.0f));
    void start();
    void stop();
    void toggle();
    bool isRecording() const;

    void record(float dt);

    void thread_video_encode();

#if SAC_RECORDER
    private:
    Recorder() {}
    void addFrame(GLubyte* ptr, const glm::vec2& mousePos);

    bool initVP8();
    bool initOpenGl_PBO();
    bool initSound();

    glm::vec2 captureOffset, captureSize;
    bool recording;
    float recordingStartTime;
    float frameGrabAccum;

    GLuint pboIds[PBO_COUNT];

    std::queue<GLubyte*> buf;
    std::queue<glm::vec2> mousePosition;

    vpx_codec_ctx_t codec;
    vpx_codec_enc_cfg_t cfg;
    vpx_image_t raw;

    int frameCounter;
    int flags;

    FILE* outfile;

    std::thread th1;
    std::mutex mutex_buf;
    std::condition_variable cond;
#endif
};

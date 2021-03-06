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



#include "Recorder.h"
#if SAC_RECORDER
#include <iostream>
#include <ctime>
#include <SDL.h>
#include <cstring>
#include <string>
#include <sstream>
#include "base/Profiler.h"
#include "base/Log.h"
#include "base/TimeUtil.h"
#include "systems/RenderingSystem.h"

#if SAC_INGAME_EDITORS
#include "util/LevelEditor.h"
#endif

#define VPX_CODEC_DISABLE_COMPAT 1
#define interface (vpx_codec_vp8_cx())
#define fourcc    0x30385056

static void* videoEncoder_Callback(void* obj){
    static_cast<Recorder*>(obj)->thread_video_encode();

    return NULL;
}

static void mem_put_le16(char *mem, unsigned int val) {
    mem[0] = val;
    mem[1] = val>>8;
}

static void mem_put_le32(char *mem, unsigned int val) {
    mem[0] = val;
    mem[1] = val>>8;
    mem[2] = val>>16;
    mem[3] = val>>24;
}

static void write_ivf_file_header(FILE *outfile,
                                  const vpx_codec_enc_cfg_t *cfg,
                                  int frame_cnt) {
    char header[32];

    if(cfg->g_pass != VPX_RC_ONE_PASS && cfg->g_pass != VPX_RC_LAST_PASS)
        return;
    header[0] = 'D';
    header[1] = 'K';
    header[2] = 'I';
    header[3] = 'F';
    mem_put_le16(header+4,  0);                   /* version */
    mem_put_le16(header+6,  32);                  /* headersize */
    mem_put_le32(header+8,  fourcc);              /* headersize */
    mem_put_le16(header+12, cfg->g_w);            /* width */
    mem_put_le16(header+14, cfg->g_h);            /* height */
    mem_put_le32(header+16, cfg->g_timebase.den); /* rate */
    mem_put_le32(header+20, cfg->g_timebase.num); /* scale */
    mem_put_le32(header+24, frame_cnt);           /* length */
    mem_put_le32(header+28, 0);                   /* unused */

    if(fwrite(header, 1, 32, outfile)){}
}

static void write_ivf_frame_header(FILE *outfile,
                                   const vpx_codec_cx_pkt_t *pkt)
{
    char             header[12];
    vpx_codec_pts_t  pts;

    if(pkt->kind != VPX_CODEC_CX_FRAME_PKT){
        LOGE("Error");
        return;
    }

    pts = pkt->data.frame.pts;
    mem_put_le32(header, pkt->data.frame.sz);
    mem_put_le32(header+4, pts&0xFFFFFFFF);
    mem_put_le32(header+8, pts >> 32);

    if(fwrite(header, 1, 12, outfile)){}
}

static void RGB_To_YV12( unsigned char *pRGBData, int nFrameWidth, int nFrameHeight, int nFrameChannel, void *pFullYPlane, void *pDownsampledUPlane, void *pDownsampledVPlane )
{
    // Convert RGB -> YV12. We do this in-place to avoid allocating any more memory.
    unsigned char *pYPlaneOut = (unsigned char*)pFullYPlane;
    int nYPlaneOut = 0;

    for ( int y=nFrameHeight-1; y > -1 ; --y)
    {
        for (int x=0; x < nFrameWidth*nFrameChannel; x+=nFrameChannel)
        {
            unsigned char B = pRGBData[x+0+y*nFrameWidth*nFrameChannel];
            unsigned char G = pRGBData[x+1+y*nFrameWidth*nFrameChannel];
            unsigned char R = pRGBData[x+2+y*nFrameWidth*nFrameChannel];

            float Y = (float)( R*66 + G*129 + B*25 + 128 ) / 256 + 16;
            float U = (float)( R*-38 + G*-74 + B*112 + 128 ) / 256 + 128;
            float V = (float)( R*112 + G*-94 + B*-18 + 128 ) / 256 + 128;

            // NOTE: We're converting pRGBData to YUV in-place here as well as writing out YUV to pFullYPlane/pDownsampledUPlane/pDownsampledVPlane.
            pRGBData[x+0+y*nFrameWidth*nFrameChannel] = (unsigned char)Y;
            pRGBData[x+1+y*nFrameWidth*nFrameChannel] = (unsigned char)U;
            pRGBData[x+2+y*nFrameWidth*nFrameChannel] = (unsigned char)V;

            // Write out the Y plane directly here rather than in another loop.
            pYPlaneOut[nYPlaneOut++] = pRGBData[x+0+y*nFrameWidth*nFrameChannel];
        }
    }
    // Downsample to U and V.
    int halfHeight = nFrameHeight >> 1;
    int halfWidth = nFrameWidth >> 1;

    unsigned char *pVPlaneOut = (unsigned char*)pDownsampledVPlane;
    unsigned char *pUPlaneOut = (unsigned char*)pDownsampledUPlane;

    for ( int yPixel=0; yPixel < halfHeight; yPixel++ )
    {
        int iBaseSrc = ( (yPixel*2) * nFrameWidth * nFrameChannel );

        for ( int xPixel=0; xPixel < halfWidth; xPixel++ )
        {
            pVPlaneOut[(halfHeight-1 - yPixel) * halfWidth + xPixel] = pRGBData[iBaseSrc + 2];
            pUPlaneOut[(halfHeight-1 - yPixel) * halfWidth + xPixel] = pRGBData[iBaseSrc + 1];

            iBaseSrc += nFrameChannel * 2;
        }
    }
}

#define TARGET_FPS 60.0
#define TIME_BETWEEN_FRAME (1.0f/TARGET_FPS)

Recorder & Recorder::Instance() {
    static Recorder instance;
    return instance;
}

void Recorder::init(const glm::vec2& offset, const glm::vec2& size) {
    captureSize = size;

    if (captureSize.x == 0 && captureSize.y == 0) {
        captureSize.x = theRenderingSystem.windowW;
        captureSize.y = theRenderingSystem.windowH;
#if SAC_INGAME_EDITORS
        captureSize.x += LevelEditor::DebugAreaWidth;
        captureSize.y += LevelEditor::DebugAreaHeight;
#endif
    }
    captureOffset = offset;
    outfile = NULL;
    recording = false;
    flags = 0;

    initVP8();
}

void Recorder::deinit() {
    th1.join();
    vpx_codec_destroy(&codec);
    vpx_img_free (&raw);

}

Recorder::~Recorder(){
    if (recording) {
        //~ if (pthread_cancel (th1) != 0) {
            //~ LOGE("pthread_cancel error for thread");
        //~ }
    }
    GL_OPERATION(glDeleteBuffers(PBO_COUNT, pboIds))
}

bool Recorder::initOpenGl_PBO (){
    // init PBOs
    int size =
#if SAC_INGAME_EDITORS
        (theRenderingSystem.windowW + LevelEditor::DebugAreaWidth) *
        (theRenderingSystem.windowH + LevelEditor::DebugAreaHeight) *
        CHANNEL_COUNT;
#else
        theRenderingSystem.windowW * theRenderingSystem.windowH * CHANNEL_COUNT;
#endif

    GL_OPERATION(glGenBuffers(PBO_COUNT, pboIds))
    GL_OPERATION(glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[0]))
    GL_OPERATION(glBufferData(GL_PIXEL_PACK_BUFFER, size, 0, GL_STREAM_READ))
    GL_OPERATION(glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[1]))
    GL_OPERATION(glBufferData(GL_PIXEL_PACK_BUFFER, size, 0, GL_STREAM_READ))

    return true;
}

bool Recorder::initVP8 (){
    // init VP8 encoder
    /* Populate encoder configuration */
    vpx_codec_err_t res = vpx_codec_enc_config_default(interface, &cfg, 0);
    if (res) {
        LOGE("Failed to get config: " << vpx_codec_err_to_string(res));
        return false;
    }
    /* Update the default configuration with our settings */
    cfg.rc_target_bitrate =2000;//(width*height*3*8*30) /1000;
    cfg.g_w = (int)captureSize.x;
    cfg.g_h = (int)captureSize.y;
    cfg.kf_mode = VPX_KF_AUTO;
    cfg.kf_max_dist = 300;
    cfg.g_pass = VPX_RC_ONE_PASS; /* -p 1 */
    cfg.g_timebase.num = 1;
    cfg.g_timebase.den = TARGET_FPS; /* fps = 30 */
    cfg.rc_end_usage = VPX_VBR; /* --end-usage=cbr */
    cfg.g_threads = 2;

    if (vpx_codec_enc_init(&codec, interface, &cfg, 0)){
        LOGE("Failed to initialize encoder");
        return false;
    }

    vpx_codec_control(&codec, VP8E_SET_CPUUSED, (cfg.g_threads > 1) ? 10 : 10);
    vpx_codec_control(&codec, VP8E_SET_STATIC_THRESHOLD, 0);
    vpx_codec_control(&codec, VP8E_SET_ENABLEAUTOALTREF, 1);

    if (cfg.g_threads > 1) {
        if (vpx_codec_control(&codec, VP8E_SET_TOKEN_PARTITIONS, (vp8e_token_partitions) cfg.g_threads) != VPX_CODEC_OK) {
            LOGE("VP8: failed to set multiple token partition");
        } else {
            //- LOGE("VP8: multiple token partitions used")
        }
    }

    if (!vpx_img_alloc(&raw, VPX_IMG_FMT_I420, cfg.g_w, cfg.g_h, 1)){
        LOGE("Failed to allocate image");
        return false;
    }

    return true;
}

bool Recorder::initSound (){
    return true;
}

void Recorder::start(){
    if (outfile == NULL && !recording && !th1.joinable()){

        //new file : time
        char tmp[256];
        long H = time(NULL);
        strftime(tmp, sizeof(tmp), "/tmp/sac_record_%Y%m%d_%H%M%S.webm", localtime(&H));

        if(!(outfile = fopen(tmp, "wb"))){
            LOGE("Failed to open '" << tmp << "' for writing. Cancelling recording");
            outfile = NULL;
            return;
        }
        LOGI("Recording start in file '" << tmp << "'");
        write_ivf_file_header(outfile, &cfg, 0);
        this->frameCounter = 0;
        recordingStartTime = TimeUtil::GetTime();
        frameGrabAccum = 0;
        recording = true;

        // on lance le thread pour l'encodage
        th1 = std::thread(videoEncoder_Callback, (void*) this);
        if (!th1.joinable()) {
            LOGE("thread creating error");
        }
    }
}

void Recorder::stop(){
    if (outfile != NULL && recording == true){
        #if SAC_ENABLE_LOG
        float duration = TimeUtil::GetTime() - recordingStartTime;
        #endif
        LOGI("Recording stop (" << duration << " sec, " << frameCounter << " images -> " << frameCounter / duration << " fps)");
        mutex_buf.lock();
        cond.notify_all();
        buf.push(NULL);
        mousePosition.push(glm::vec2(-10,-10));
        mutex_buf.unlock();
        recording = false;
    }
}

void Recorder::toggle(){
    if (outfile)
        stop();
    else
        start();
}

void Recorder::record(float dt){
    static int index = 0;

    if (recording && outfile){
        if (pboIds[0] == 0) {
            initOpenGl_PBO();
        }

        frameGrabAccum += dt;

        if (frameGrabAccum >= TIME_BETWEEN_FRAME) {
            PROFILE("Recorder", "read-request", BeginEvent);
            // "index" is used to read pixels from framebuffer to a PBO
            // "nextIndex" is used to update pixels in the other PBO
            index = (index + 1) % PBO_COUNT;
            int nextIndex = (index + 1) % PBO_COUNT;

            // set the target framebuffer to read
            GL_OPERATION(glReadBuffer(GL_FRONT))

            const int w = (int)captureSize.x;
            const int h = (int)captureSize.y;
            // read pixels from framebuffer to PBO
            // glReadPixels() should return immediately.
            GL_OPERATION(glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[index]))
            GL_OPERATION(glReadPixels((int)captureOffset.x, (int)captureOffset.y, w, h, GL_BGRA, GL_UNSIGNED_BYTE, 0))
            PROFILE("Recorder", "read-request", EndEvent);
            PROFILE("Recorder", "read-back", BeginEvent);
            // map the PBO to process its data by CPU
            GL_OPERATION(glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[nextIndex]))
            GLubyte* ptr = NULL;
            GL_OPERATION (ptr = (GLubyte*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY))
            if(ptr)
            {
                GLubyte *test = new GLubyte[w*h * CHANNEL_COUNT];
                memcpy (test, ptr, w*h*CHANNEL_COUNT );

                mutex_buf.lock();
                buf.push(test);

                int x, y;
                SDL_GetMouseState(&x, &y);
                mousePosition.push( glm::vec2( x, y) );

                cond.notify_all();
                mutex_buf.unlock();

                GL_OPERATION(glUnmapBuffer(GL_PIXEL_PACK_BUFFER))
            }

            // back to conventional pixel operation
            GL_OPERATION(glBindBuffer(GL_PIXEL_PACK_BUFFER, 0))
            PROFILE("Recorder", "read-back", EndEvent);

            frameGrabAccum -= TIME_BETWEEN_FRAME;
        }
    }
}

void Recorder::thread_video_encode(){
    while (recording || (outfile != NULL && !buf.empty()) ) {
        std::unique_lock<std::mutex> lock(mutex_buf);
        while (buf.empty())
            cond.wait(lock);
        GLubyte *ptr = buf.front();
        glm::vec2 mousePos = mousePosition.front();
        buf.pop();
        mousePosition.pop();
        lock.unlock();
        addFrame(ptr, mousePos);
        if (ptr)
            delete [] ptr;
    }

    if(outfile != NULL) {
        if(!fseek(outfile, 0, SEEK_SET))
            write_ivf_file_header(outfile, &cfg, this->frameCounter-1);

        fclose(outfile);

        outfile = NULL;

        LOGI("Recording is available");
    }
}

void drawPoint(GLubyte* ptr, int width, int height, int channelCount, int cursorX, int cursorY) {
    LOGV(2, "Cursor position: " << cursorX << " , " << cursorY);

    //warning: cursorY is inverted relatively to the byte array!
    int yMin = std::max((height - cursorY) - 10, 0);
    int yMax = std::min((height - cursorY) + 10, height);
    for ( int y = yMin; y < yMax; ++y) {
        float t = (y - yMin) * 1. / (yMax - yMin);
        //draw a "triangle" like shape
        for (int x = std::max(cursorX - (int)(t * 10), 0); x < std::min(cursorX + (int)(t * 10), width); ++x) {
            int pos = (x + y * width) * channelCount;
            ptr[0 + pos] = 0;
            ptr[1 + pos] = 0;
            ptr[2 + pos] = 0;
        }
    }
}

void Recorder::addFrame(GLubyte *ptr, const glm::vec2& mousePos){
    vpx_codec_iter_t iter = NULL;
    const vpx_codec_cx_pkt_t *pkt;

    if (ptr) {
        //retrieve cursor position
        //int x, y;
        //SDL_GetMouseState(&x, &y);
        //the offset seems to be only needed on x axis
        //x -= captureOffset.x;
        
        drawPoint(ptr, cfg.g_w, cfg.g_h, CHANNEL_COUNT, mousePos.x-captureOffset.x, mousePos.y);
        RGB_To_YV12(ptr, cfg.g_w, cfg.g_h, CHANNEL_COUNT, raw.planes[0], raw.planes[1], raw.planes[2]);
    }


    PROFILE("Recorder", "encode-image", BeginEvent);
    if(vpx_codec_encode(&codec, ptr ? &raw : NULL, this->frameCounter,
                        1, flags, VPX_DL_REALTIME)){
        const char *detail = vpx_codec_error_detail(&codec);
        std::stringstream ss;
        ss << "Failed to encode frame";
        if (detail) ss << ":\t" << detail;
        LOGE(ss.str());
    }
    PROFILE("Recorder", "encode-image", EndEvent);
    PROFILE("Recorder", "write-disk", BeginEvent);
    while( (pkt = vpx_codec_get_cx_data(&codec, &iter)) ) {
        switch(pkt->kind) {
        case VPX_CODEC_CX_FRAME_PKT:
            write_ivf_frame_header(outfile, pkt);
            if(fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz,
                      outfile)){}
            break;
        default:
            break;
        }
    }
    PROFILE("Recorder", "write-disk", EndEvent);
    ++this->frameCounter;
}

bool Recorder::isRecording() const { return recording; }

#else
void Recorder::init(glm::detail::tvec2<float, (glm::precision)0> const&, glm::detail::tvec2<float, (glm::precision)0> const&) {}
void Recorder::stop() {}
void Recorder::start() {}
void Recorder::deinit() {}
void Recorder::record(float) {}
bool Recorder::isRecording() const { return false; }
Recorder::~Recorder() {}
Recorder& Recorder::Instance() {
        static Recorder r;
        return r;
}
#endif

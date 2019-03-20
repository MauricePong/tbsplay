#ifndef HEAD_H
#define HEAD_H
#include <iostream>
using namespace std;
extern "C"{
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <asm/types.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include "libavcodec/avcodec.h"
#include "libavcodec/vorbis_parser.h"
#include "libavformat/avformat.h"
#include "libavutil/time.h"
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavdevice/avdevice.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/hwcontext.h"
#include "libavutil/avutil.h"
#include "libavutil/pixdesc.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_audio.h"
#include "SDL2/SDL_types.h"
#include "SDL2/SDL_name.h"
#include "SDL2/SDL_main.h"
#include "SDL2/SDL_config.h"
#include "SDL2/SDL_thread.h"
#include "SDL2/SDL_events.h"

#include "get_media_devices.h"
}

typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef FLOAT               *PFLOAT;
typedef int                 INT;
typedef unsigned int        UINT;
typedef unsigned int        *PUINT;
typedef unsigned long ULONG_PTR, *PULONG_PTR;
typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;

#define MAKEWORD(a, b)      ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define LOWORD(l)           ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l)           ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w)           ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w)           ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define SDL_AUDIO_BUFFER_SIZE 1024
//4096
//1024
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#define FLUSH_DATA "FLUSH"
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#define MAX_AUDIO_SIZE (25 * 16 * 1024)
#define MAX_VIDEO_SIZE (25 * 256 * 1024)

#define VIDEO_PICTURE_QUEUE_SIZE 300
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE  900
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

class PacketQueue {
public:
    AVPacketList *first_pkt;
    AVPacketList *last_pkt;
    int nb_packets;
    size_t size;
    SDL_mutex *mutex;
    SDL_cond *cond;
};

class BufferDataNode
{
public:
    uint8_t * buffer;
    size_t bufferSize;
    BufferDataNode * next;
};

class BufferQueue{
public:
    BufferDataNode * DataQueneHead;
    BufferDataNode * DataQueneTail;
    SDL_mutex *Mutex;
};

class FrameNode {
public:
    AVFrame *frame = nullptr;
    FrameNode *next = nullptr;
};

class FrameQueue {
public:
    FrameNode *FrameQueneHead = nullptr;
    FrameNode *FrameQueneTail = nullptr;
    int size = 0;
    SDL_mutex *Mutex = nullptr;
};

class Mdata{
public:
    string val;
    int en = {0};
};

class Parse_Parameter{
public:
    Mdata v;
    Mdata a;
    Mdata i;
    Mdata c;
    Mdata o;
    Mdata m;
    Mdata x;
    Mdata y;
    Mdata k;
    int f = {0};
    int e = {0};
    int h = {0};
    int d = {0};
    int M = {0};
};

class AVInputfilename{
public:
    string videname;
    string audioname;
};

class Capture_parse_ctx{
public:
    string v;
    string a;
    string encoder;
    string o;
    string m;
    string k;
    int w = 0;
    int h = 0;
    int fps = 0;
    int vbitrate = 0;
    int sample = 0;
    int fs = 0;
};

class AVFilterPacket{
public:
    AVFilterContext *filter_buffer_ctx;
    AVFilterContext *filter_buffersink_ctx;
    AVFilterGraph *filter_graph;
};

class SDL2_Packet{
public:
    SDL_AudioDeviceID mAudioID;
    SDL_Rect sdlRect;
    SDL_Renderer* sdlRenderer;
    SDL_Texture *sdlTexture;
    SDL_Window *screen;
};

class AV_PTS{
public:
    double val = {0.0};
    int64_t count = 0;
};


class Capture_ctx{
public:
    AVBufferRef *hw_device_ctx = nullptr;
    AVFormatContext *vInputFmtCtx = nullptr;
    AVFormatContext *aInputFmtCtx = nullptr;
    AVFormatContext *avOutputFmtCtx = nullptr;
    int vStreamNb = -1;
    int aStreamNb= -1;
    AVStream *vStream = nullptr;
    AVStream *aStream = nullptr;
    AVStream *vOutStream = nullptr;
    AVStream *aOutStream = nullptr;
    AVCodec *vDecoder = nullptr;
    AVCodec *aDecoder = nullptr;
    AVCodecContext *vDecoderCtx = nullptr;
    AVCodecContext *aDecoderCtx = nullptr;
    AVCodec *vEncoder = nullptr;
    AVCodec *aEncoder = nullptr;
    AVCodecContext *vEncoderCtx = nullptr;
    AVCodecContext *aEncoderCtx = nullptr;
    Capture_parse_ctx parse_ctx;
    int quit = 0;

    enum AVSampleFormat audio_src_fmt;
    enum AVSampleFormat audio_tgt_fmt;
    int audio_src_channels;
    int audio_tgt_channels;
    int64_t audio_src_channel_layout;
    int64_t audio_tgt_channel_layout;
    int audio_src_freq;
    int audio_tgt_freq;
    unsigned int audio_buf_size;
    unsigned int audio_buf_index;
    AVPacket audio_pkt;
    uint8_t *audio_pkt_data;
    int audio_pkt_size;
    uint8_t *audio_buf;
    DECLARE_ALIGNED(16,uint8_t,audio_buf2) [AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
    struct SwrContext *swr_ctx; //用于解码后的音频格式转换
    int audio_hw_buf_size;
    double audio_clock; ///音频时钟
    double video_clock; ///<pts of last decoded frame / predicted pts of next decoded frame
    PacketQueue videoq;
    PacketQueue audioq;
    FrameQueue videofq;
    FrameQueue audiofq;
    /// 跳转相关的变量
    ///播放控制相关
    int areadFinished = 0; //文件读取完毕
    int vreadFinished = 0; //文件读取完毕
    int audioFinished = 0;
    int videoFinished = 0;
    int avencodeFinished =0;
    int adecFinished = 0;
    int vdecFinished = 0;
    AV_PTS apts;
    AV_PTS vpts;
    bool isMute; //静音标识
    float mVolume; //0~1 超过1 表示放大倍数

    SDL2_Packet sdl2pkt;
    AVFilterPacket afilterpkt;
    AVFilterPacket vfilterpkt;
};

class AVFile_Ctx{
public:
    AVFormatContext *InputFmtCtx = nullptr;
    int vStreamNb = -1;
    int aStreamNb= -1;
    AVStream *vStream = nullptr;
    AVStream *aStream = nullptr;
    AVCodec *vDecoder = nullptr;
    AVCodec *aDecoder = nullptr;
    AVCodecContext *vDecoderCtx = nullptr;
    AVCodecContext *aDecoderCtx = nullptr;
    enum AVSampleFormat audio_src_fmt;
    enum AVSampleFormat audio_tgt_fmt;
    int audio_src_channels;
    int audio_tgt_channels;
    int64_t audio_src_channel_layout;
    int64_t audio_tgt_channel_layout;
    int audio_src_freq;
    int audio_tgt_freq;
    unsigned int audio_buf_size;
    unsigned int audio_buf_index;
    AVPacket audio_pkt;
    uint8_t *audio_pkt_data;
    int audio_pkt_size;
    uint8_t *audio_buf;
    DECLARE_ALIGNED(16,uint8_t,audio_buf2) [AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
    struct SwrContext *swr_ctx; //用于解码后的音频格式转换
    int audio_hw_buf_size;
    double audio_clock; ///音频时钟
    double video_clock; ///<pts of last decoded frame / predicted pts of next decoded frame
    PacketQueue videoq;
    PacketQueue audioq;
    int areadFinished = 0; //文件读取完毕
    int vreadFinished = 0; //文件读取完毕
    int avplayerFinshed = 0;
    bool isMute; //静音标识
    float mVolume; //0~1 超过1 表示放大倍数
    int quit = 0;
    int w = 0;
    int h = 0;
    int vbitrate = 0;
    int fps = 0;
    int sample = 0;
    int fs = 0;
    int k = 0;
    string i;
    SDL2_Packet sdl2pkt;
    AVFilterPacket vfilterpkt;
};

class HWDevice {
public:
    char *name;
    enum AVHWDeviceType type;
    AVBufferRef *device_ref;
};



extern void DataQuene_Input(BufferQueue *bq,uint8_t * buffer,size_t size);
extern BufferDataNode *DataQuene_get(BufferQueue *bq);
extern void buffer_queue_flush(BufferQueue *bq);
extern void buffer_queue_deinit(BufferQueue *bq);
extern void buffer_queue_init(BufferQueue *bq);

extern void packet_queue_flush(PacketQueue *q);
extern void packet_queue_deinit(PacketQueue *q);
extern void packet_queue_init(PacketQueue *q);
extern int packet_queue_put(PacketQueue *q, AVPacket *pkt);
extern int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);

extern void FrameQuene_Input(FrameQueue *fq,AVFrame *avframe);
extern FrameNode *FrameQuene_get(FrameQueue *fq);
extern void frame_queue_flush(FrameQueue *fq);
extern void frame_queue_deinit(FrameQueue *fq);
extern void frame_queue_init(FrameQueue *fq);

extern void enum_muxer_format(void);

//hw

extern HWDevice *hw_device_get_by_name(const char * name);
extern int hw_device_init_from_string(const char *arg, HWDevice **dev_out);
extern void hw_device_free_all(void);
#endif

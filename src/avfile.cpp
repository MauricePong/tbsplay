#include "avfile.h"
static AVFile_Ctx gavf_ctx;

AVFile::AVFile()
{
    gavf_ctx.areadFinished = 0;
    gavf_ctx.vreadFinished = 0;
    gavf_ctx.avplayerFinshed = 0;
    gavf_ctx.quit = 0;
    avdevice_register_all();
    avformat_network_init();
    //init SDL windows////////////////////////////
    if(!SDL_getenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE")){
        SDL_setenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE","1",1);
    }

    if(SDL_Init(SDL_INIT_VIDEO| SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf( "initialize SDL - %s\n", SDL_GetError());
    }
}

AVFile::~AVFile()
{
    gavf_ctx.quit = 1;
    gavf_ctx.areadFinished = 1;
    gavf_ctx.vreadFinished = 1;
    gavf_ctx.avplayerFinshed = 1;
    SDL_Quit();
}

int AVFile::check_parameter(Parse_Parameter *pp)
{
    int ret = 0;
    if(0 == pp->i.en){
        ret = -1;
    }

    if(1 == pp->o.en){
        if(pp->o.val.find("sdl2") == string::npos){
            ret = -1;
        }
    }

    return 0;
}

static int keydone_thread(void *)
{
    SDL_Event myEvent;
    while(!gavf_ctx.quit){
        SDL_WaitEvent(&myEvent);
        switch (myEvent.type) {
        case SDL_KEYDOWN:
            if(myEvent.key.keysym.sym == SDLK_ESCAPE){
                gavf_ctx.quit = 1;
                break;
            }else if(myEvent.key.keysym.sym == SDLK_F11){
                SDL_SetWindowFullscreen(gavf_ctx.sdl2pkt.screen,static_cast<uint32_t>(gavf_ctx.fs));
                gavf_ctx.fs =  gavf_ctx.fs ? 0:1;
            }
            break;
        case SDL_QUIT:
            gavf_ctx.quit = 1;
            break;
        default:
            break;
        }
        SDL_Delay(1);
    }
    return 0;
}

static int interrupt_cb(void *ctx)
{
    // auto ptr = static_cast<time_t*>(ctx);
    // int ret = time(nullptr) - *ptr >= 5 ? 1: 0;
    // if(1 == ret){
    //    gavf_ctx.quit = 1;
    //    gavf_ctx.avplayerFinshed = 1;
    //   printf("Access av file timeout 5 s,then exit..\n");
    // }
    // return ret;
}

static double synchronize_video(AVFrame *src_frame, double pts)
{
    double frame_delay;
    if (pts != 0.0) {
        /* if we have pts, set video clock to it */
        gavf_ctx.video_clock = pts;
    } else {
        /* if we aren't given a pts, set it to the clock */
        pts = gavf_ctx.video_clock;
    }
    /* update the video clock */
    frame_delay = av_q2d(gavf_ctx.vDecoderCtx->time_base);
    /* if we are repeating a frame, adjust clock accordingly */
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    gavf_ctx.video_clock += frame_delay;
    return pts;
}

static int video_thread(void *)
{
    int ret = 0;
    AVPacket pkt1, *packet = &pkt1;
    int got_picture;
    double video_pts = 0; //当前视频的pts
    double audio_pts = 0; //音频pts
    int w,h = 0;
    ///解码视频相关
    AVFrame *pFrame;
    char titile[36] = {'\0'};
    sprintf(titile,"tbsplay,%s",gavf_ctx.i.data());
    pFrame = av_frame_alloc();
    gavf_ctx.sdl2pkt.screen = SDL_CreateWindow(titile, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                               gavf_ctx.vDecoderCtx->width, gavf_ctx.vDecoderCtx->height,SDL_WINDOW_OPENGL|
                                               SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!gavf_ctx.sdl2pkt.screen) {
        printf("SDL: could not create window - exiting:%s\n",SDL_GetError());
        return -1;
    }

    SDL_SetWindowFullscreen(gavf_ctx.sdl2pkt.screen,static_cast<uint32_t>(gavf_ctx.fs));
    gavf_ctx.sdl2pkt.sdlRenderer = SDL_CreateRenderer(gavf_ctx.sdl2pkt.screen, -1, SDL_RENDERER_ACCELERATED
                                                      |SDL_RENDERER_PRESENTVSYNC|SDL_RENDERER_TARGETTEXTURE);
    SDL_SetRenderDrawColor(gavf_ctx.sdl2pkt.sdlRenderer,0,0,0,255);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");

    Uint32 pixformat= 0;
    if(gavf_ctx.i.find(".yuv") != string::npos){
        pixformat = SDL_PIXELFORMAT_UYVY;
    }else{
        pixformat = SDL_PIXELFORMAT_IYUV;
    }

    gavf_ctx.sdl2pkt.sdlTexture = SDL_CreateTexture(gavf_ctx.sdl2pkt.sdlRenderer,pixformat, SDL_TEXTUREACCESS_STREAMING,
                                                    gavf_ctx.vDecoderCtx->width, gavf_ctx.vDecoderCtx->height);
    while(1){
        if (gavf_ctx.quit){
            printf("%s quit \n",__FUNCTION__);
            packet_queue_flush(&gavf_ctx.videoq); //清空队列
            break;
        }
        if (packet_queue_get(&gavf_ctx.videoq, packet, 0) <= 0){
            if (gavf_ctx.vreadFinished){//队列里面没有数据了且读取完毕了
                break;
            }else{
                SDL_Delay(10); //队列只是暂时没有数据而已
                continue;
            }
        }

        //收到这个数据 说明刚刚执行过跳转 现在需要把解码器的数据 清除一下
        if(strcmp((char*)packet->data,FLUSH_DATA) == 0){
            avcodec_flush_buffers(gavf_ctx.vDecoderCtx);
            av_packet_unref(packet);
            SDL_Delay(10);
            continue;
        }
        //  av_packet_rescale_ts(packet,gcap_ctx.vStream->time_base,
        //                     gcap_ctx.vDecoderCtx->time_base);
        ret = avcodec_decode_video2(gavf_ctx.vDecoderCtx, pFrame, &got_picture,packet);
        if (ret < 0) {
            printf("decode error.\n");
            av_packet_unref(packet);
            SDL_Delay(10);
            continue;
        }
        if (packet->dts == AV_NOPTS_VALUE && pFrame->opaque&& *(uint64_t*) pFrame->opaque != AV_NOPTS_VALUE){
            video_pts = *(uint64_t *) pFrame->opaque;
        }
        else if (packet->dts != AV_NOPTS_VALUE){
            video_pts = packet->dts;
        }
        else{
            video_pts = 0;
        }
        video_pts *= av_q2d(gavf_ctx.vStream->time_base);
        video_pts = synchronize_video(pFrame, video_pts);
        while(1){
            if (gavf_ctx.quit){
                break;
            }

            if(gavf_ctx.aStreamNb < 0){
                break;
            }

            if (gavf_ctx.avplayerFinshed && gavf_ctx.audioq.size == 0){//读取完了 且音频数据也播放完了 就剩下视频数据了  直接显示出来了 不用同步了
                break;
            }

            audio_pts = gavf_ctx.audio_clock;
            video_pts = gavf_ctx.video_clock;
            if (video_pts <= audio_pts) break;
            int delayTime = (video_pts - audio_pts) * 1000;
            delayTime = delayTime > 5 ? 5:delayTime;
            SDL_Delay(delayTime);
        }
        if (got_picture) {
            //FIX: If window is resize
            SDL_GetWindowSize(gavf_ctx.sdl2pkt.screen,&w,&h);
            gavf_ctx.sdl2pkt.sdlRect.x = 0;
            gavf_ctx.sdl2pkt.sdlRect.y = 0;
            gavf_ctx.sdl2pkt.sdlRect.w = w;
            gavf_ctx.sdl2pkt.sdlRect.h = h;
            if(gavf_ctx.i.find(".yuv") != string::npos){
                SDL_UpdateTexture(gavf_ctx.sdl2pkt.sdlTexture, nullptr,pFrame->data[0], pFrame->linesize[0]);
            }else{
                SDL_UpdateYUVTexture(gavf_ctx.sdl2pkt.sdlTexture,
                                     nullptr,
                                     pFrame->data[0], pFrame->linesize[0],
                        pFrame->data[1], pFrame->linesize[1],
                        pFrame->data[2], pFrame->linesize[2]);
            }

            SDL_RenderClear(gavf_ctx.sdl2pkt.sdlRenderer );
            SDL_RenderCopy(gavf_ctx.sdl2pkt.sdlRenderer, gavf_ctx.sdl2pkt.sdlTexture, nullptr, &gavf_ctx.sdl2pkt.sdlRect);
            SDL_RenderPresent(gavf_ctx.sdl2pkt.sdlRenderer );
            av_frame_unref(pFrame);
        }
        av_packet_unref(packet);
    }
    av_frame_free(&pFrame);
    return ret;
}


static void RaiseVolume(char* buf, int size, int uRepeat, double vol)//buf为需要调节音量的音频数据块首地址指针，size为长度，uRepeat为重复次数，通常设为1，vol为增益倍数,可以小于1
{
    int i, j;
    if (!size)
    {
        return;
    }
    for ( i = 0; i < size; i += 2)
    {
        short wData;
        wData = MAKEWORD(buf[i], buf[i + 1]);
        long dwData = wData;
        for ( j = 0; j < uRepeat; j++)
        {
            dwData = dwData * vol;
            if (dwData < -0x8000)
            {
                dwData = -0x8000;
            }
            else if (dwData > 0x7FFF)
            {
                dwData = 0x7FFF;
            }
        }
        wData = LOWORD(dwData);
        buf[i] = LOBYTE(wData);
        buf[i + 1] = HIBYTE(wData);
    }
}

static int audio_decode_frame(double *pts_ptr)
{
    int len1, len2, decoded_data_size;
    AVPacket *pkt = &gavf_ctx.audio_pkt;
    AVFrame *audio_frame = nullptr;
    int got_frame = 0;
    int64_t dec_channel_layout;
    int wanted_nb_samples, resampled_data_size, n;
    double pts;
    for (;;) {
        while (gavf_ctx.audio_pkt_size > 0) {
            if (!audio_frame) {
                if (!(audio_frame = av_frame_alloc())) {
                    return AVERROR(ENOMEM);
                }
            } else{
                //  avcodec_get_frame_defaults(gcap_ctx.audio_frame);
                av_frame_unref(audio_frame);
            }
            av_packet_rescale_ts(pkt,gavf_ctx.aStream->time_base,
                                 gavf_ctx.aDecoderCtx->time_base);
            len1 = avcodec_decode_audio4(gavf_ctx.aDecoderCtx, audio_frame,
                                         &got_frame, pkt);
            if (len1 < 0) {
                // error, skip the frame
                gavf_ctx.audio_pkt_size = 0;
                break;
            }

            gavf_ctx.audio_pkt_data += len1;
            gavf_ctx.audio_pkt_size -= len1;
            if (!got_frame){
                SDL_Delay(10);
                continue;
            }
            /* 计算解码出来的桢需要的缓冲大小 */
            decoded_data_size = av_samples_get_buffer_size(nullptr,
                                                           audio_frame->channels,
                                                           audio_frame->nb_samples,
                                                           static_cast<enum AVSampleFormat>(audio_frame->format),
                                                           1);
            dec_channel_layout =
                    (audio_frame->channel_layout
                     && audio_frame->channels
                     == av_get_channel_layout_nb_channels(
                         audio_frame->channel_layout)) ?
                        static_cast<int64_t>(audio_frame->channel_layout) :
                        av_get_default_channel_layout(
                            audio_frame->channels);
            wanted_nb_samples = audio_frame->nb_samples;
            if (audio_frame->format != gavf_ctx.audio_src_fmt
                    || dec_channel_layout != gavf_ctx.audio_src_channel_layout
                    || audio_frame->sample_rate != gavf_ctx.audio_src_freq
                    || (wanted_nb_samples != audio_frame->nb_samples
                        && !gavf_ctx.swr_ctx)) {
                if (gavf_ctx.swr_ctx)
                    swr_free(&gavf_ctx.swr_ctx);
                gavf_ctx.swr_ctx = swr_alloc_set_opts(nullptr,
                                                      gavf_ctx.audio_tgt_channel_layout,
                                                      static_cast<enum AVSampleFormat>(gavf_ctx.audio_tgt_fmt),
                                                      gavf_ctx.audio_tgt_freq, dec_channel_layout,
                                                      static_cast<enum AVSampleFormat>(audio_frame->format),
                                                      audio_frame->sample_rate,
                                                      0, nullptr);
                if (!gavf_ctx.swr_ctx || swr_init(gavf_ctx.swr_ctx) < 0) {
                    fprintf(stderr,"swr_init() failed\n");
                    break;
                }
                gavf_ctx.audio_src_channel_layout = dec_channel_layout;
                gavf_ctx.audio_src_channels = gavf_ctx.aDecoderCtx->channels;
                gavf_ctx.audio_src_freq = gavf_ctx.aDecoderCtx->sample_rate;
                gavf_ctx.audio_src_fmt = gavf_ctx.aDecoderCtx->sample_fmt;
            }

            /* 这里我们可以对采样数进行调整，增加或者减少，一般可以用来做声画同步 */
            if (gavf_ctx.swr_ctx) {
                const uint8_t **in =
                        (const uint8_t **) audio_frame->extended_data;
                uint8_t *out[] = { gavf_ctx.audio_buf2 };
                if (wanted_nb_samples != audio_frame->nb_samples) {
                    if (swr_set_compensation(gavf_ctx.swr_ctx,
                                             (wanted_nb_samples - audio_frame->nb_samples)
                                             * gavf_ctx.audio_tgt_freq
                                             / audio_frame->sample_rate,
                                             wanted_nb_samples * gavf_ctx.audio_tgt_freq
                                             / audio_frame->sample_rate) < 0) {
                        fprintf(stderr,"swr_set_compensation() failed\n");
                        break;
                    }
                }
                len2 = swr_convert(gavf_ctx.swr_ctx, out,
                                   sizeof(gavf_ctx.audio_buf2) / gavf_ctx.audio_tgt_channels
                                   / av_get_bytes_per_sample(gavf_ctx.audio_tgt_fmt),
                                   in, audio_frame->nb_samples);
                if (len2 < 0) {
                    fprintf(stderr,"swr_convert() failed\n");
                    break;
                }
                if (len2 == sizeof(gavf_ctx.audio_buf2) / gavf_ctx.audio_tgt_channels
                        / av_get_bytes_per_sample(gavf_ctx.audio_tgt_fmt)) {
                    fprintf(stderr,"warning: audio buffer is probably too small\n");
                    swr_init(gavf_ctx.swr_ctx);
                }
                gavf_ctx.audio_buf = gavf_ctx.audio_buf2;
                resampled_data_size = len2 * gavf_ctx.audio_tgt_channels
                        * av_get_bytes_per_sample(gavf_ctx.audio_tgt_fmt);
            } else {
                resampled_data_size = decoded_data_size;
                gavf_ctx.audio_buf = audio_frame->data[0];
            }

            pts = gavf_ctx.audio_clock;
            *pts_ptr = pts;
            n = 2 * gavf_ctx.aDecoderCtx->channels;
            gavf_ctx.audio_clock += static_cast<double>(resampled_data_size)
                    / static_cast<double>(n * gavf_ctx.aDecoderCtx->sample_rate);
            // We have data, return it and come back for more later
            //           printf("resampled_data_size:%d\n",resampled_data_size);
            return resampled_data_size;
        }
        if (pkt->data)
            av_packet_unref(pkt);
        memset(pkt, 0, sizeof(*pkt));
        if (gavf_ctx.quit)
        {
            packet_queue_flush(&gavf_ctx.audioq);
            av_frame_free(&audio_frame);
            return -1;
        }
        if (packet_queue_get(&gavf_ctx.audioq, pkt, 0) <= 0)
        {
            //return -1;
            SDL_Delay(10);
            continue;
        }
        //收到这个数据 说明刚刚执行过跳转 现在需要把解码器的数据 清除一下
        if(strcmp((char*)pkt->data,FLUSH_DATA) == 0)
        {
            avcodec_flush_buffers(gavf_ctx.aDecoderCtx);
            av_packet_unref(pkt);
            SDL_Delay(10);
            continue;
        }
        gavf_ctx.audio_pkt_data = pkt->data;
        gavf_ctx.audio_pkt_size = pkt->size;

        /* if update, update the audio clock w/pts */
        if (pkt->pts != AV_NOPTS_VALUE) {
            gavf_ctx.audio_clock = av_q2d(gavf_ctx.aStream->time_base) * pkt->pts;
        }
    }

    cout<<"audio_decode_frame end" << endl;
    return -1;
}

static void audio_callback(void *, uint8_t *stream, int len)
{
    int len1, audio_data_size;
    double pts;
    /*   len是由SDL传入的SDL缓冲区的大小，如果这个缓冲未满，我们就一直往里填充数据 */
    while (len > 0) {
        /*  audio_buf_index 和 audio_buf_size 标示我们自己用来放置解码出来的数据的缓冲区，*/
        /*   这些数据待copy到SDL缓冲区， 当audio_buf_index >= audio_buf_size的时候意味着我*/
        /*   们的缓冲为空，没有数据可供copy，这时候需要调用audio_decode_frame来解码出更多的桢数据 */
        if (gavf_ctx.audio_buf_index >= gavf_ctx.audio_buf_size) {
            audio_data_size = audio_decode_frame(&pts);
            /* audio_data_size < 0 标示没能解码出数据，我们默认播放静音 */
            if (audio_data_size < 0) {
                //                printf("audio_callback: audio_data_size<0\n");
                /* silence */
                gavf_ctx.audio_buf_size = 1024;
                /* 清零，静音 */
                if (gavf_ctx.audio_buf == nullptr) return;
                memset(gavf_ctx.audio_buf, 0, gavf_ctx.audio_buf_size);
            } else {
                gavf_ctx.audio_buf_size = static_cast<uint>(audio_data_size);
            }
            gavf_ctx.audio_buf_index = 0;
        }
        /*  查看stream可用空间，决定一次copy多少数据，剩下的下次继续copy */
        len1 = static_cast<int>(gavf_ctx.audio_buf_size - gavf_ctx.audio_buf_index);
        if (len1 > len) {
            len1 = len;
        }

        if (gavf_ctx.audio_buf == nullptr) return;
        if (gavf_ctx.isMute){ //静音 或者 是在暂停的时候跳转了
            memset(gavf_ctx.audio_buf + gavf_ctx.audio_buf_index, 0, static_cast<size_t>(len1));
        }else{
            RaiseVolume((char*)(gavf_ctx.audio_buf) + gavf_ctx.audio_buf_index, len1, 1, gavf_ctx.mVolume);
        }

        memcpy(stream,(uint8_t *)(gavf_ctx.audio_buf) + gavf_ctx.audio_buf_index, static_cast<size_t>(len1));
        len -= len1;
        stream += len1;
        gavf_ctx.audio_buf_index += static_cast<uint32_t>(len1);
    }
}

static int openSDL()
{
    SDL_AudioSpec wanted_spec, spec;
    int64_t wanted_channel_layout = 0;
    int wanted_nb_channels = 2;
    int i  = 0;

    if (!wanted_channel_layout||
            wanted_nb_channels != av_get_channel_layout_nb_channels(static_cast<uint64_t>(wanted_channel_layout))) {
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
    wanted_spec.channels = static_cast<uint8_t>(av_get_channel_layout_nb_channels(static_cast<uint64_t>(wanted_channel_layout)));
    wanted_spec.freq = gavf_ctx.sample ? gavf_ctx.sample : gavf_ctx.aDecoderCtx->sample_rate; //gcap_ctx.aDecoderCtx->sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        //fprintf(stderr,"Invalid sample rate or channel count!\n");
        return -1;
    }
    wanted_spec.format = AUDIO_S16SYS; // 具体含义请查看“SDL宏定义”部分
    wanted_spec.silence = 0;            // 0指示静音
    wanted_spec.samples = 1024;  // 自定义SDL缓冲区大小
    wanted_spec.callback = audio_callback;        // 音频解码的关键回调函数
    //wanted_spec.userdata = &gcap_ctx;                    // 传给上面回调函数的外带数据
    int num = SDL_GetNumAudioDevices(0);
    for (i =0;i<num;i++){
        gavf_ctx.sdl2pkt.mAudioID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(i,0), false, &wanted_spec, &spec,0);
        if (gavf_ctx.sdl2pkt.mAudioID > 0){
            break;
        }
    }

    /* 检查实际使用的配置（保存在spec,由SDL_OpenAudio()填充） */
    if (spec.format != wanted_spec.format) {
        printf("SDL advised audio format %#x is not supported!\n",spec.format);
        return -1;
    }
    if (spec.channels != wanted_spec.channels) {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            fprintf(stderr,"SDL advised channel count %d is not supported!\n",spec.channels);
            return -1;
        }
    }
    gavf_ctx.audio_hw_buf_size = static_cast<int>(spec.size);
    /* 把设置好的参数保存到大结构中 */
    gavf_ctx.audio_src_fmt = gavf_ctx.audio_tgt_fmt = AV_SAMPLE_FMT_S16;
    gavf_ctx.audio_src_freq = gavf_ctx.audio_tgt_freq = spec.freq;
    gavf_ctx.audio_src_channel_layout = gavf_ctx.audio_tgt_channel_layout =
            wanted_channel_layout;
    gavf_ctx.audio_src_channels = gavf_ctx.audio_tgt_channels = spec.channels;
    gavf_ctx.audio_buf_size = 0;
    gavf_ctx.audio_buf_index = 0;
    memset(&gavf_ctx.audio_pkt, 0, sizeof(gavf_ctx.audio_pkt));
    return 0;
}

static void closeSDL()
{
    if (gavf_ctx.sdl2pkt.mAudioID > 0){
        SDL_CloseAudioDevice(gavf_ctx.sdl2pkt.mAudioID);
    }

    gavf_ctx.sdl2pkt.mAudioID = 0;
}




static int av_player(void *arg)
{
    int ret = 0;
    AVPacket packet;
    SDL_Thread * video_tid  = nullptr;
    char *filename = static_cast<char *>(arg);
    char video_size[32] = {'\0'};
    char framerate[32] = {'\0'};
    char sample_rate[32] = {'\0'};
    AVDictionary *format_opts = nullptr;
    AVInputFormat *avif = nullptr;
    gavf_ctx.isMute = false;
    gavf_ctx.mVolume = 1;
    gavf_ctx.InputFmtCtx = avformat_alloc_context();
    if(string::npos != string(filename).find(".yuv")){
        avif = av_find_input_format("rawvideo");
        sprintf(video_size,"%dx%d",gavf_ctx.w,gavf_ctx.h);
        sprintf(framerate,"%d",gavf_ctx.fps);
        av_dict_set(&format_opts,"video_size",video_size,0);
        av_dict_set(&format_opts,"pixel_format","uyvy422",0);
        av_dict_set(&format_opts,"framerate",framerate,0);
    }else if(string::npos != string(filename).find(".pcm")){
        avif = av_find_input_format("s16le");
        sprintf(sample_rate,"%d",gavf_ctx.sample);
        av_dict_set(&format_opts,"channels","2",0);
        av_dict_set(&format_opts,"sample_rate",sample_rate,0);
    }

    if ((ret = avformat_open_input(&gavf_ctx.InputFmtCtx,filename, avif, &format_opts)) < 0) {
        cout<<"erro:can't open input avfile."<<endl;
        avformat_free_context(gavf_ctx.InputFmtCtx);
        gavf_ctx.avplayerFinshed = 1;
        gavf_ctx.quit = 1;
        return ret;
    }

    if ((ret = avformat_find_stream_info(gavf_ctx.InputFmtCtx, nullptr)) < 0) {
        cout<<"erro:can't find  input  stream information."<<endl;
        avformat_close_input(&gavf_ctx.InputFmtCtx);
        avformat_free_context(gavf_ctx.InputFmtCtx);
        gavf_ctx.avplayerFinshed = 1;
        gavf_ctx.quit = 1;
        return ret;
    }
    av_dump_format(gavf_ctx.InputFmtCtx, 0, filename, 0);

    if((gavf_ctx.vStreamNb = ret = av_find_best_stream(gavf_ctx.InputFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &gavf_ctx.vDecoder, 0)) < 0){
        cout<<"erro:can't find video best stream."<<endl;
        //avformat_close_input(&gavf_ctx.InputFmtCtx);
        //avformat_free_context(gavf_ctx.InputFmtCtx);
        //gavf_ctx.avplayerFinshed = 1;
        //gavf_ctx.quit = 1;
        // return ret;
    }

    if((gavf_ctx.aStreamNb = ret = av_find_best_stream(gavf_ctx.InputFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &gavf_ctx.aDecoder, 0)) < 0){
        cout<<"erro:can't find audio best stream."<<endl;
        // avformat_close_input(&gavf_ctx.InputFmtCtx);
        // avformat_free_context(gavf_ctx.InputFmtCtx);
        // gavf_ctx.avplayerFinshed = 1;
        // gavf_ctx.quit = 1;
        //return ret;
    }

    if((gavf_ctx.vStreamNb < 0)&&(gavf_ctx.aStreamNb < 0)){
        avformat_close_input(&gavf_ctx.InputFmtCtx);
        avformat_free_context(gavf_ctx.InputFmtCtx);
        gavf_ctx.avplayerFinshed = 1;
        gavf_ctx.quit = 1;
        return ret;
    }

    if(gavf_ctx.vStreamNb >= 0){
        if (!(gavf_ctx.vDecoderCtx= avcodec_alloc_context3(gavf_ctx.vDecoder))){
            ret = -1;
            cout<<"erro:alloc video decoder context fail."<<endl;
            avformat_close_input(&gavf_ctx.InputFmtCtx);
            avformat_free_context(gavf_ctx.InputFmtCtx);
            gavf_ctx.avplayerFinshed = 1;
            gavf_ctx.quit = 1;
            return ret;
        }

        gavf_ctx.vStream = gavf_ctx.InputFmtCtx->streams[gavf_ctx.vStreamNb];
        if ((ret = avcodec_parameters_to_context(gavf_ctx.vDecoderCtx,gavf_ctx.vStream->codecpar)) < 0) {
            cout<<"erro:can't copy video decoder parameters to context."<<endl;
            avformat_close_input(&gavf_ctx.InputFmtCtx);
            avformat_free_context(gavf_ctx.InputFmtCtx);
            gavf_ctx.avplayerFinshed = 1;
            gavf_ctx.quit = 1;
            return ret;
        }

        if ((ret = avcodec_open2(gavf_ctx.vDecoderCtx, gavf_ctx.vDecoder, nullptr)) < 0){
            cout<<"erro:fail to open codec for video decoding."<<endl;
            avformat_close_input(&gavf_ctx.InputFmtCtx);
            avformat_free_context(gavf_ctx.InputFmtCtx);
            gavf_ctx.avplayerFinshed = 1;
            gavf_ctx.quit = 1;
            return ret;
        }

        video_tid = SDL_CreateThread(video_thread, "video_thread", nullptr);
        packet_queue_init(&gavf_ctx.videoq);

    }

    if(gavf_ctx.aStreamNb >= 0){
        if (!(gavf_ctx.aDecoderCtx= avcodec_alloc_context3(gavf_ctx.aDecoder))){
            ret = -1;
            cout<<"erro:alloc audio decoder context fail."<<endl;
            avformat_close_input(&gavf_ctx.InputFmtCtx);
            avformat_free_context(gavf_ctx.InputFmtCtx);
            gavf_ctx.avplayerFinshed = 1;
            gavf_ctx.quit = 1;
            return ret;
        }

        gavf_ctx.aStream = gavf_ctx.InputFmtCtx->streams[gavf_ctx.aStreamNb];
        if ((ret = avcodec_parameters_to_context(gavf_ctx.aDecoderCtx,gavf_ctx.aStream->codecpar)) < 0) {
            cout<<"erro:can't copy audio decoder parameters to context."<<endl;
            avformat_close_input(&gavf_ctx.InputFmtCtx);
            avformat_free_context(gavf_ctx.InputFmtCtx);
            gavf_ctx.avplayerFinshed = 1;
            gavf_ctx.quit = 1;
            return ret;
        }

        if ((ret = avcodec_open2(gavf_ctx.aDecoderCtx, gavf_ctx.aDecoder, nullptr)) < 0){
            cout<<"erro:fail to open codec for audio decoding."<<endl;
            avformat_close_input(&gavf_ctx.InputFmtCtx);
            avformat_free_context(gavf_ctx.InputFmtCtx);
            gavf_ctx.avplayerFinshed = 1;
            gavf_ctx.quit = 1;
            return ret;
        }

        packet_queue_init(&gavf_ctx.audioq);
        if((ret = openSDL())<0){
            avcodec_close(gavf_ctx.aDecoderCtx);
            avcodec_close(gavf_ctx.vDecoderCtx);
            avformat_close_input(&gavf_ctx.InputFmtCtx);
            avformat_free_context(gavf_ctx.InputFmtCtx);
            closeSDL();
            gavf_ctx.avplayerFinshed = 1;
            gavf_ctx.quit = 1;
        }

        SDL_LockAudioDevice(gavf_ctx.sdl2pkt.mAudioID);
        SDL_PauseAudioDevice(gavf_ctx.sdl2pkt.mAudioID,0);
        SDL_UnlockAudioDevice(gavf_ctx.sdl2pkt.mAudioID);
    }

    while (1){
        if (gavf_ctx.quit){
            //停止播放了
            break;
        }

        if(gavf_ctx.vStreamNb >= 0){

            if (gavf_ctx.videoq.size > MAX_VIDEO_SIZE) {
                //  printf("videoq.size > MAX_VIDEO_SIZE\n");
                SDL_Delay(1);
                continue;
            }
        }

        if(gavf_ctx.aStreamNb >= 0){
            if(gavf_ctx.audioq.size > MAX_AUDIO_SIZE){
                // printf("audioq.size > MAX_AUDIO_SIZE\n");
                SDL_Delay(1);
                continue;
            }
        }

        if (av_read_frame(gavf_ctx.InputFmtCtx,&packet) < 0){
            gavf_ctx.avplayerFinshed = 1;
            if (gavf_ctx.quit){
                break; //解码线程也执行完了 可以退出了
            }
            SDL_Delay(10);
            continue;
        }

        if (packet.stream_index == gavf_ctx.vStreamNb){
            packet_queue_put(&gavf_ctx.videoq, &packet);
            //这里我们将数据存入队列 因此不调用 av_free_packet 释放
        }else if (packet.stream_index == gavf_ctx.aStreamNb){
            packet_queue_put(&gavf_ctx.audioq, &packet);
        }else {
            av_packet_unref(&packet);
        }
    }


    if(gavf_ctx.aStreamNb >= 0){
        avcodec_close(gavf_ctx.aDecoderCtx);
        avcodec_free_context(&gavf_ctx.aDecoderCtx);
        SDL_LockAudioDevice(gavf_ctx.sdl2pkt.mAudioID);
        SDL_PauseAudioDevice(gavf_ctx.sdl2pkt.mAudioID,1);
        SDL_UnlockAudioDevice(gavf_ctx.sdl2pkt.mAudioID);
        closeSDL();
        packet_queue_deinit(&gavf_ctx.audioq);
    }

    if(gavf_ctx.vStreamNb >= 0){
        avcodec_close(gavf_ctx.vDecoderCtx);
        avcodec_free_context(&gavf_ctx.vDecoderCtx);
        packet_queue_deinit(&gavf_ctx.videoq);
        SDL_Delay(7);
        SDL_WaitThread(video_tid, nullptr);
    }


    avformat_close_input(&gavf_ctx.InputFmtCtx);
    avformat_free_context(gavf_ctx.InputFmtCtx);
    gavf_ctx.avplayerFinshed = 1;
    gavf_ctx.quit = 1;
    return ret;
}

int AVFile::execute_parameter(Parse_Parameter *pp)
{
    int ret = 0;
    char avname[128] = {'\0'};
    gavf_ctx.i = pp->i.val;
    gavf_ctx.fs = pp->f;
    if(1 == pp->x.en){
        sscanf(pp->x.val.data(),"%d,%d,%d,%d",&gavf_ctx.w,&gavf_ctx.h,&gavf_ctx.fps,&gavf_ctx.vbitrate);
    }

    if(1 == pp->y.en){
        sscanf(pp->y.val.data(),"%d",&gavf_ctx.sample);
    }

    strcpy(avname,gavf_ctx.i.data());
    if(1 == pp->o.en){
        player(avname);
    }else{
        show_information(avname);
    }

    return ret;
}

int AVFile::player(char *avfilename)
{
    int ret = 0;
    SDL_Thread *key_tid = SDL_CreateThread(keydone_thread, "keydone_thread", nullptr);
    SDL_Thread *av_tid = SDL_CreateThread(av_player,"av_player",avfilename);
    while(1 == gavf_ctx.quit){
        if(1 == gavf_ctx.avplayerFinshed){
            break;
        }
        SDL_Delay(1);
    }

    SDL_Delay(5);
    SDL_WaitThread(av_tid, nullptr);
    SDL_WaitThread(key_tid, nullptr);
    return  ret;
}

int AVFile::show_information(char *avfilename)
{
    int ret = 0;
    AVFormatContext *InputFmtCtx = nullptr;
    InputFmtCtx = avformat_alloc_context();
    AVDictionary *format_opts = nullptr;
    AVInputFormat *avif = nullptr;
    char video_size[32] = {'\0'};
    char framerate[32] = {'\0'};
    char sample_rate[32] = {'\0'};
    //  gavf_ctx.InputFmtCtx->interrupt_callback.callback = interrupt_cb;
    //  gavf_ctx.InputFmtCtx->interrupt_callback.opaque = timeout_ptr;
    // if(!av_dict_get(format_opts,"scan_all_pmts",nullptr,AV_DICT_MATCH_CASE)){
    //     av_dict_set(&format_opts,"scan_all_pmts","1",AV_DICT_DONT_OVERWRITE);
    // }

    if(string::npos != string(avfilename).find(".yuv")){
        avif = av_find_input_format("rawvideo");
        sprintf(video_size,"%dx%d",gavf_ctx.w,gavf_ctx.h);
        sprintf(framerate,"%d",gavf_ctx.fps);
        av_dict_set(&format_opts,"video_size",video_size,0);
        av_dict_set(&format_opts,"pixel_format","uyvy422",0);
        av_dict_set(&format_opts,"framerate",framerate,0);
    }else if(string::npos != string(avfilename).find(".pcm")){
        avif = av_find_input_format("s16le");
        sprintf(sample_rate,"%d",gavf_ctx.sample);
        av_dict_set(&format_opts,"channels","2",0);
        av_dict_set(&format_opts,"sample_rate",sample_rate,0);
    }

    if ((ret = avformat_open_input(&InputFmtCtx,avfilename, avif, &format_opts)) < 0) {
        cout<<"erro:can't open input file."<<endl;
        avformat_close_input(&InputFmtCtx);
        avformat_free_context(InputFmtCtx);
        return ret;
    }

    if ((ret = avformat_find_stream_info(InputFmtCtx, nullptr)) < 0) {
        cout<<"erro:can't find  input  stream information."<<endl;
        avformat_close_input(&InputFmtCtx);
        avformat_free_context(InputFmtCtx);
        return ret;
    }
    av_dump_format(InputFmtCtx, 0, avfilename, 0);
    avformat_close_input(&InputFmtCtx);
    avformat_free_context(InputFmtCtx);
    return ret;
}

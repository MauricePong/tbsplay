#include "capture.h"
static Capture_ctx gcap_ctx;


Capture::Capture()
{
    gcap_ctx.audioFinished = 0;
    gcap_ctx.videoFinished = 0;
    gcap_ctx.avencodeFinished = 0;
    gcap_ctx.quit = 0;
    avdevice_register_all();
    avformat_network_init();
}

Capture::~Capture()
{
    gcap_ctx.quit = 1;
    gcap_ctx.audioFinished = 1;
    gcap_ctx.videoFinished = 1;
    gcap_ctx.avencodeFinished = 1;
    SDL_Quit();
}

int Capture::check_parameter(Parse_Parameter *pp)
{
    int ret = 0;
    if((0 == pp->a.en)&&(0 == pp->v.en)&&(0 == pp->o.en)){
        ret = 0;
    }else if((1 == pp->a.en)&&(0 == pp->v.en)){
        if(pp->a.val.find("hw:") == string::npos){
            ret = -1;
        }
    }else if((0 == pp->a.en)&&(1 == pp->v.en)){
        if(pp->v.val.find("/dev/video") == string::npos){
            ret = -1;
        }
    }else {
        if((pp->a.val.find("hw:") == string::npos) || (pp->v.val.find("/dev/video") == string::npos)){
            ret = -1;
        }
    }

    if(1 == pp->i.en){
        ret = -1;
    }
    return ret;
}

static int tbswrite(int fd, uint32_t reg, uint32_t val)
{
    int ret;
    struct v4l2_tbs_data data;
    data.baseaddr = 0;
    data.reg = reg;
    data.value = val;
    ret = ioctl(fd, VIDIOC_TBS_S_CTL, &data);
    if (ret < 0) {
        printf("VIDIOC_TBS_S_CTL failed (%d)\n", ret);
        return ret;
    }
    return 0;
}

static int tbsread(int fd, uint32_t reg, uint32_t *val)
{
    int ret;
    struct v4l2_tbs_data data;
    data.baseaddr = 0;
    data.reg = reg;
    ret = ioctl(fd, VIDIOC_TBS_G_CTL, &data);
    if (ret < 0) {
        printf("VIDIOC_TBS_G_CTL failed (%d)\n", ret);
        return ret;
    }
    *val = data.value;
    return 0;
}


int Capture::check_hardware_id(Parse_Parameter *pp)
{
    int vfd = 0;
    int ret = 0;
    uint32_t chip = 0;
    char camer_device[24] = {'\0'};
    if(0 == pp->v.en){
        strcpy(camer_device,"/dev/video0");
    }else{
        strcpy(camer_device,pp->v.val.data());
    }
    ret = vfd = open(camer_device, O_RDWR|O_NONBLOCK, 0);
    if(vfd < 0){
        printf("failed to open device:%s\n",camer_device);
        return ret;
    }

    tbsread(vfd,32,&chip);
    chip = chip&0x0000ffff;
    if((0x1263 != chip) && (0x1463 != chip)&&(0x2463 != chip)){
        ret = -1;
    }

    close(vfd);
    // ret = 1;
    return ret;
}

static int keydone_thread(void *)
{
    SDL_Event myEvent;
    while(!gcap_ctx.quit){
        SDL_WaitEvent(&myEvent);
        switch (myEvent.type) {
        case SDL_KEYDOWN:
            if(myEvent.key.keysym.sym == SDLK_ESCAPE){
                gcap_ctx.quit = 1;
                break;
            }else if(myEvent.key.keysym.sym == SDLK_F11){
                SDL_SetWindowFullscreen(gcap_ctx.sdl2pkt.screen,static_cast<uint32_t>(gcap_ctx.parse_ctx.fs));
                gcap_ctx.parse_ctx.fs =  gcap_ctx.parse_ctx.fs ? 0:1;
            }
            break;
        case SDL_QUIT:
            gcap_ctx.quit = 1;
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
    auto ptr = static_cast<time_t*>(ctx);
    int ret = time(nullptr) - *ptr >= 2 ? 1: 0;
    if(1 == ret){
        gcap_ctx.quit = 1;
        printf("Access hardware timeout 2 s,then exit..\n");
    }
    return ret;
}


static int audio_decode_frame(double *pts_ptr)
{
    int len1, len2, decoded_data_size;
    AVPacket *pkt = &gcap_ctx.audio_pkt;
    AVFrame *audio_frame = nullptr;
    int got_frame = 0;
    int64_t dec_channel_layout;
    int wanted_nb_samples, resampled_data_size, n;
    double pts;
    for (;;) {
        while (gcap_ctx.audio_pkt_size > 0) {
            if (!audio_frame) {
                if (!(audio_frame = av_frame_alloc())) {
                    return AVERROR(ENOMEM);
                }
            } else{
                //  avcodec_get_frame_defaults(gcap_ctx.audio_frame);
                av_frame_unref(audio_frame);
            }
            av_packet_rescale_ts(pkt,gcap_ctx.aStream->time_base,
                                 gcap_ctx.aDecoderCtx->time_base);
            len1 = avcodec_decode_audio4(gcap_ctx.aDecoderCtx, audio_frame,
                                         &got_frame, pkt);
            if (len1 < 0) {
                // error, skip the frame
                gcap_ctx.audio_pkt_size = 0;
                break;
            }
            gcap_ctx.audio_pkt_data += len1;
            gcap_ctx.audio_pkt_size -= len1;
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
            if (audio_frame->format != gcap_ctx.audio_src_fmt
                    || dec_channel_layout != gcap_ctx.audio_src_channel_layout
                    || audio_frame->sample_rate != gcap_ctx.audio_src_freq
                    || (wanted_nb_samples != audio_frame->nb_samples
                        && !gcap_ctx.swr_ctx)) {
                if (gcap_ctx.swr_ctx)
                    swr_free(&gcap_ctx.swr_ctx);
                gcap_ctx.swr_ctx = swr_alloc_set_opts(nullptr,
                                                      gcap_ctx.audio_tgt_channel_layout,
                                                      static_cast<enum AVSampleFormat>(gcap_ctx.audio_tgt_fmt),
                                                      gcap_ctx.audio_tgt_freq, dec_channel_layout,
                                                      static_cast<enum AVSampleFormat>(audio_frame->format),
                                                      audio_frame->sample_rate,
                                                      0, nullptr);
                if (!gcap_ctx.swr_ctx || swr_init(gcap_ctx.swr_ctx) < 0) {
                    fprintf(stderr,"swr_init() failed\n");
                    break;
                }
                gcap_ctx.audio_src_channel_layout = dec_channel_layout;
                gcap_ctx.audio_src_channels = gcap_ctx.aDecoderCtx->channels;
                gcap_ctx.audio_src_freq = gcap_ctx.aDecoderCtx->sample_rate;
                gcap_ctx.audio_src_fmt = gcap_ctx.aDecoderCtx->sample_fmt;
            }

            /* 这里我们可以对采样数进行调整，增加或者减少，一般可以用来做声画同步 */
            if (gcap_ctx.swr_ctx) {
                const uint8_t **in =
                        (const uint8_t **) audio_frame->extended_data;
                uint8_t *out[] = { gcap_ctx.audio_buf2 };
                if (wanted_nb_samples != audio_frame->nb_samples) {
                    if (swr_set_compensation(gcap_ctx.swr_ctx,
                                             (wanted_nb_samples - audio_frame->nb_samples)
                                             * gcap_ctx.audio_tgt_freq
                                             / audio_frame->sample_rate,
                                             wanted_nb_samples * gcap_ctx.audio_tgt_freq
                                             / audio_frame->sample_rate) < 0) {
                        fprintf(stderr,"swr_set_compensation() failed\n");
                        break;
                    }
                }
                len2 = swr_convert(gcap_ctx.swr_ctx, out,
                                   sizeof(gcap_ctx.audio_buf2) / gcap_ctx.audio_tgt_channels
                                   / av_get_bytes_per_sample(gcap_ctx.audio_tgt_fmt),
                                   in, audio_frame->nb_samples);
                if (len2 < 0) {
                    fprintf(stderr,"swr_convert() failed\n");
                    break;
                }
                if (len2 == sizeof(gcap_ctx.audio_buf2) / gcap_ctx.audio_tgt_channels
                        / av_get_bytes_per_sample(gcap_ctx.audio_tgt_fmt)) {
                    fprintf(stderr,"warning: audio buffer is probably too small\n");
                    swr_init(gcap_ctx.swr_ctx);
                }
                gcap_ctx.audio_buf = gcap_ctx.audio_buf2;
                resampled_data_size = len2 * gcap_ctx.audio_tgt_channels
                        * av_get_bytes_per_sample(gcap_ctx.audio_tgt_fmt);
            } else {
                resampled_data_size = decoded_data_size;
                gcap_ctx.audio_buf = audio_frame->data[0];
            }

            pts = gcap_ctx.audio_clock;
            *pts_ptr = pts;
            n = 2 * gcap_ctx.aDecoderCtx->channels;
            gcap_ctx.audio_clock += static_cast<double>(resampled_data_size)
                    / static_cast<double>(n * gcap_ctx.aDecoderCtx->sample_rate);
            // We have data, return it and come back for more later
            //           printf("resampled_data_size:%d\n",resampled_data_size);
            return resampled_data_size;
        }
        if (pkt->data)
            av_packet_unref(pkt);
        memset(pkt, 0, sizeof(*pkt));
        if (gcap_ctx.quit)
        {
            packet_queue_flush(&gcap_ctx.audioq);
            av_frame_free(&audio_frame);
            return -1;
        }
        if (packet_queue_get(&gcap_ctx.audioq, pkt, 0) <= 0)
        {
            //return -1;
            SDL_Delay(10);
            continue;
        }
        //收到这个数据 说明刚刚执行过跳转 现在需要把解码器的数据 清除一下
        if(strcmp((char*)pkt->data,FLUSH_DATA) == 0)
        {
            avcodec_flush_buffers(gcap_ctx.aDecoderCtx);
            av_packet_unref(pkt);
            SDL_Delay(10);
            continue;
        }
        gcap_ctx.audio_pkt_data = pkt->data;
        gcap_ctx.audio_pkt_size = pkt->size;

        /* if update, update the audio clock w/pts */
        if (pkt->pts != AV_NOPTS_VALUE) {
            gcap_ctx.audio_clock = av_q2d(gcap_ctx.aStream->time_base) * pkt->pts;
        }
    }

    cout<<"audio_decode_frame end" << endl;;
    return -1;
}



static void RaiseVolume(char* buf, int size, int uRepeat, double vol)
//buf为需要调节音量的音频数据块首地址指针，size为长度，uRepeat为重复次数，通常设为1，vol为增益倍数,可以小于1
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

static void audio_callback(void *userdata, uint8_t *stream, int len)
{
    int len1, audio_data_size;
    double pts;
    /*   len是由SDL传入的SDL缓冲区的大小，如果这个缓冲未满，我们就一直往里填充数据 */
    while (len > 0) {
        /*  audio_buf_index 和 audio_buf_size 标示我们自己用来放置解码出来的数据的缓冲区，*/
        /*   这些数据待copy到SDL缓冲区， 当audio_buf_index >= audio_buf_size的时候意味着我*/
        /*   们的缓冲为空，没有数据可供copy，这时候需要调用audio_decode_frame来解码出更多的桢数据 */
        if (gcap_ctx.audio_buf_index >= gcap_ctx.audio_buf_size) {
            audio_data_size = audio_decode_frame(&pts);
            /* audio_data_size < 0 标示没能解码出数据，我们默认播放静音 */
            if (audio_data_size < 0) {
                //                printf("audio_callback: audio_data_size<0\n");
                /* silence */
                gcap_ctx.audio_buf_size = 1024;
                /* 清零，静音 */
                if (gcap_ctx.audio_buf == nullptr) return;
                memset(gcap_ctx.audio_buf, 0, gcap_ctx.audio_buf_size);
            } else {
                gcap_ctx.audio_buf_size = static_cast<uint>(audio_data_size);
            }
            gcap_ctx.audio_buf_index = 0;
        }
        /*  查看stream可用空间，决定一次copy多少数据，剩下的下次继续copy */
        len1 = static_cast<int>(gcap_ctx.audio_buf_size - gcap_ctx.audio_buf_index);
        if (len1 > len) {
            len1 = len;
        }

        if (gcap_ctx.audio_buf == nullptr) return;
        if (gcap_ctx.isMute){ //静音 或者 是在暂停的时候跳转了
            memset(gcap_ctx.audio_buf + gcap_ctx.audio_buf_index, 0, static_cast<size_t>(len1));
        }else{
            RaiseVolume((char*)(gcap_ctx.audio_buf) + gcap_ctx.audio_buf_index, len1, 1, gcap_ctx.mVolume);
        }

        memcpy(stream,(uint8_t *)(gcap_ctx.audio_buf) + gcap_ctx.audio_buf_index, static_cast<size_t>(len1));
        len -= len1;
        stream += len1;
        gcap_ctx.audio_buf_index += static_cast<uint32_t>(len1);
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
    wanted_spec.freq = gcap_ctx.parse_ctx.sample ? gcap_ctx.parse_ctx.sample : gcap_ctx.aDecoderCtx->sample_rate; //gcap_ctx.aDecoderCtx->sample_rate;
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
        gcap_ctx.sdl2pkt.mAudioID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(i,0), false, &wanted_spec, &spec,0);
        if (gcap_ctx.sdl2pkt.mAudioID > 0){
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
    gcap_ctx.audio_hw_buf_size = static_cast<int>(spec.size);
    /* 把设置好的参数保存到大结构中 */
    gcap_ctx.audio_src_fmt = gcap_ctx.audio_tgt_fmt = AV_SAMPLE_FMT_S16;
    gcap_ctx.audio_src_freq = gcap_ctx.audio_tgt_freq = spec.freq;
    gcap_ctx.audio_src_channel_layout = gcap_ctx.audio_tgt_channel_layout =
            wanted_channel_layout;
    gcap_ctx.audio_src_channels = gcap_ctx.audio_tgt_channels = spec.channels;
    gcap_ctx.audio_buf_size = 0;
    gcap_ctx.audio_buf_index = 0;
    memset(&gcap_ctx.audio_pkt, 0, sizeof(gcap_ctx.audio_pkt));
    return 0;
}

static void closeSDL()
{
    if (gcap_ctx.sdl2pkt.mAudioID > 0)
    {
        SDL_CloseAudioDevice(gcap_ctx.sdl2pkt.mAudioID);
    }

    gcap_ctx.sdl2pkt.mAudioID = 0;
}

static int init_audio_filters(const char *filters_descr)
{
    char args[512];
    int ret = 0;
    const AVFilter *abuffersrc  = avfilter_get_by_name("abuffer");
    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    static const enum AVSampleFormat out_sample_fmts[] = { gcap_ctx.aDecoderCtx->sample_fmt, AV_SAMPLE_FMT_NONE };
    static const int64_t out_channel_layouts[] = { static_cast<int64_t>(gcap_ctx.aDecoderCtx->channel_layout), -1 };//AV_CH_LAYOUT_MONO
    static const int out_sample_rates[] = { 44100, -1 };
    const AVFilterLink *outlink;
    AVRational time_base = gcap_ctx.aStream->time_base;

    gcap_ctx.afilterpkt.filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || ! gcap_ctx.afilterpkt.filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer audio source: the decoded frames from the decoder will be inserted here. */
    if (!gcap_ctx.aDecoderCtx->channel_layout)
        gcap_ctx.aDecoderCtx->channel_layout = static_cast<uint64_t>(av_get_default_channel_layout(gcap_ctx.aDecoderCtx->channels));
    snprintf(args, sizeof(args),
             "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%lx",
             time_base.num, time_base.den, gcap_ctx.aDecoderCtx->sample_rate,
             av_get_sample_fmt_name(gcap_ctx.aDecoderCtx->sample_fmt), gcap_ctx.aDecoderCtx->channel_layout);
    ret = avfilter_graph_create_filter(&gcap_ctx.afilterpkt.filter_buffer_ctx, abuffersrc, "in",
                                       args, nullptr, gcap_ctx.afilterpkt.filter_graph);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        goto end;
    }

    /* buffer audio sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&gcap_ctx.afilterpkt.filter_buffersink_ctx, abuffersink, "out",
                                       nullptr, nullptr, gcap_ctx.afilterpkt.filter_graph);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
        goto end;
    }

    ret = av_opt_set_int_list(gcap_ctx.afilterpkt.filter_buffersink_ctx, "sample_fmts", out_sample_fmts, -1,AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot set output sample format\n");
        goto end;
    }

    ret = av_opt_set_int_list(gcap_ctx.afilterpkt.filter_buffersink_ctx, "channel_layouts", out_channel_layouts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot set output channel layout\n");
        goto end;
    }

    ret = av_opt_set_int_list(gcap_ctx.afilterpkt.filter_buffersink_ctx, "sample_rates", out_sample_rates, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot set output sample rate\n");
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = gcap_ctx.afilterpkt.filter_buffer_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = nullptr;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = gcap_ctx.afilterpkt.filter_buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = nullptr;

    if ((ret = avfilter_graph_parse_ptr(gcap_ctx.afilterpkt.filter_graph, filters_descr,
                                        &inputs, &outputs, nullptr)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(gcap_ctx.afilterpkt.filter_graph, nullptr)) < 0)
        goto end;

    /* Print summary of the sink buffer
     * Note: args buffer is reused to store channel layout string */
    outlink = gcap_ctx.afilterpkt.filter_buffersink_ctx->inputs[0];
    av_get_channel_layout_string(args, sizeof(args), -1, outlink->channel_layout);
    av_log(nullptr, AV_LOG_INFO, "Output: srate:%dHz fmt:%s chlayout:%s\n",
           static_cast<int>(outlink->sample_rate),
           static_cast<char*>(av_x_if_null(av_get_sample_fmt_name(AVSampleFormat(outlink->format)), "?")),
           args);

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static int va_filter_filters(void)
{
    HWDevice *dev;
    char tmp[32] = {"vaapi:/dev/dri/renderD128"};
    int erro = hw_device_init_from_string(tmp,&dev);
    //test hwfilter
    if(erro < 0){
        printf("erro:failed to hw_device_init_from_string\n");
        return erro;
    }
    gcap_ctx.hw_device_ctx = av_buffer_ref(dev->device_ref);
    for(uint i = 0;i < gcap_ctx.vfilterpkt.filter_graph->nb_filters;i++){
        gcap_ctx.vfilterpkt.filter_graph->filters[i]->hw_device_ctx = av_buffer_ref(gcap_ctx.hw_device_ctx);
        if(!gcap_ctx.vfilterpkt.filter_graph->filters[i]->hw_device_ctx){
            return -1;
        }
    }
    return erro;
}

static int init_video_filters(const char *filters_descr)
{
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVRational time_base = gcap_ctx.vStream->time_base;
    // AVBufferSrcParameters *par = av_buffersrc_parameters_alloc();
    enum AVPixelFormat pix_fmts[] = { gcap_ctx.vDecoderCtx->pix_fmt, AV_PIX_FMT_NV12,AV_PIX_FMT_UYVY422,AV_PIX_FMT_VAAPI,AV_PIX_FMT_NONE };
    gcap_ctx.vfilterpkt.filter_graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !gcap_ctx.vfilterpkt.filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }


    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             gcap_ctx.vDecoderCtx->width, gcap_ctx.vDecoderCtx->height, gcap_ctx.vDecoderCtx->pix_fmt,
             time_base.num, time_base.den,
             gcap_ctx.vDecoderCtx->sample_aspect_ratio.num, gcap_ctx.vDecoderCtx->sample_aspect_ratio.den);


    ret = avfilter_graph_create_filter(&gcap_ctx.vfilterpkt.filter_buffer_ctx, buffersrc, "in",
                                       args, nullptr, gcap_ctx.vfilterpkt.filter_graph);
    // memset(par,0,sizeof (*par));
    //par->format = AV_PIX_FMT_NONE;
    // gcap_ctx.vfilterpkt.filter_buffer_ctx->hw_device_ctx = av_buffer_ref(gcap_ctx.hw_device_ctx);

    //par->hw_frames_ctx = gcap_ctx.vDecoderCtx->hw_frames_ctx;
    // av_buffersrc_parameters_set(gcap_ctx.vfilterpkt.filter_buffer_ctx,par);

    // av_freep(&par);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&gcap_ctx.vfilterpkt.filter_buffersink_ctx, buffersink, "out",
                                       nullptr, nullptr, gcap_ctx.vfilterpkt.filter_graph);
    // gcap_ctx.filter_buffersink_ctx->hw_device_ctx = av_buffer_ref(gcap_ctx.hw_device_ctx);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }
    // gcap_ctx.vfilterpkt.filter_buffersink_ctx->hw_device_ctx = av_buffer_ref(gcap_ctx.hw_device_ctx);


    ret = av_opt_set_int_list(gcap_ctx.vfilterpkt.filter_buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = gcap_ctx.vfilterpkt.filter_buffer_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = nullptr;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = gcap_ctx.vfilterpkt.filter_buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = nullptr;



    if ((ret = avfilter_graph_parse_ptr(gcap_ctx.vfilterpkt.filter_graph, filters_descr,
                                        &inputs, &outputs, nullptr)) < 0)
        goto end;

    if(gcap_ctx.parse_ctx.o.find("sdl2") == string::npos){
        va_filter_filters();
    }

    if ((ret = avfilter_graph_config(gcap_ctx.vfilterpkt.filter_graph, nullptr)) < 0)
        goto end;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static double synchronize_video(AVFrame *src_frame, double pts) {

    double frame_delay;

    if (pts != 0.0) {
        /* if we have pts, set video clock to it */
        gcap_ctx.video_clock = pts;
    } else {
        /* if we aren't given a pts, set it to the clock */
        pts = gcap_ctx.video_clock;
    }
    /* update the video clock */
    frame_delay = av_q2d(gcap_ctx.vDecoderCtx->time_base);
    /* if we are repeating a frame, adjust clock accordingly */
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    gcap_ctx.video_clock += frame_delay;
    return pts;
}

static AVPixelFormat get_vaapi_format(AVCodecContext *ctx, const AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;
    ctx = nullptr;
    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == AV_PIX_FMT_VAAPI)
            return *p;
    }
    fprintf(stderr, "Unable to decode this file using VA-API.\n");
    return AV_PIX_FMT_NONE;
}



static int video_thread(void *)
{
    int ret = 0;
    AVPacket pkt1, *packet = &pkt1;
    int got_picture;
    double video_pts = 0; //当前视频的pts
    double audio_pts = 0; //音频pts
    char filters_descr[128]= {'\0'};
    int testw = gcap_ctx.parse_ctx.w ? gcap_ctx.parse_ctx.w : gcap_ctx.vDecoderCtx->width;
    int testh = gcap_ctx.parse_ctx.h ? gcap_ctx.parse_ctx.h : gcap_ctx.vDecoderCtx->height;;
    int fps = gcap_ctx.vStream->avg_frame_rate.num/gcap_ctx.vStream->avg_frame_rate.den;
    if((59 == fps)||(29 == fps)){
        ++fps;
    }
    int testfps = gcap_ctx.parse_ctx.fps ?
                gcap_ctx.parse_ctx.fps :
                fps;
    uint32_t testfs  = static_cast<uint32_t>(gcap_ctx.parse_ctx.fs);
    int pix_enc_en = ((gcap_ctx.vDecoderCtx->width != testw)||
                      (gcap_ctx.vDecoderCtx->height != testh)||
                      (fps != testfps)) ? 1:0;
    int w,h = 0;
    ///解码视频相关
    AVFrame *pFrame,*filtersFrame;
    char titile[36] = {'\0'};
    sprintf(titile,"tbsplay,%s,%s",gcap_ctx.parse_ctx.v.data(),gcap_ctx.parse_ctx.a.data());
    pFrame = av_frame_alloc();
    filtersFrame = av_frame_alloc();

    //  sprintf(filters_descr,"format=nv12,hwupload,deinterlace_vaapi=rate=field:auto=1,scale_vaapi=w=%d:h=%d,hwdownload",
    //            pCodecCtx->width, pCodecCtx->height);
    if(gcap_ctx.parse_ctx.k.find("default") != string::npos){
        if(1 == pix_enc_en){
            if((testw != gcap_ctx.vDecoderCtx->width)&&(testh != gcap_ctx.vDecoderCtx->height)&&(testfps != fps)){
                sprintf(filters_descr,"yadif=mode=send_frame:parity=auto:deint=all,scale=w=%d:h=%d,fps=fps=%d",testw,testh,testfps);
            } else if((testw != gcap_ctx.vDecoderCtx->width)&&(testh != gcap_ctx.vDecoderCtx->height)){
                sprintf(filters_descr,"yadif=mode=send_frame:parity=auto:deint=all,scale=w=%d:h=%d",testw,testh);
            } else if(testfps != fps){
                sprintf(filters_descr,"yadif=mode=send_frame:parity=auto:deint=all,fps=fps=%d",testfps);
            }else{
                sprintf(filters_descr,"yadif=mode=send_frame:parity=auto:deint=all");
            }
        }else{
            sprintf(filters_descr,"yadif=mode=send_frame:parity=auto:deint=all");
            //sprintf(filters_descr,"format=nv12,hwupload,deinterlace_vaapi=rate=field:auto=0,hwdownload,format=uyvy422");
            //  sprintf(filters_descr,"format=nv12,hwupload,deinterlace_vaapi=rate=field:auto=0,hwdownload,format=nv12");
        }

        init_video_filters(filters_descr);
    }else{
        if(1 == pix_enc_en){
            if((testw != gcap_ctx.vDecoderCtx->width)&&(testh != gcap_ctx.vDecoderCtx->height)&&(testfps != fps)){
                sprintf(filters_descr,"scale=w=%d:h=%d,fps=fps=%d",testw,testh,testfps);
                init_video_filters(filters_descr);
            } else if((testw != gcap_ctx.vDecoderCtx->width)&&(testh != gcap_ctx.vDecoderCtx->height)){
                sprintf(filters_descr,"scale=w=%d:h=%d",testw,testh);
                init_video_filters(filters_descr);
            } else if(testfps != fps){
                sprintf(filters_descr,"fps=fps=%d",testfps);
                init_video_filters(filters_descr);
            }
        }
    }

    gcap_ctx.sdl2pkt.screen = SDL_CreateWindow(titile, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                               testw, testh,SDL_WINDOW_OPENGL|
                                               SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!gcap_ctx.sdl2pkt.screen) {
        printf("SDL: could not create window - exiting:%s\n",SDL_GetError());
        return -1;
    }
    SDL_SetWindowFullscreen(gcap_ctx.sdl2pkt.screen,testfs);
    gcap_ctx.sdl2pkt.sdlRenderer = SDL_CreateRenderer(gcap_ctx.sdl2pkt.screen, -1, SDL_RENDERER_ACCELERATED
                                                      |SDL_RENDERER_PRESENTVSYNC|SDL_RENDERER_TARGETTEXTURE);

    SDL_SetRenderDrawColor(gcap_ctx.sdl2pkt.sdlRenderer,0,0,0,255);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
    // SDL_SetHint(SDL_HINT_RENDER_VSYNC,"1");
    // SDL_RenderSetLogicalSize(is->sdlRenderer, pCodecCtx->width, pCodecCtx->height);
    //  Uint32 pixformat= SDL_PIXELFORMAT_UYVY;SDL_PIXELFORMAT_NV12,SDL_PIXELFORMAT_YVYU//SDL_PIXELFORMAT_IYUV
    Uint32 pixformat= SDL_PIXELFORMAT_UYVY;
    gcap_ctx.sdl2pkt.sdlTexture = SDL_CreateTexture(gcap_ctx.sdl2pkt.sdlRenderer,pixformat, SDL_TEXTUREACCESS_STREAMING,
                                                    testw, testh);
    while(1){
        if (gcap_ctx.quit){
            printf("%s quit \n",__FUNCTION__);
            packet_queue_flush(&gcap_ctx.videoq); //清空队列
            break;
        }
        if (packet_queue_get(&gcap_ctx.videoq, packet, 0) <= 0){
            if (gcap_ctx.vreadFinished){//队列里面没有数据了且读取完毕了
                break;
            }else{
                SDL_Delay(10); //队列只是暂时没有数据而已
                continue;
            }
        }

        //收到这个数据 说明刚刚执行过跳转 现在需要把解码器的数据 清除一下
        if(strcmp((char*)packet->data,FLUSH_DATA) == 0){
            avcodec_flush_buffers(gcap_ctx.vDecoderCtx);
            av_packet_unref(packet);
            SDL_Delay(10);
            continue;
        }
        //  av_packet_rescale_ts(packet,gcap_ctx.vStream->time_base,
        //                     gcap_ctx.vDecoderCtx->time_base);
        ret = avcodec_decode_video2(gcap_ctx.vDecoderCtx, pFrame, &got_picture,packet);
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
        video_pts *= av_q2d(gcap_ctx.vStream->time_base);
        video_pts = synchronize_video(pFrame, video_pts);
        while(1){
            if (gcap_ctx.quit){
                break;
            }
            if(gcap_ctx.parse_ctx.a.find("hw:") == string::npos){
                break;
            }
            if (gcap_ctx.areadFinished && gcap_ctx.audioq.size == 0){//读取完了 且音频数据也播放完了 就剩下视频数据了  直接显示出来了 不用同步了
                break;
            }

            audio_pts = gcap_ctx.audio_clock;
            //主要是 跳转的时候 我们把video_clock设置成0了
            //因此这里需要更新video_pts
            //否则当从后面跳转到前面的时候 会卡在这里
            video_pts = gcap_ctx.video_clock;
            //qDebug()<<__FUNCTION__<<video_pts<<audio_pts;
            if (video_pts <= audio_pts) break;
            int delayTime = (video_pts - audio_pts) * 1000;
            delayTime = delayTime > 5 ? 5:delayTime;
            SDL_Delay(delayTime);
        }
        if (got_picture) {
            //FIX: If window is resize
            SDL_GetWindowSize(gcap_ctx.sdl2pkt.screen,&w,&h);
            gcap_ctx.sdl2pkt.sdlRect.x = 0;
            gcap_ctx.sdl2pkt.sdlRect.y = 0;
            gcap_ctx.sdl2pkt.sdlRect.w = w;
            gcap_ctx.sdl2pkt.sdlRect.h = h;
            // SDL_UpdateTexture( is->sdlTexture, &is->sdlRect,packet->data, (pCodecCtx->width*2));
            // SDL_UpdateYUVTexture(SDL_Texture * texture,
            //                                                  const SDL_Rect * rect,
            //                                                  const Uint8 *Yplane, int Ypitch,
            //                                                 const Uint8 *Uplane, int Upitch,
            //                                                const Uint8 *Vplane, int Vpitch);
            if((1 == pix_enc_en)||(gcap_ctx.parse_ctx.k.find("default") != string::npos)){
                /* push the decoded frame into the filtergraph */
                if (av_buffersrc_add_frame_flags(gcap_ctx.vfilterpkt.filter_buffer_ctx, pFrame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                    av_log(nullptr, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                    break;
                }

                while (1) {
                    ret = av_buffersink_get_frame(gcap_ctx.vfilterpkt.filter_buffersink_ctx, filtersFrame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
                        av_frame_unref(filtersFrame);
                        break;
                    }
                    if (ret < 0){
                        av_frame_unref(filtersFrame);
                        break;
                    }

                    SDL_UpdateTexture(gcap_ctx.sdl2pkt.sdlTexture, nullptr,filtersFrame->data[0], filtersFrame->linesize[0]);
                    SDL_RenderClear(gcap_ctx.sdl2pkt.sdlRenderer );
                    SDL_RenderCopy(gcap_ctx.sdl2pkt.sdlRenderer, gcap_ctx.sdl2pkt.sdlTexture, nullptr, &gcap_ctx.sdl2pkt.sdlRect);
                    SDL_RenderPresent(gcap_ctx.sdl2pkt.sdlRenderer );
                    av_frame_unref(filtersFrame);
                }
            }else{
                SDL_UpdateTexture(gcap_ctx.sdl2pkt.sdlTexture, nullptr,pFrame->data[0], (pFrame->linesize[0]));
                SDL_RenderClear(gcap_ctx.sdl2pkt.sdlRenderer );
                SDL_RenderCopy(gcap_ctx.sdl2pkt.sdlRenderer, gcap_ctx.sdl2pkt.sdlTexture, nullptr, &gcap_ctx.sdl2pkt.sdlRect);
                SDL_RenderPresent(gcap_ctx.sdl2pkt.sdlRenderer );
            }
            av_frame_unref(pFrame);
        }
        av_packet_unref(packet);
    }
    avfilter_graph_free(&gcap_ctx.vfilterpkt.filter_graph);
    av_frame_free(&pFrame);
    av_frame_free(&filtersFrame);

    return ret;
}

static void sigterm_handler(int )
{
    gcap_ctx.quit = 1;
}

int Capture::execute_parameter(Parse_Parameter *pp)
{
    int ret = 0;
    Capture_parse_ctx parsectx;
    parsectx.v = pp->v.val;
    parsectx.a = pp->a.val;
    parsectx.fs = pp->f;
    if(1 == pp->k.en){
        if((!pp->k.val.compare("0") || (!pp->k.val.compare("default")))){
            parsectx.k = string("default");
        }else if((!pp->k.val.compare("1") || (!pp->k.val.compare("bob")))){
            parsectx.k = string("bob");
        }else if((!pp->k.val.compare("2") || (!pp->k.val.compare("weave")))){
            parsectx.k = string("weave");
        }else if((!pp->k.val.compare("3") || (!pp->k.val.compare("motion_adaptive")))){
            parsectx.k = string("motion_adaptive");
        }else if((!pp->k.val.compare("4") || (!pp->k.val.compare("motion_compensated")))){
            parsectx.k = string("motion_compensated");
        }else{
            parsectx.k = string("nullptr");
        }
    }else{
        parsectx.k = string("nullptr");
    }
    parsectx.o = pp->o.val;
    if(1 == pp->m.en){
        parsectx.m = pp->m.val;
    }else{
        parsectx.m = string("nullptr");
    }

    if((0 == pp->a.en)&&(0 == pp->v.en)&&(0 == pp->o.en)){
        parsectx.v = string("/dev/video0");
        parsectx.a = string("hw:1,0");
        parsectx.o = string("sdl2");
        pp->o.en = 1;
    }
    if(1 == pp->x.en){
        sscanf(pp->x.val.data(),"%d,%d,%d,%d",&parsectx.w,&parsectx.h,&parsectx.fps,&parsectx.vbitrate);
    }else{
        parsectx.w = 0;
        parsectx.h = 0;
        parsectx.fps = 0;
        parsectx.vbitrate = 0;
    }

    if(1 == pp->y.en){
        sscanf(pp->y.val.data(),"%d",&parsectx.sample);
    }else{
        parsectx.sample = 0;
    }

    if(1 == pp->c.en){
        parsectx.encoder = pp->c.val;
    }else{
        parsectx.encoder = "h264_vaapi";//default
    }

    if(1 == pp->o.en){
        if(parsectx.o.find("sdl2") != string::npos){
            //init SDL windows////////////////////////////
            if(!SDL_getenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE")){
                SDL_setenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE","1",1);
            }

            if(SDL_Init(SDL_INIT_VIDEO| SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
                printf( " initialize SDL - %s\n", SDL_GetError());
            }

            SDL_Thread *key_tid = SDL_CreateThread(keydone_thread, "keydone_thread", nullptr);
            player(parsectx);
            SDL_WaitThread(key_tid, nullptr);
        }else if((parsectx.o.find(".pcm") != string::npos) || (parsectx.o.find(".yuv") != string::npos)){
            signal(SIGINT,sigterm_handler);
            signal(SIGTTIN,sigterm_handler);
            signal(SIGTERM,sigterm_handler);
            if(SDL_Init(SDL_INIT_TIMER)) {
                printf( " initialize SDL - %s\n", SDL_GetError());
            }
            raw_recoder(parsectx);
        }else{
            if(SDL_Init(SDL_INIT_TIMER)) {
                printf( " initialize SDL - %s\n", SDL_GetError());
            }
            signal(SIGINT,sigterm_handler);
            signal(SIGTTIN,sigterm_handler);
            signal(SIGTERM,sigterm_handler);
            enc_recoder_and_push_stream(parsectx);
        }
    }else{
        if(SDL_Init(SDL_INIT_TIMER)) {
            printf( " initialize SDL - %s\n", SDL_GetError());
        }
        signal(SIGINT,sigterm_handler);
        signal(SIGTTIN,sigterm_handler);
        signal(SIGTERM,sigterm_handler);
        show_input_information(parsectx);
    }

    return ret;
}

static int audio_player(void *)
{
    int ret = 0;
    gcap_ctx.isMute = false;
    gcap_ctx.mVolume = 1;
    string filename = gcap_ctx.parse_ctx.a;
    AVInputFormat *aInputFmt = nullptr;
    AVPacket apacket;
    time_t timeout = time(nullptr);
    time_t *timeout_ptr  = &timeout;

    if(nullptr == (aInputFmt = av_find_input_format("alsa"))){
        cout<<"erro:can not find audio input format."<<endl;
        ret = -1;
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    gcap_ctx.aInputFmtCtx = avformat_alloc_context();
    gcap_ctx.aInputFmtCtx->interrupt_callback.callback = interrupt_cb;
    gcap_ctx.aInputFmtCtx->interrupt_callback.opaque =  timeout_ptr;
    if ((ret = avformat_open_input(&gcap_ctx.aInputFmtCtx ,filename.data(), aInputFmt, nullptr)) < 0) {
        cout<<"erro:can't open input audio device."<<endl;
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    /*
    if ((ret = avformat_find_stream_info(gcap_ctx.aInputFmtCtx, nullptr)) < 0) {
        cout<<"erro:can't find audio input  stream information."<<endl;
        avformat_close_input(&gcap_ctx.aInputFmtCtx);
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }
*/
    if((gcap_ctx.aStreamNb = ret = av_find_best_stream(gcap_ctx.aInputFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &gcap_ctx.aDecoder, 0)) < 0){
        cout<<"erro:can't find audio best stream."<<endl;
        avformat_close_input(&gcap_ctx.aInputFmtCtx);
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    if (!(gcap_ctx.aDecoderCtx= avcodec_alloc_context3(gcap_ctx.aDecoder))){
        ret = -1;
        cout<<"erro:alloc audio decoder context fail."<<endl;
        avformat_close_input(&gcap_ctx.aInputFmtCtx);
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    gcap_ctx.aStream = gcap_ctx.aInputFmtCtx->streams[gcap_ctx.aStreamNb];
    gcap_ctx.aStream->discard = AVDISCARD_DEFAULT;
    if ((ret = avcodec_parameters_to_context(gcap_ctx.aDecoderCtx,gcap_ctx.aStream->codecpar)) < 0) {
        cout<<"erro:can't copy audio decoder parameters to context."<<endl;
        avformat_close_input(&gcap_ctx.aInputFmtCtx);
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    if ((ret = avcodec_open2(gcap_ctx.aDecoderCtx, gcap_ctx.aDecoder, nullptr)) < 0){
        cout<<"erro:fail to open codec for audio decoding."<<endl;
        avcodec_close(gcap_ctx.aDecoderCtx);
        avformat_close_input(&gcap_ctx.aInputFmtCtx);
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    // av_dump_format(gcap_ctx.aInputFmtCtx, 0, filename.data(), 0);
    packet_queue_init(&gcap_ctx.audioq);
    if((ret = openSDL())<0){
        avcodec_close(gcap_ctx.aDecoderCtx);
        avformat_close_input(&gcap_ctx.aInputFmtCtx);
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        closeSDL();
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
    }
    SDL_LockAudioDevice(gcap_ctx.sdl2pkt.mAudioID);
    SDL_PauseAudioDevice(gcap_ctx.sdl2pkt.mAudioID,0);
    SDL_UnlockAudioDevice(gcap_ctx.sdl2pkt.mAudioID);

    while (1){
        if (gcap_ctx.quit){
            //停止播放了
            break;
        }
        if (gcap_ctx.audioq.size > MAX_AUDIO_SIZE){
            //printf("audioq.size > MAX_AUDIO_SIZE\n");
            SDL_Delay(5);
            continue;
        }
        if (av_read_frame(gcap_ctx.aInputFmtCtx, &apacket) < 0){
            gcap_ctx.areadFinished = 1;
            if (gcap_ctx.quit){
                break; //解码线程也执行完了 可以退出了
            }
            SDL_Delay(10);
            continue;
        }
        if( apacket.stream_index == gcap_ctx.aStreamNb ){
            packet_queue_put(&gcap_ctx.audioq, &apacket);
            //这里我们将数据存入队列 因此不调用 av_free_packet 释放
        }else{
            // Free the packet that was allocated by av_read_frame
            av_packet_unref(&apacket);
        }
    }
    SDL_LockAudioDevice(gcap_ctx.sdl2pkt.mAudioID);
    SDL_PauseAudioDevice(gcap_ctx.sdl2pkt.mAudioID,1);
    SDL_UnlockAudioDevice(gcap_ctx.sdl2pkt.mAudioID);
    closeSDL();
    avcodec_close(gcap_ctx.aDecoderCtx);
    avcodec_free_context(&gcap_ctx.aDecoderCtx);
    avformat_close_input(&gcap_ctx.aInputFmtCtx);
    avformat_free_context(gcap_ctx.aInputFmtCtx);
    packet_queue_deinit(&gcap_ctx.audioq);
    gcap_ctx.quit = 1;
    gcap_ctx.audioFinished = 1;
    return ret;
}

static int video_player(void *)
{
    int ret = 0;
    AVInputFormat *vInputFmt = nullptr;
    string filename = gcap_ctx.parse_ctx.v;
    time_t timeout = time(nullptr);
    time_t *timeout_ptr  = &timeout;

    if(nullptr == (vInputFmt = av_find_input_format("video4linux2"))){
        cout<<"erro:can not find video input format."<<endl;
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;
    }

    gcap_ctx.vInputFmtCtx = avformat_alloc_context();
    gcap_ctx.vInputFmtCtx->interrupt_callback.callback = interrupt_cb;
    gcap_ctx.vInputFmtCtx->interrupt_callback.opaque = timeout_ptr;
    if ((ret = avformat_open_input(&gcap_ctx.vInputFmtCtx,filename.data(), vInputFmt, nullptr)) < 0) {
        cout<<"erro:can't open input video device."<<endl;
        avformat_close_input(&gcap_ctx.vInputFmtCtx);
        avformat_free_context(gcap_ctx.vInputFmtCtx);
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    if((gcap_ctx.vStreamNb = ret = av_find_best_stream(gcap_ctx.vInputFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &gcap_ctx.vDecoder, 0)) < 0){
        cout<<"erro:can't find video best stream."<<endl;
        avformat_close_input(&gcap_ctx.vInputFmtCtx);
        avformat_free_context(gcap_ctx.vInputFmtCtx);
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    if (!(gcap_ctx.vDecoderCtx= avcodec_alloc_context3(gcap_ctx.vDecoder))){
        ret = -1;
        cout<<"erro:alloc video decoder context fail."<<endl;
        avformat_close_input(&gcap_ctx.vInputFmtCtx);
        avformat_free_context(gcap_ctx.vInputFmtCtx);
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    gcap_ctx.vStream = gcap_ctx.vInputFmtCtx->streams[gcap_ctx.vStreamNb];
    if ((ret = avcodec_parameters_to_context(gcap_ctx.vDecoderCtx,gcap_ctx.vStream->codecpar)) < 0) {
        cout<<"erro:can't copy video decoder parameters to context."<<endl;
        avformat_close_input(&gcap_ctx.vInputFmtCtx);
        avformat_free_context(gcap_ctx.vInputFmtCtx);
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;

        return ret;
    }

    if ((ret = avcodec_open2(gcap_ctx.vDecoderCtx, gcap_ctx.vDecoder, nullptr)) < 0){
        cout<<"erro:fail to open codec for vidoe decoding."<<endl;
        avformat_close_input(&gcap_ctx.vInputFmtCtx);
        avformat_free_context(gcap_ctx.vInputFmtCtx);
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;

        return ret;
    }
    packet_queue_init(&gcap_ctx.videoq);
    SDL_Thread * video_tid = SDL_CreateThread(video_thread, "video_thread", nullptr);
    AVPacket vpacket;
    while (1){
        if (gcap_ctx.quit){
            //停止播放了
            break;
        }
        if (gcap_ctx.videoq.size > MAX_VIDEO_SIZE) {
            //printf("videoq.size > MAX_VIDEO_SIZE\n");
            SDL_Delay(5);
            continue;
        }
        if (av_read_frame(gcap_ctx.vInputFmtCtx,&vpacket) < 0){
            gcap_ctx.vreadFinished = 1;
            if (gcap_ctx.quit){
                break; //解码线程也执行完了 可以退出了
            }
            SDL_Delay(10);
            continue;
        }
        if (vpacket.stream_index == gcap_ctx.vStreamNb){
            packet_queue_put(&gcap_ctx.videoq, &vpacket);
            //这里我们将数据存入队列 因此不调用 av_free_packet 释放
        }
        else{
            av_packet_unref(&vpacket);
        }
    }
    avcodec_close(gcap_ctx.vDecoderCtx);
    avcodec_free_context(&gcap_ctx.vDecoderCtx);
    avformat_close_input(&gcap_ctx.vInputFmtCtx);
    avformat_free_context(gcap_ctx.vInputFmtCtx);
    packet_queue_deinit(&gcap_ctx.videoq);
    SDL_Delay(5);
    SDL_WaitThread(video_tid, nullptr);
    gcap_ctx.videoFinished = 1;
    gcap_ctx.quit = 1;
    return ret;
}

static int rawaudio_recording(void *arg)
{
    int ret = 0;
    char rawabuf[1024] = {'\0'};
    AVInputFormat *aInputFmt = nullptr;
    AVPacket apacket;
    string filename = gcap_ctx.parse_ctx.a;
    time_t timeout = time(nullptr);
    time_t *timeout_ptr  = &timeout;
    strcpy(rawabuf,static_cast<char*>(arg));
    if(nullptr == (aInputFmt = av_find_input_format("alsa"))){
        cout<<"erro:can not find audio input format."<<endl;
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        ret = -1;
        return ret;
    }

    gcap_ctx.aInputFmtCtx = avformat_alloc_context();
    gcap_ctx.aInputFmtCtx->interrupt_callback.callback = interrupt_cb;
    gcap_ctx.aInputFmtCtx->interrupt_callback.opaque = timeout_ptr;
    if ((ret = avformat_open_input(&gcap_ctx.aInputFmtCtx,filename.data(), aInputFmt, nullptr)) < 0) {
        cout<<"erro:can't open input audio device."<<endl;
        avformat_close_input(&gcap_ctx.aInputFmtCtx);
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    Mfile *araw = new Mfile();
    if((ret = araw->mopen(rawabuf,"wb")) < 0){
        avformat_close_input(&gcap_ctx.aInputFmtCtx);
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        delete araw;
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    while (1){
        if (gcap_ctx.quit){
            //停止播放了
            break;
        }

        if (av_read_frame(gcap_ctx.aInputFmtCtx,&apacket) < 0){
            gcap_ctx.areadFinished = 1;
            if (gcap_ctx.quit){
                break; //解码线程也执行完了 可以退出了
            }
            SDL_Delay(10);
            continue;
        }
        araw->dofile(apacket.data,static_cast<size_t>(apacket.size),"write");
        av_packet_unref(&apacket);
    }

    avformat_close_input(&gcap_ctx.aInputFmtCtx);
    avformat_free_context(gcap_ctx.aInputFmtCtx);
    delete araw;
    gcap_ctx.audioFinished = 1;
    gcap_ctx.quit = 1;
    return ret;
}

static int rawvideo_recording(void *arg)
{
    int ret = 0;
    char rawvbuf[1024] = {'\0'};
    AVInputFormat *vInputFmt = nullptr;
    AVPacket vpacket;
    string filename = gcap_ctx.parse_ctx.v;
    time_t timeout = time(nullptr);
    time_t *timeout_ptr  = &timeout;
    strcpy(rawvbuf,static_cast<char*>(arg));
    if(nullptr == (vInputFmt = av_find_input_format("video4linux2"))){
        cout<<"erro:can not find video input format."<<endl;
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;
        ret = -1;
        return ret;
    }

    gcap_ctx.vInputFmtCtx = avformat_alloc_context();
    gcap_ctx.vInputFmtCtx->interrupt_callback.callback = interrupt_cb;
    gcap_ctx.vInputFmtCtx->interrupt_callback.opaque = timeout_ptr;
    if ((ret = avformat_open_input(&gcap_ctx.vInputFmtCtx,filename.data(), vInputFmt, nullptr)) < 0) {
        cout<<"erro:can't open input video device."<<endl;
        avformat_close_input(&gcap_ctx.vInputFmtCtx);
        avformat_free_context(gcap_ctx.vInputFmtCtx);
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    Mfile *vraw = new Mfile();
    if((ret = vraw->mopen(rawvbuf,"wb")) < 0){
        avformat_close_input(&gcap_ctx.vInputFmtCtx);
        avformat_free_context(gcap_ctx.vInputFmtCtx);
        delete vraw;
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    while (1){
        if (gcap_ctx.quit){
            //停止播放了
            break;
        }
        if (av_read_frame(gcap_ctx.vInputFmtCtx,&vpacket) < 0){
            gcap_ctx.vreadFinished = 1;
            if (gcap_ctx.quit){
                break; //解码线程也执行完了 可以退出了
            }
            SDL_Delay(10);
            continue;
        }
        vraw->dofile(vpacket.data,static_cast<size_t>(vpacket.size),"write");
        av_packet_unref(&vpacket);
    }

    avformat_close_input(&gcap_ctx.vInputFmtCtx);
    avformat_free_context(gcap_ctx.vInputFmtCtx);
    delete vraw;
    gcap_ctx.videoFinished = 1;
    gcap_ctx.quit = 1;
    return ret;
}

static int video_pix_enc(void *)
{
    int ret =0;
    int ret1 = 0;
    AVPacket pkt1, *packet = &pkt1;
    char filters_descr[128]= {'\0'};
    int testw = gcap_ctx.parse_ctx.w ? gcap_ctx.parse_ctx.w : gcap_ctx.vDecoderCtx->width;
    int testh = gcap_ctx.parse_ctx.h ? gcap_ctx.parse_ctx.h : gcap_ctx.vDecoderCtx->height;
    int fps = gcap_ctx.vStream->avg_frame_rate.num/gcap_ctx.vStream->avg_frame_rate.den;
    if((29 == fps) ||(59 == fps)){
        ++fps;
    }

    int testfps = gcap_ctx.parse_ctx.fps ?
                gcap_ctx.parse_ctx.fps :
                fps;
    int pix_enc_en = ((gcap_ctx.vDecoderCtx->width != testw)||
                      (gcap_ctx.vDecoderCtx->height != testh)||
                      ((fps) != testfps)) ? 1:0;
    AVFrame *pFrame,*filtersFrame;
    pFrame = av_frame_alloc();
    filtersFrame = av_frame_alloc();

    if(gcap_ctx.parse_ctx.k.find("nullptr") == string::npos){
        if(1 == pix_enc_en){
            if((testw != gcap_ctx.vDecoderCtx->width)&&(testh != gcap_ctx.vDecoderCtx->height)&&(testfps != (fps))){
                sprintf(filters_descr,"format=nv12,fps=fps=%d,hwupload,deinterlace_vaapi=mode=%s:rate=frame:auto=0,scale_vaapi=w=%d:h=%d",
                        testfps,gcap_ctx.parse_ctx.k.data(),testw,testh);
            } else if((testw != gcap_ctx.vDecoderCtx->width)&&(testh != gcap_ctx.vDecoderCtx->height)){
                sprintf(filters_descr,"format=nv12,hwupload,deinterlace_vaapi=mode=%s:rate=frame:auto=0,scale_vaapi=w=%d:h=%d",
                        gcap_ctx.parse_ctx.k.data(),testw,testh);
            } else if(testfps != (fps)){
                sprintf(filters_descr,"format=nv12,fps=fps=%d,hwupload,deinterlace_vaapi=mode=%s:rate=frame:auto=0",testfps,gcap_ctx.parse_ctx.k.data());
            }else{
                sprintf(filters_descr,"format=nv12,hwupload,deinterlace_vaapi=mode=%s:rate=frame:auto=0",gcap_ctx.parse_ctx.k.data());
            }
        }else{
            sprintf(filters_descr,"format=nv12,hwupload,deinterlace_vaapi=mode=%s:rate=frame:auto=0",gcap_ctx.parse_ctx.k.data());
        }
    }else{
        if(1 == pix_enc_en){
            if((testw != gcap_ctx.vDecoderCtx->width)&&(testh != gcap_ctx.vDecoderCtx->height)&&(testfps != (fps))){
                sprintf(filters_descr,"format=nv12,fps=fps=%d,hwupload,scale_vaapi=w=%d:h=%d",testfps,testw,testh);
            } else if((testw != gcap_ctx.vDecoderCtx->width)&&(testh != gcap_ctx.vDecoderCtx->height)){
                sprintf(filters_descr,"format=nv12,hwupload,scale_vaapi=w=%d:h=%d",testw,testh);
            } else if(testfps != (fps)){
                sprintf(filters_descr,"format=nv12,fps=fps=%d,hwupload",testfps);
            } else {
                sprintf(filters_descr,"format=nv12,hwupload");
            }
        }else{
            sprintf(filters_descr,"format=nv12,hwupload");
        }
    }

    init_video_filters(filters_descr);
    gcap_ctx.vdecFinished = 1;
    while(1){
        if (gcap_ctx.quit){
            printf("%s quit \n",__FUNCTION__);
            packet_queue_flush(&gcap_ctx.videoq); //清空队列
            break;
        }
        if (packet_queue_get(&gcap_ctx.videoq, packet, 0) <= 0){
            if (gcap_ctx.vreadFinished){//队列里面没有数据了且读取完毕了
                break;
            }else{
                SDL_Delay(10); //队列只是暂时没有数据而已
                continue;
            }
        }

        // av_packet_rescale_ts(packet,gcap_ctx.vStream->time_base,
        //                      gcap_ctx.vDecoderCtx->time_base);
        if((ret1 = avcodec_send_packet(gcap_ctx.vDecoderCtx,packet)) <0){
            cout<<"error: sending a packet for decoding"<<endl;
            av_packet_unref(packet);
            SDL_Delay(1);
            continue;
        }

        while(ret1 >=0){
            ret1 = avcodec_receive_frame(gcap_ctx.vDecoderCtx,pFrame);
            if(ret1 == AVERROR(EAGAIN) || ret1 == AVERROR_EOF){
                av_packet_unref(packet);
                SDL_Delay(1);
                continue;
            }else if(ret1 < 0){
                cout<<"error: during decoding"<<endl;
                av_packet_unref(packet);
                SDL_Delay(1);
                continue;
            }

            //pFrame->pts = pFrame->best_effort_timestamp; //?cur_pts
            /* push the decoded frame into the filtergraph */
            if (av_buffersrc_add_frame_flags(gcap_ctx.vfilterpkt.filter_buffer_ctx, pFrame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                av_log(nullptr, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                break;
            }

            while (1) {
                ret = av_buffersink_get_frame(gcap_ctx.vfilterpkt.filter_buffersink_ctx, filtersFrame);
                //printf("ret == ===== %d\n",ret);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
                    av_frame_unref(filtersFrame);
                    break;
                }
                if (ret < 0){
                    av_frame_unref(filtersFrame);
                    break;
                }
                //do filterFrame
                FrameQuene_Input(&gcap_ctx.videofq,filtersFrame);
                av_frame_unref(filtersFrame);
            }
            av_frame_unref(pFrame);
        }
        av_packet_unref(packet);
    }
    avfilter_graph_free(&gcap_ctx.vfilterpkt.filter_graph);
    av_frame_free(&pFrame);
    av_frame_free(&filtersFrame);
    return ret;
}


static int video_capture(void *)
{
    int ret = 0;
    AVInputFormat *vInputFmt = nullptr;
    string filename = gcap_ctx.parse_ctx.v;
    AVPacket vpacket;
    time_t timeout = time(nullptr);
    time_t *timeout_ptr  = &timeout;

    if(nullptr == (vInputFmt = av_find_input_format("video4linux2"))){
        cout<<"erro:can not find video input format."<<endl;
        ret = -1;
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    gcap_ctx.vInputFmtCtx = avformat_alloc_context();
    gcap_ctx.vInputFmtCtx->interrupt_callback.callback = interrupt_cb;
    gcap_ctx.vInputFmtCtx->interrupt_callback.opaque = timeout_ptr;

    if ((ret = avformat_open_input(&gcap_ctx.vInputFmtCtx,filename.data(), vInputFmt, nullptr)) < 0) {
        cout<<"erro:can't open input video device."<<endl;
        avformat_close_input(&gcap_ctx.vInputFmtCtx);
        avformat_free_context(gcap_ctx.vInputFmtCtx);
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    if((gcap_ctx.vStreamNb = ret = av_find_best_stream(gcap_ctx.vInputFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &gcap_ctx.vDecoder, 0)) < 0){
        cout<<"erro:can't find video best stream."<<endl;
        avformat_close_input(&gcap_ctx.vInputFmtCtx);
        avformat_free_context(gcap_ctx.vInputFmtCtx);
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    if (!(gcap_ctx.vDecoderCtx= avcodec_alloc_context3(gcap_ctx.vDecoder))){
        ret = -1;
        cout<<"erro:alloc video decoder context fail."<<endl;
        avformat_close_input(&gcap_ctx.vInputFmtCtx);
        avformat_free_context(gcap_ctx.vInputFmtCtx);
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    gcap_ctx.vStream = gcap_ctx.vInputFmtCtx->streams[gcap_ctx.vStreamNb];
    if ((ret = avcodec_parameters_to_context(gcap_ctx.vDecoderCtx,gcap_ctx.vStream->codecpar)) < 0) {
        cout<<"erro:can't copy video decoder parameters to context."<<endl;
        avformat_close_input(&gcap_ctx.vInputFmtCtx);
        avformat_free_context(gcap_ctx.vInputFmtCtx);
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;

        return ret;
    }

    if ((ret = avcodec_open2(gcap_ctx.vDecoderCtx, gcap_ctx.vDecoder, nullptr)) < 0){
        cout<<"erro:fail to open codec for vidoe decoding."<<endl;
        avformat_close_input(&gcap_ctx.vInputFmtCtx);
        avformat_free_context(gcap_ctx.vInputFmtCtx);
        gcap_ctx.videoFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }
    packet_queue_init(&gcap_ctx.videoq);
    frame_queue_init(&gcap_ctx.videofq);
    SDL_Thread * video_tid = SDL_CreateThread(video_pix_enc, "video_pix_enc", nullptr);
    while (1){
        if (gcap_ctx.quit){
            //停止播放了
            break;
        }
        if (gcap_ctx.videoq.size > MAX_VIDEO_SIZE) {
            //printf("videoq.size > MAX_VIDEO_SIZE\n");
            SDL_Delay(10);
            continue;
        }

        if(gcap_ctx.videofq.size > VIDEO_PICTURE_QUEUE_SIZE){
            printf("video frame.size >VIDEO_PICTURE_QUEUE_SIZE\n");
            SDL_Delay(10);
            continue;
        }

        if (av_read_frame(gcap_ctx.vInputFmtCtx,&vpacket) < 0){
            gcap_ctx.vreadFinished = 1;
            if (gcap_ctx.quit){
                break; //解码线程也执行完了 可以退出了
            }
            SDL_Delay(10);
            continue;
        }
        if (vpacket.stream_index == gcap_ctx.vStreamNb){
            packet_queue_put(&gcap_ctx.videoq, &vpacket);
            //这里我们将数据存入队列 因此不调用 av_free_packet 释放
        }
        else{
            av_packet_unref(&vpacket);
        }
    }
    avcodec_free_context(&gcap_ctx.vDecoderCtx);
    avformat_close_input(&gcap_ctx.vInputFmtCtx);
    avformat_free_context(gcap_ctx.vInputFmtCtx);
    packet_queue_deinit(&gcap_ctx.videoq);
    SDL_Delay(5);
    SDL_WaitThread(video_tid, nullptr);
    frame_queue_deinit(&gcap_ctx.videofq);
    av_buffer_unref(&gcap_ctx.hw_device_ctx);
    hw_device_free_all();
    gcap_ctx.videoFinished = 1;
    gcap_ctx.quit = 1;
    return ret;
}

static int audio_pcm_enc(void *)
{
    int ret = 0;
    int ret1 = 0;
    AVFrame *pFrame = av_frame_alloc();
    AVPacket pkt,*packet =&pkt;
    while(1){
        if (gcap_ctx.quit){
            printf("%s quit \n",__FUNCTION__);
            packet_queue_flush(&gcap_ctx.audioq); //清空队列
            break;
        }
        if (packet_queue_get(&gcap_ctx.audioq,packet, 0) <= 0){
            if (gcap_ctx.audioFinished){//队列里面没有数据了且读取完毕了
                break;
            }else{
                SDL_Delay(10); //队列只是暂时没有数据而已
                continue;
            }
        }

        av_packet_rescale_ts(packet,gcap_ctx.aStream->time_base,
                             gcap_ctx.aDecoderCtx->time_base);

        if((ret1 = avcodec_send_packet(gcap_ctx.aDecoderCtx,packet)) <0){
            cout<<"error: sending a packet for decoding"<<endl;
            av_packet_unref(packet);
            SDL_Delay(1);
            continue;
        }

        while(ret1 >=0){
            ret1 = avcodec_receive_frame(gcap_ctx.aDecoderCtx,pFrame);
            if(ret1 == AVERROR(EAGAIN) || ret1 == AVERROR_EOF){
                av_packet_unref(packet);
                SDL_Delay(1);
                continue;
            }else if(ret1 < 0){
                cout<<"error: during decoding"<<endl;
                av_packet_unref(packet);
                SDL_Delay(1);
                continue;
            }
            //pFrame->pts = pFrame->best_effort_timestamp;
            FrameQuene_Input(&gcap_ctx.audiofq,pFrame);
            av_frame_unref(pFrame);
        }
        av_packet_unref(packet);
    }

    av_frame_free(&pFrame);
    return ret;
}

static int audio_capture(void *)
{
    int ret = 0;
    string filename = gcap_ctx.parse_ctx.a;
    AVInputFormat *aInputFmt = nullptr;
    AVPacket apacket;
    time_t timeout = time(nullptr);
    time_t *timeout_ptr  = &timeout;

    if(nullptr == (aInputFmt = av_find_input_format("alsa"))){
        cout<<"erro:can not find audio input format."<<endl;
        ret = -1;
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    gcap_ctx.aInputFmtCtx = avformat_alloc_context();
    gcap_ctx.aInputFmtCtx->interrupt_callback.callback = interrupt_cb;
    gcap_ctx.aInputFmtCtx->interrupt_callback.opaque =  timeout_ptr;
    if ((ret = avformat_open_input(&gcap_ctx.aInputFmtCtx ,filename.data(), aInputFmt, nullptr)) < 0) {
        cout<<"erro:can't open input audio device."<<endl;
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }
    if((gcap_ctx.aStreamNb = ret = av_find_best_stream(gcap_ctx.aInputFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &gcap_ctx.aDecoder, 0)) < 0){
        cout<<"erro:can't find audio best stream."<<endl;
        avformat_close_input(&gcap_ctx.aInputFmtCtx);
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    if (!(gcap_ctx.aDecoderCtx= avcodec_alloc_context3(gcap_ctx.aDecoder))){
        ret = -1;
        cout<<"erro:alloc audio decoder context fail."<<endl;
        avformat_close_input(&gcap_ctx.aInputFmtCtx);
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    gcap_ctx.aStream = gcap_ctx.aInputFmtCtx->streams[gcap_ctx.aStreamNb];
    gcap_ctx.aStream->discard = AVDISCARD_DEFAULT;
    if ((ret = avcodec_parameters_to_context(gcap_ctx.aDecoderCtx,gcap_ctx.aStream->codecpar)) < 0) {
        cout<<"erro:can't copy audio decoder parameters to context."<<endl;
        avformat_close_input(&gcap_ctx.aInputFmtCtx);
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    if ((ret = avcodec_open2(gcap_ctx.aDecoderCtx, gcap_ctx.aDecoder, nullptr)) < 0){
        cout<<"erro:fail to open codec for audio decoding."<<endl;
        avcodec_close(gcap_ctx.aDecoderCtx);
        avformat_close_input(&gcap_ctx.aInputFmtCtx);
        avformat_free_context(gcap_ctx.aInputFmtCtx);
        gcap_ctx.audioFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    gcap_ctx.adecFinished = 1;
    packet_queue_init(&gcap_ctx.audioq);
    frame_queue_init(&gcap_ctx.audiofq);
    SDL_Thread *audio_tid = SDL_CreateThread(audio_pcm_enc, "audio_pcm_enc", nullptr);

    while (1){
        if (gcap_ctx.quit){
            //停止播放了
            break;
        }
        if (gcap_ctx.audioq.size > MAX_AUDIO_SIZE){
            //printf("audioq.size > MAX_AUDIO_SIZE\n");
            SDL_Delay(10);
            continue;
        }
        if (gcap_ctx.audiofq.size > SAMPLE_QUEUE_SIZE){
            printf("audiofq.size > SAMPLE_QUEUE_SIZE\n");
            SDL_Delay(10);
            continue;
        }

        if (av_read_frame(gcap_ctx.aInputFmtCtx, &apacket) < 0){
            gcap_ctx.areadFinished = 1;
            if (gcap_ctx.quit){
                break; //解码线程也执行完了 可以退出了
            }
            SDL_Delay(10);
            continue;
        }
        if( apacket.stream_index == gcap_ctx.aStreamNb ){
            packet_queue_put(&gcap_ctx.audioq, &apacket);
            //这里我们将数据存入队列 因此不调用 av_free_packet 释放
        }else{
            // Free the packet that was allocated by av_read_frame
            av_packet_unref(&apacket);
        }
    }

    avcodec_free_context(&gcap_ctx.aDecoderCtx);
    avformat_close_input(&gcap_ctx.aInputFmtCtx);
    avformat_free_context(gcap_ctx.aInputFmtCtx);
    packet_queue_deinit(&gcap_ctx.audioq);
    SDL_Delay(5);
    SDL_WaitThread(audio_tid, nullptr);
    frame_queue_deinit(&gcap_ctx.audiofq);
    gcap_ctx.quit = 1;
    gcap_ctx.audioFinished = 1;
    return ret;
}

static int v_encode(AVFormatContext* &ofc)
{
    int ret = 0;
    AVBufferRef *hw_frames_ref = nullptr;
    AVHWFramesContext *frames_ctx = nullptr;
    AVDictionary *dictParam = nullptr;
    //hw enc
    if(!(gcap_ctx.vEncoder = avcodec_find_encoder_by_name(gcap_ctx.parse_ctx.encoder.data()))){
        cout<<"erro: could not find video encoder."<<gcap_ctx.parse_ctx.encoder.data()<<endl;
        ret = -1;
        return ret;
    }

    if(!(gcap_ctx.vEncoderCtx = avcodec_alloc_context3(gcap_ctx.vEncoder))){
        cout<<"could not alloc encoder context."<<endl;
        ret = AVERROR(ENOMEM);
        return ret;
    }

    while(1 != gcap_ctx.vdecFinished){
        if(1 == gcap_ctx.quit){
            break;
        }
        SDL_Delay(1);
    }
    //hw encoderctx
    gcap_ctx.vEncoderCtx->width = gcap_ctx.parse_ctx.w ? gcap_ctx.parse_ctx.w : gcap_ctx.vDecoderCtx->width;
    gcap_ctx.vEncoderCtx->height = gcap_ctx.parse_ctx.h ? gcap_ctx.parse_ctx.h : gcap_ctx.vDecoderCtx->height;
    int den = gcap_ctx.vStream->avg_frame_rate.num/gcap_ctx.vStream->avg_frame_rate.den;
    if((29 == den) ||(59 == den)){
        ++den;
    }
    gcap_ctx.vEncoderCtx->time_base = {1,gcap_ctx.parse_ctx.fps ?
                                       gcap_ctx.parse_ctx.fps :
                                       den};
    gcap_ctx.vEncoderCtx->framerate = {gcap_ctx.vEncoderCtx->time_base.den,1};
    gcap_ctx.vEncoderCtx->sample_aspect_ratio = {1,1};
    gcap_ctx.vEncoderCtx->qmin = 0;
    gcap_ctx.vEncoderCtx->qmax = 0;
    gcap_ctx.vEncoderCtx->max_qdiff = 0;
    av_opt_set(gcap_ctx.vEncoderCtx->priv_data,"qp","0",AV_OPT_SEARCH_CHILDREN);
    //gcap_ctx.vDecoderCtx->sample_aspect_ratio;
    //gcap_ctx.vEncoderCtx->bit_rate = gcap_ctx.parse_ctx.vbitrate?gcap_ctx.parse_ctx.vbitrate:gcap_ctx.vDecoderCtx->bit_rate;
    if(gcap_ctx.parse_ctx.vbitrate){
        gcap_ctx.vEncoderCtx->bit_rate = gcap_ctx.parse_ctx.vbitrate;
        gcap_ctx.vEncoderCtx->rc_min_rate = gcap_ctx.parse_ctx.vbitrate;
        gcap_ctx.vEncoderCtx->rc_max_rate = gcap_ctx.parse_ctx.vbitrate;
        gcap_ctx.vEncoderCtx->bit_rate_tolerance = gcap_ctx.parse_ctx.vbitrate;
        gcap_ctx.vEncoderCtx->rc_buffer_size = gcap_ctx.parse_ctx.vbitrate;
        gcap_ctx.vEncoderCtx->rc_initial_buffer_occupancy = gcap_ctx.vEncoderCtx->rc_buffer_size *3/4;

    }
    //gcap_ctx.vEncoderCtx->bit_rate = gcap_ctx.parse_ctx.vbitrate?gcap_ctx.parse_ctx.vbitrate:3000000;
    gcap_ctx.vEncoderCtx->pix_fmt = AV_PIX_FMT_VAAPI;

    /*set hw_frames_ctx for encoder's AVCodecContext*/
    if (!(hw_frames_ref = av_hwframe_ctx_alloc(gcap_ctx.hw_device_ctx))) {
        cout<<"error:Failed to create VAAPI frame context."<<endl;
        avcodec_free_context(&gcap_ctx.vEncoderCtx);
        ret = -1;
        return ret;
    }

    frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
    frames_ctx->format    = AV_PIX_FMT_VAAPI;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width     = gcap_ctx.vEncoderCtx->width;
    frames_ctx->height    = gcap_ctx.vEncoderCtx->height;
    frames_ctx->initial_pool_size =  20;
    if ((ret = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        cout<<"error:failed to initialize VAAPI frame context."<<endl;
        av_buffer_unref(&hw_frames_ref);
        avcodec_free_context(&gcap_ctx.vEncoderCtx);
        return ret;
    }

    gcap_ctx.vEncoderCtx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (! gcap_ctx.vEncoderCtx->hw_frames_ctx){
        ret = AVERROR(ENOMEM);
        av_buffer_unref(&hw_frames_ref);
        avcodec_free_context(&gcap_ctx.vEncoderCtx);
        return ret;
    }

    if (ofc->oformat->flags & AVFMT_GLOBALHEADER){
        gcap_ctx.vEncoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    if(gcap_ctx.parse_ctx.vbitrate){
       // av_dict_set(&dictParam,"tune","zerolatency",0);
        // av_dict_set(&dictParam, "preset", "superfast",0);
        // av_dict_set(&dictParam, "profile", "high",0);
    }

    if((ret = avcodec_open2(gcap_ctx.vEncoderCtx,gcap_ctx.vEncoder,&dictParam))<0){
        cout<<"error:cannot open video encoder codec."<<endl;
        av_buffer_unref(&hw_frames_ref);
        avcodec_free_context(&gcap_ctx.vEncoderCtx);
        return ret;
    }
    return ret;
}


static int a_encode(AVFormatContext* &ofc)
{
    int ret = 0;
    //soft enc
    if(!(gcap_ctx.aEncoder = avcodec_find_encoder_by_name("libfdk_aac"))){
        cout<<"error: could not find audio encoder."<<endl;
        ret = -1;
        return ret;
    }

    if(!(gcap_ctx.aEncoderCtx = avcodec_alloc_context3(gcap_ctx.aEncoder))){
        cout<<"error:could not alloc encoder context."<<endl;
        ret = AVERROR(ENOMEM);
        return ret;
    }

    while(1 != gcap_ctx.adecFinished){
        if(1 == gcap_ctx.quit){
            break;
        }
        SDL_Delay(1);
    }

    gcap_ctx.aEncoderCtx->sample_rate = gcap_ctx.parse_ctx.sample ?
                gcap_ctx.parse_ctx.sample :
                gcap_ctx.aDecoderCtx->sample_rate;
    gcap_ctx.aEncoderCtx->channel_layout = AV_CH_LAYOUT_STEREO;
    //gcap_ctx.aDecoderCtx->channel_layout;
    gcap_ctx.aEncoderCtx->channels = av_get_channel_layout_nb_channels(gcap_ctx.aEncoderCtx->channel_layout);
    /* take first format from list of supported formats */
    gcap_ctx.aEncoderCtx->sample_fmt = gcap_ctx.aEncoder->sample_fmts[0];
    gcap_ctx.aEncoderCtx->time_base.num = 1;
    gcap_ctx.aEncoderCtx->time_base.den = gcap_ctx.aEncoderCtx->sample_rate;
    //add
    gcap_ctx.aEncoderCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    gcap_ctx.aEncoderCtx->bit_rate = 192000;
    if (ofc->oformat->flags & AVFMT_GLOBALHEADER){
        gcap_ctx.aEncoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if((ret = avcodec_open2(gcap_ctx.aEncoderCtx,gcap_ctx.aEncoder,nullptr))< 0){
        cout<<"error:cannot open audio encoder ."<<endl;
        avcodec_free_context(&gcap_ctx.aEncoderCtx);
        return ret;
    }

    return ret;
}

static int video_encode_frame_write(AVFormatContext *ofc, AVStream *vostream)
{
    int ret = 0;
    int ret1 = 0;
    AVPacket enc_pkt;
    AVFrame *hw_frame = nullptr;
    FrameNode *node = FrameQuene_get(&gcap_ctx.videofq);
    if (node == nullptr){
        SDL_Delay(1); //延时1ms
        //  cout<<"no get video frame quene"<<endl;
        return -1;
    }else{
        //if (!(hw_frame = av_frame_alloc())) {
        //    ret  = AVERROR(ENOMEM);
        //    av_frame_free(&node->frame);
        //    free(node);
        //    return ret;
        // }

        //if((ret = av_hwframe_get_buffer(gcap_ctx.vEncoderCtx->hw_frames_ctx,hw_frame,0) < 0)){
        //    av_frame_free(&hw_frame);
        //    av_frame_free(&node->frame);
        //    free(node);
        //    return ret;
        //}

        //if(!(hw_frame->hw_frames_ctx)){
        //    av_frame_free(&hw_frame);
        //    av_frame_free(&node->frame);
        //   free(node);
        //   return ret;
        //}

        //if((ret = av_hwframe_transfer_data(hw_frame,node->frame,0))< 0){
        //    av_frame_free(&hw_frame);
        ///    av_frame_free(&node->frame);
        //    free(node);
        //     return ret;
        // }

        av_init_packet(&enc_pkt);
        enc_pkt.data = nullptr;
        enc_pkt.size = 0;
        enc_pkt.pts = 0;
        node->frame->pts = gcap_ctx.vpts.count;
        if ((ret = avcodec_send_frame(gcap_ctx.vEncoderCtx, node->frame)) < 0) {
            cout<<"error: fail to  vidoe codec send frame."<<endl;
            av_frame_free(&node->frame);
            free(node);
            return ret;
        }

        while (1) {
            ret1 = avcodec_receive_packet(gcap_ctx.vEncoderCtx, &enc_pkt);
            if (ret1){
                break;
            }

            enc_pkt.stream_index = vostream->index;
            av_packet_rescale_ts(&enc_pkt,
                                 gcap_ctx.vEncoderCtx->time_base,
                                 vostream->time_base);

            gcap_ctx.vpts.val = enc_pkt.pts*av_q2d(vostream->time_base);
            ret = av_interleaved_write_frame(ofc, &enc_pkt);
            av_packet_unref(&enc_pkt);
        }
    }

    ++gcap_ctx.vpts.count;
    av_frame_free(&node->frame);
    free(node);
    return ret;
}

static int audio_encode_frame_write(AVFormatContext *ofc, AVStream *aostream)
{
    int ret = 0;
    int ret1 = 0;
    static int64_t  start_pts = 0;
    AVPacket enc_pkt;
    FrameNode *node = FrameQuene_get(&gcap_ctx.audiofq);
    if (node == nullptr){
        SDL_Delay(1); //延时1ms
        // cout<<"no get audio frame quene"<<endl;
        return -1;
    }else{
        av_init_packet(&enc_pkt);
        enc_pkt.data = nullptr;
        enc_pkt.size = 0;
        enc_pkt.pts = 0;
        if ((ret = avcodec_send_frame(gcap_ctx.aEncoderCtx, node->frame)) < 0) {
            cout<<"error: fail to audio codec send frame."<<endl;
            av_frame_free(&node->frame);
            free(node);
            return ret;
        }

        while (ret1 >=0) {
            ret1 = avcodec_receive_packet(gcap_ctx.aEncoderCtx, &enc_pkt);
            if (ret1)
                break;
            enc_pkt.stream_index = aostream->index;

            if(start_pts == 0){
                start_pts = enc_pkt.pts;
            }
            enc_pkt.dts = enc_pkt.pts = enc_pkt.pts - start_pts;
            av_packet_rescale_ts(&enc_pkt,
                                 gcap_ctx.aEncoderCtx->time_base,
                                 aostream->time_base);
            gcap_ctx.apts.val = enc_pkt.pts*av_q2d(aostream->time_base);
            ret = av_interleaved_write_frame(ofc, &enc_pkt);
            av_packet_unref(&enc_pkt);
        }
    }
    av_frame_free(&node->frame);
    free(node);
    return ret;
}

static int av_encode(void *)
{
    int ret = 0;
    char ofmt[16] = "nullptr";
    gcap_ctx.aOutStream = nullptr;
    gcap_ctx.vOutStream = nullptr;
    gcap_ctx.avOutputFmtCtx = nullptr;

    if(gcap_ctx.parse_ctx.m.find("nullptr") != string::npos){
        if(gcap_ctx.parse_ctx.o.find("rtp://") != string::npos){
            strcpy(ofmt,"rtp_mpegts");
        }else if(gcap_ctx.parse_ctx.o.find("udp://") != string::npos) {
            strcpy(ofmt,"mpegts");
        }else if(gcap_ctx.parse_ctx.o.find(".ts") != string::npos) {
            strcpy(ofmt,"mpegts");
        }else if(gcap_ctx.parse_ctx.o.find("rtmp://") != string::npos) {
            strcpy(ofmt,"flv");
        }else if(gcap_ctx.parse_ctx.o.find("rtsp://") != string::npos) {
            strcpy(ofmt,"rtsp");
        }
    }else{
        strcpy(ofmt,gcap_ctx.parse_ctx.m.data());
    }

    avformat_alloc_output_context2(&gcap_ctx.avOutputFmtCtx, nullptr, nullptr, gcap_ctx.parse_ctx.o.data());
    if (!gcap_ctx.avOutputFmtCtx) {
        if(string(ofmt).find("nullptr") == string::npos){
            avformat_alloc_output_context2(&gcap_ctx.avOutputFmtCtx, nullptr, ofmt, gcap_ctx.parse_ctx.o.data());
        }
        if(!gcap_ctx.avOutputFmtCtx){
            cout<<"error:could not create output context"<<endl;
            return AVERROR_UNKNOWN;
        }
    }

    if(gcap_ctx.parse_ctx.v.find("/dev/video") != string::npos){
        if (gcap_ctx.avOutputFmtCtx->oformat->video_codec != AV_CODEC_ID_NONE) {
            ret = v_encode(gcap_ctx.avOutputFmtCtx);
        }else{
            ret = -1;
        }

        if(ret < 0){
            avcodec_close(gcap_ctx.vEncoderCtx);
            avformat_free_context(gcap_ctx.avOutputFmtCtx);
            gcap_ctx.avencodeFinished = 1;
            gcap_ctx.quit = 1;
            return ret;
        }

        gcap_ctx.vOutStream = avformat_new_stream(gcap_ctx.avOutputFmtCtx, nullptr);
        if((ret = avcodec_parameters_from_context(gcap_ctx.vOutStream->codecpar, gcap_ctx.vEncoderCtx))< 0){
            cout <<"error:failed to copy encoder parameters to video output stream."<<endl;
            avcodec_close(gcap_ctx.vEncoderCtx);
            avcodec_free_context(&gcap_ctx.vEncoderCtx);
            avformat_free_context(gcap_ctx.avOutputFmtCtx);

            gcap_ctx.avencodeFinished = 1;
            gcap_ctx.quit = 1;
            return ret;
        }

        gcap_ctx.vOutStream->time_base = gcap_ctx.vEncoderCtx->time_base;
    }

    if(gcap_ctx.parse_ctx.a.find("hw:") != string::npos){
        if (gcap_ctx.avOutputFmtCtx->oformat->audio_codec != AV_CODEC_ID_NONE) {
            ret = a_encode(gcap_ctx.avOutputFmtCtx);
        }else{
            ret = -1;
        }

        if(ret < 0){
            avcodec_close(gcap_ctx.aEncoderCtx);
            avformat_free_context(gcap_ctx.avOutputFmtCtx);
            gcap_ctx.avencodeFinished = 1;
            gcap_ctx.quit = 1;
            return ret;
        }

        gcap_ctx.aOutStream = avformat_new_stream(gcap_ctx.avOutputFmtCtx, nullptr);
        if((ret = avcodec_parameters_from_context(gcap_ctx.aOutStream->codecpar, gcap_ctx.aEncoderCtx))< 0){
            cout <<"error:failed to copy encoder parameters to audio output stream."<<endl;
            if(gcap_ctx.parse_ctx.a.find("hw:") != string::npos){
                avcodec_close(gcap_ctx.aEncoderCtx);
                avcodec_free_context(&gcap_ctx.aEncoderCtx);
            }

            if(gcap_ctx.parse_ctx.v.find("/dev/video") != string::npos){
                avcodec_close(gcap_ctx.vEncoderCtx);
                avcodec_free_context(&gcap_ctx.vEncoderCtx);
            }

            avformat_free_context(gcap_ctx.avOutputFmtCtx);
            gcap_ctx.avencodeFinished = 1;
            gcap_ctx.quit = 1;
            return ret;
        }

        gcap_ctx.aOutStream->time_base = gcap_ctx.aEncoderCtx->time_base;
    }

    av_dump_format(gcap_ctx.avOutputFmtCtx, 0, gcap_ctx.parse_ctx.o.data(), 1);
    if (!(gcap_ctx.avOutputFmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if((ret = avio_open(&gcap_ctx.avOutputFmtCtx->pb, gcap_ctx.parse_ctx.o.data(), AVIO_FLAG_WRITE))<0){
            cout<<"error: could not open output file :"<<gcap_ctx.parse_ctx.o<<endl;
            if(gcap_ctx.parse_ctx.a.find("hw:") != string::npos){
                avcodec_close(gcap_ctx.aEncoderCtx);
                avcodec_free_context(&gcap_ctx.aEncoderCtx);
            }

            if(gcap_ctx.parse_ctx.v.find("/dev/video") != string::npos){
                avcodec_close(gcap_ctx.vEncoderCtx);
                avcodec_free_context(&gcap_ctx.vEncoderCtx);
            }

            avformat_free_context(gcap_ctx.avOutputFmtCtx);
            gcap_ctx.avencodeFinished = 1;
            gcap_ctx.quit = 1;
            return ret;
        }
    }

    /* init muxer, write output file header */
    if((ret = avformat_write_header(gcap_ctx.avOutputFmtCtx, nullptr))< 0){
        cout<<"error: occurred when opening output file."<<endl;
        avio_close(gcap_ctx.avOutputFmtCtx->pb);
        if(gcap_ctx.parse_ctx.a.find("hw:") != string::npos){
            avcodec_close(gcap_ctx.aEncoderCtx);
            avcodec_free_context(&gcap_ctx.aEncoderCtx);
        }

        if(gcap_ctx.parse_ctx.v.find("/dev/video") != string::npos){
            avcodec_close(gcap_ctx.vEncoderCtx);
            avcodec_free_context(&gcap_ctx.vEncoderCtx);
        }

        avformat_free_context(gcap_ctx.avOutputFmtCtx);
        gcap_ctx.avencodeFinished = 1;
        gcap_ctx.quit = 1;
        return ret;
    }

    while(1){
        if (1 == gcap_ctx.quit)
        {
            frame_queue_flush(&gcap_ctx.videofq); //清空队列
            frame_queue_flush(&gcap_ctx.audiofq);
            break;
        }

        /* write interleaved audio and video frames */
        if((nullptr != gcap_ctx.aOutStream)&&(nullptr != gcap_ctx.vOutStream)){
            // av_compare_ts()
            if(gcap_ctx.apts.val < gcap_ctx.vpts.val){
                audio_encode_frame_write(gcap_ctx.avOutputFmtCtx, gcap_ctx.aOutStream);
            }else{
                video_encode_frame_write(gcap_ctx.avOutputFmtCtx, gcap_ctx.vOutStream);
            }
        }else if(nullptr != gcap_ctx.aOutStream){
            audio_encode_frame_write(gcap_ctx.avOutputFmtCtx, gcap_ctx.aOutStream);
        }else if(nullptr != gcap_ctx.vOutStream){
            video_encode_frame_write(gcap_ctx.avOutputFmtCtx, gcap_ctx.vOutStream);
        }

        //printf("audio time:%lf, video time:%lf\n",gcap_ctx.apts.val,gcap_ctx.vpts.val);
        //printf("\r");
    }

    av_write_trailer(gcap_ctx.avOutputFmtCtx);
    avio_close(gcap_ctx.avOutputFmtCtx->pb);
    if(gcap_ctx.parse_ctx.a.find("hw:") != string::npos){
        avcodec_close(gcap_ctx.aEncoderCtx);
        avcodec_free_context(&gcap_ctx.aEncoderCtx);
    }

    if(gcap_ctx.parse_ctx.v.find("/dev/video") != string::npos){
        avcodec_close(gcap_ctx.vEncoderCtx);
        avcodec_free_context(&gcap_ctx.vEncoderCtx);
    }
    avformat_free_context(gcap_ctx.avOutputFmtCtx);
    gcap_ctx.avencodeFinished = 1;
    gcap_ctx.quit = 1;
    return ret;
}

int Capture::player(Capture_parse_ctx &parsectx)
{
    int ret = 0;
    gcap_ctx.parse_ctx = parsectx;
    SDL_Thread *atid = nullptr;
    SDL_Thread *vtid = nullptr;
    if(gcap_ctx.parse_ctx.a.find("hw:") != string::npos){
        atid = SDL_CreateThread(audio_player, "audio_player", nullptr);
    }

    if(gcap_ctx.parse_ctx.v.find("/dev/video") != string::npos){
        vtid = SDL_CreateThread(video_player, "video_player", nullptr);
    }

    if((nullptr != atid)||(nullptr != vtid)){
        while((1 != gcap_ctx.quit)){
            SDL_Delay(1);
        }
        SDL_Delay(7);
    }

    if(nullptr != atid){
        while((1 != gcap_ctx.audioFinished)){
            SDL_Delay(1);
        }
        SDL_WaitThread(atid, nullptr);
    }

    if(nullptr != vtid){
        while((1 != gcap_ctx.videoFinished)){
            SDL_Delay(1);
        }
        SDL_WaitThread(vtid, nullptr);
    }

    return ret;
}

int Capture::raw_recoder(Capture_parse_ctx &parsectx)
{
    int ret = 0;
    gcap_ctx.parse_ctx = parsectx;
    SDL_Thread *araw_tid = nullptr;
    SDL_Thread *vraw_tid = nullptr;
    char pcmbf[1024] = {'\0'};
    char yuvbf[1024] = {'\0'};
    if((gcap_ctx.parse_ctx.o.find(".pcm") != string::npos)&&(gcap_ctx.parse_ctx.o.find(".yuv") != string::npos)){
        sscanf(gcap_ctx.parse_ctx.o.data(),"%s %s",yuvbf,pcmbf);
        if((string(pcmbf).find(".pcm") != string::npos)){
            araw_tid = SDL_CreateThread(rawaudio_recording,"rawaduio_recording",pcmbf);
        }else if((string(yuvbf).find(".pcm") != string::npos)){
            araw_tid = SDL_CreateThread(rawaudio_recording,"rawaduio_recording",yuvbf);
        }

        if((string(pcmbf).find(".yuv") != string::npos)){
            vraw_tid = SDL_CreateThread(rawvideo_recording,"rawvideo_recording",pcmbf);
        }else if((string(yuvbf).find(".yuv") != string::npos)){
            vraw_tid = SDL_CreateThread(rawvideo_recording,"rawvideo_recording",yuvbf);
        }
    }else if(gcap_ctx.parse_ctx.o.find(".pcm") != string::npos){
        sscanf(gcap_ctx.parse_ctx.o.data(),"%s",pcmbf);
        araw_tid = SDL_CreateThread(rawaudio_recording,"rawaduio_recording",pcmbf);
    }else if(gcap_ctx.parse_ctx.o.find(".yuv") != string::npos){
        sscanf(gcap_ctx.parse_ctx.o.data(),"%s",yuvbf);
        vraw_tid = SDL_CreateThread(rawvideo_recording,"rawvideo_recording",yuvbf);
    }

    if((nullptr != araw_tid)||(nullptr != vraw_tid)){
        while((1 != gcap_ctx.quit)){
            SDL_Delay(1);
        }
        SDL_Delay(10);
    }

    if(nullptr != araw_tid){
        while((1 != gcap_ctx.audioFinished)){
            SDL_Delay(1);
        }
        SDL_WaitThread(araw_tid, nullptr);
    }

    if(nullptr != vraw_tid){
        while((1 != gcap_ctx.videoFinished)){
            SDL_Delay(1);
        }
        SDL_WaitThread(vraw_tid, nullptr);
    }

    return ret;

}

int Capture::enc_recoder_and_push_stream(Capture_parse_ctx &parsectx)
{
    int ret = 0;
    gcap_ctx.parse_ctx = parsectx;
    SDL_Thread *vcap_tid = nullptr;
    SDL_Thread *acap_tid = nullptr;
    SDL_Thread *avenc_tid = nullptr;

    if(gcap_ctx.parse_ctx.v.find("/dev/video") != string::npos){
        vcap_tid = SDL_CreateThread(video_capture,"video_capture",nullptr);
    }
    if(gcap_ctx.parse_ctx.a.find("hw:") != string::npos){
        acap_tid = SDL_CreateThread(audio_capture,"audio_capture",nullptr);
    }

    avenc_tid = SDL_CreateThread(av_encode,"av_encode",nullptr);

    if((nullptr != vcap_tid)||(nullptr != acap_tid)){
        while((1 != gcap_ctx.quit)){
            SDL_Delay(1);
        }
        SDL_Delay(10);
    }

    if(nullptr != acap_tid){
        while((1 != gcap_ctx.audioFinished)){
            SDL_Delay(1);
        }
        SDL_WaitThread(acap_tid, nullptr);
    }

    if(nullptr != vcap_tid){
        while((1 != gcap_ctx.videoFinished)){
            SDL_Delay(1);
        }
        SDL_WaitThread(vcap_tid, nullptr);
    }

    if(nullptr != avenc_tid){
        while((1 != gcap_ctx.avencodeFinished)){
            SDL_Delay(1);
        }
        SDL_WaitThread(avenc_tid, nullptr);
    }

    return ret;
}


int Capture::show_input_information(Capture_parse_ctx &parsectx)
{
    int ret = 0;
    gcap_ctx.parse_ctx = parsectx;
    if(gcap_ctx.parse_ctx.a.find("hw:") != string::npos){
        av_show_input_information(gcap_ctx.parse_ctx.a);
    }
    if(gcap_ctx.parse_ctx.v.find("/dev/video") != string::npos){
        av_show_input_information(gcap_ctx.parse_ctx.v);
    }
    return ret;
}

int Capture::av_show_input_information(string &filename)
{
    int ret = 0;
    AVInputFormat *InputFmt = nullptr;
    AVFormatContext *InputFmtCtx = nullptr;
    string input_format;

    if(filename.find("hw:") != string::npos){
        input_format = string("alsa");
    }
    if(filename.find("/dev/video") != string::npos){
        input_format = string("video4linux2");
    }

    if(nullptr == (InputFmt = av_find_input_format(input_format.data()))){
        cout<<"erro:can not find video input format."<<endl;
        ret = -1;
        return ret;
    }

    InputFmtCtx = avformat_alloc_context();
    if ((ret = avformat_open_input(&InputFmtCtx,filename.data(), InputFmt, nullptr)) < 0) {
        cout<<"erro:can't open input device."<<endl;
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
    av_dump_format(InputFmtCtx, 0, filename.data(), 0);
    avformat_close_input(&InputFmtCtx);
    avformat_free_context(InputFmtCtx);


    return ret;
}

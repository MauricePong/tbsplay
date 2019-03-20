#include "head.h"


void DataQuene_Input(BufferQueue *bq,uint8_t * buffer,size_t size)
{
    BufferDataNode * node = static_cast<BufferDataNode*>(malloc(sizeof(BufferDataNode)));
    node->buffer = static_cast<uint8_t *>(malloc(size));
    node->bufferSize = size;
    node->next = nullptr;

    memcpy(node->buffer,buffer,size);
    SDL_LockMutex(bq->Mutex);
    if (bq->DataQueneHead == nullptr){
        bq->DataQueneHead = node;
    }else{
        bq->DataQueneTail->next = node;
    }
    bq->DataQueneTail = node;
    SDL_UnlockMutex(bq->Mutex);
}

BufferDataNode *DataQuene_get(BufferQueue *bq)
{
    BufferDataNode * node = nullptr;
    SDL_LockMutex(bq->Mutex);
    if (bq->DataQueneHead != nullptr){
        node = bq->DataQueneHead;
        if (bq->DataQueneTail == bq->DataQueneHead){
            bq->DataQueneTail = nullptr;
        }
        bq->DataQueneHead = bq->DataQueneHead->next;
    }
    SDL_UnlockMutex(bq->Mutex);
    return node;
}

void buffer_queue_flush(BufferQueue *bq)
{
    BufferDataNode *bnode;
    while(1){
        bnode = DataQuene_get(bq);
        if(nullptr == bnode){
            break;
        }
        free(bnode->buffer);
        free(bnode);
        bnode = nullptr;
    }
}

void buffer_queue_deinit(BufferQueue *bq) {
    buffer_queue_flush(bq);
    SDL_DestroyMutex(bq->Mutex);
}

void buffer_queue_init(BufferQueue *bq) {
    memset(bq, 0, sizeof(BufferQueue));
    bq->Mutex = SDL_CreateMutex();
    bq->DataQueneHead = nullptr;
    bq->DataQueneTail = nullptr;
}

void packet_queue_flush(PacketQueue *q)
{
    AVPacketList *pkt, *pkt1;
    SDL_LockMutex(q->mutex);
    for(pkt = q->first_pkt; pkt != nullptr; pkt = pkt1)
    {
        pkt1 = pkt->next;
        av_packet_unref(&pkt->pkt);
        av_freep(&pkt);

    }
    q->last_pkt = nullptr;
    q->first_pkt = nullptr;
    q->nb_packets = 0;
    q->size = 0;
    SDL_UnlockMutex(q->mutex);
}

void packet_queue_deinit(PacketQueue *q) {
    packet_queue_flush(q);
    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}

void packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
    q->size = 0;
    q->nb_packets = 0;
    q->first_pkt = nullptr;
    q->last_pkt = nullptr;
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {

    AVPacketList *pkt1;
    if (av_dup_packet(pkt) < 0) {
        return -1;
    }
    pkt1 = static_cast<AVPacketList*>(av_malloc(sizeof(AVPacketList)));
    if (!pkt1)
        return -1;
    //if(av_packet_ref(&pkt1->pkt,pkt) < 0){
    //    return -1;
    //}
    pkt1->pkt = *pkt;
    pkt1->next = nullptr;

    SDL_LockMutex(q->mutex);

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += static_cast<size_t>(pkt1->pkt.size);
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
    return 0;
}

int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
    AVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {
        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = nullptr;
            q->nb_packets--;
            q->size -= static_cast<size_t>(pkt1->pkt.size);
            *pkt = pkt1->pkt;
            //f(av_packet_ref(pkt,&pkt1->pkt) < 0){
            //    ret = 0;
            //    break;
            //}
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }

    }

    SDL_UnlockMutex(q->mutex);
    return ret;
}

void FrameQuene_Input(FrameQueue *fq,AVFrame *avframe)
{
    FrameNode * node = static_cast<FrameNode*>(malloc(sizeof(FrameNode)));
    node->frame = av_frame_alloc();
    node->next = nullptr;
    av_frame_ref(node->frame,avframe);
    SDL_LockMutex(fq->Mutex);
    if (fq->FrameQueneHead == nullptr){
        fq->FrameQueneHead = node;
    }else{
        fq->FrameQueneTail->next = node;
    }
    fq->FrameQueneTail = node;
    ++fq->size;
    SDL_UnlockMutex(fq->Mutex);
}

FrameNode *FrameQuene_get(FrameQueue *fq)
{
    FrameNode *node = nullptr;
    SDL_LockMutex(fq->Mutex);
    if (fq->FrameQueneHead != nullptr){
        node = fq->FrameQueneHead;
        if (fq->FrameQueneTail == fq->FrameQueneHead){
            fq->FrameQueneTail = nullptr;
        }
        fq->FrameQueneHead = fq->FrameQueneHead->next;
    }

    if(fq->size > 0){
        --fq->size;
    }
    SDL_UnlockMutex(fq->Mutex);
    return node;
}

void frame_queue_flush(FrameQueue *fq)
{
    FrameNode *node;
    while(1){
        node = FrameQuene_get(fq);
        if(nullptr == node){
            break;
        }
        av_frame_free(&node->frame);
        free(node);
        node = nullptr;
    }
}

void frame_queue_deinit(FrameQueue *fq) {
    frame_queue_flush(fq);
    SDL_DestroyMutex(fq->Mutex);
}

void frame_queue_init(FrameQueue *fq) {
    memset(fq, 0, sizeof(FrameQueue));
    fq->Mutex = SDL_CreateMutex();
    fq->FrameQueneHead = nullptr;
    fq->FrameQueneTail = nullptr;
    fq->size = 0;
}

int is_device(const AVClass *avclass)
{
    if (!avclass)
        return 0;
    return AV_IS_INPUT_DEVICE(avclass->category) || AV_IS_OUTPUT_DEVICE(avclass->category);
}

void enum_muxer_format()
{
    void *ofmt_opaque = nullptr;
    const AVOutputFormat *ofmt = nullptr;
    void *ifmt_opaque = nullptr;
    const AVInputFormat *ifmt  = nullptr;
    int is_dev = 0;
    int encode = 0;
    int decode = 0;
    const char *last_name = "000";
    const char *name      = nullptr;
    const char *long_name = nullptr;
    printf("%s\n"
           " D. = Demuxing supported\n"
           " .E = Muxing supported\n"
           " --\n","File formats:");
    for(;;){
        ofmt_opaque = nullptr;
        name      = nullptr;
        long_name = nullptr;
        encode = 0;
        decode = 0;
        while((ofmt = av_muxer_iterate(&ofmt_opaque))){
            is_dev = is_device(ofmt->priv_class);
            if (is_dev){
                continue;
            }

            if ((!name || strcmp(ofmt->name, name) < 0) &&
                    strcmp(ofmt->name, last_name) > 0) {
                name      = ofmt->name;
                long_name = ofmt->long_name;
                encode  = 1;
            }
        }

        ifmt_opaque = nullptr;
        while ((ifmt = av_demuxer_iterate(&ifmt_opaque))) {
            is_dev = is_device(ifmt->priv_class);
            if (is_dev){
                continue;
            }
            if ((!name || strcmp(ifmt->name, name) < 0) &&
                    strcmp(ifmt->name, last_name) > 0) {
                name      = ifmt->name;
                long_name = ifmt->long_name;
                encode    = 0;
            }
            if (name && strcmp(ifmt->name, name) == 0)
                decode = 1;
        }

        if (!name){
            break;
        }

        last_name = name;
        printf(" %s%s %-15s %s\n",
               decode ? "D" : " ",
               encode ? "E" : " ",
               name,
               long_name ? long_name:" ");
    }
}


static int nb_hw_devices;
static HWDevice **hw_devices;
HWDevice *hw_device_get_by_name(const char *name)
{
    int i;
    for (i = 0; i < nb_hw_devices; i++) {
        if (!strcmp(hw_devices[i]->name, name))
            return hw_devices[i];
    }
    return nullptr;
}

static HWDevice *hw_device_add(void)
{
    int err;
    err = av_reallocp_array(&hw_devices, nb_hw_devices + 1,
                            sizeof(*hw_devices));
    if (err) {
        nb_hw_devices = 0;
        return nullptr;
    }
    hw_devices[nb_hw_devices] = static_cast<HWDevice*>(av_mallocz(sizeof(HWDevice)));
    if (!hw_devices[nb_hw_devices])
        return nullptr;
    return hw_devices[nb_hw_devices++];
}

static char *hw_device_default_name(enum AVHWDeviceType type)
{
    // Make an automatic name of the form "type%d".  We arbitrarily
    // limit at 1000 anonymous devices of the same type - there is
    // probably something else very wrong if you get to this limit.
    const char *type_name = av_hwdevice_get_type_name(type);
    char *name;
    size_t index_pos;
    int index, index_limit = 1000;
    index_pos = strlen(type_name);
    name = static_cast<char *>(av_malloc(index_pos + 4));
    if (!name)
        return nullptr;
    for (index = 0; index < index_limit; index++) {
        snprintf(name, index_pos + 4, "%s%d", type_name, index);
        if (!hw_device_get_by_name(name))
            break;
    }
    if (index >= index_limit) {
        av_freep(&name);
        return nullptr;
    }
    return name;
}

int hw_device_init_from_string(const char *arg, HWDevice **dev_out)
{
    // "type=name:device,key=value,key2=value2"
    // "type:device,key=value,key2=value2"
    // -> av_hwdevice_ctx_create()
    // "type=name@name"
    // "type@name"
    // -> av_hwdevice_ctx_create_derived()

    AVDictionary *options = nullptr;
    char *type_name = nullptr, *name = nullptr, *device = nullptr;
    enum AVHWDeviceType type;
    HWDevice *dev, *src;
    AVBufferRef *device_ref = nullptr;
    int err;
    const char *errmsg, *p, *q;
    size_t k;

    k = strcspn(arg, ":=@");
    p = arg + k;

    type_name = av_strndup(arg, k);
    if (!type_name) {
        err = AVERROR(ENOMEM);
        goto fail;
    }
    type = av_hwdevice_find_type_by_name(type_name);
    if (type == AV_HWDEVICE_TYPE_NONE) {
        errmsg = "unknown device type";
        goto invalid;
    }

    if (*p == '=') {
        k = strcspn(p + 1, ":@");

        name = av_strndup(p + 1, k);
        if (!name) {
            err = AVERROR(ENOMEM);
            goto fail;
        }
        if (hw_device_get_by_name(name)) {
            errmsg = "named device already exists";
            goto invalid;
        }

        p += 1 + k;
    } else {
        name = hw_device_default_name(type);
        if (!name) {
            err = AVERROR(ENOMEM);
            goto fail;
        }
    }

    if (!*p) {
        // New device with no parameters.
        err = av_hwdevice_ctx_create(&device_ref, type,
                                     nullptr, nullptr, 0);
        if (err < 0)
            goto fail;

    } else if (*p == ':') {
        // New device with some parameters.
        ++p;
        q = strchr(p, ',');
        if (q) {
            device = av_strndup(p, q - p);
            if (!device) {
                err = AVERROR(ENOMEM);
                goto fail;
            }
            err = av_dict_parse_string(&options, q + 1, "=", ",", 0);
            if (err < 0) {
                errmsg = "failed to parse options";
                goto invalid;
            }
        }

        err = av_hwdevice_ctx_create(&device_ref, type,
                                     device ? device : p, options, 0);
        if (err < 0)
            goto fail;

    } else if (*p == '@') {
        // Derive from existing device.

        src = hw_device_get_by_name(p + 1);
        if (!src) {
            errmsg = "invalid source device name";
            goto invalid;
        }

        err = av_hwdevice_ctx_create_derived(&device_ref, type,
                                             src->device_ref, 0);
        if (err < 0)
            goto fail;
    } else {
        errmsg = "parse error";
        goto invalid;
    }

    dev = hw_device_add();
    if (!dev) {
        err = AVERROR(ENOMEM);
        goto fail;
    }

    dev->name = name;
    dev->type = type;
    dev->device_ref = device_ref;

    if (dev_out)
        *dev_out = dev;

    name = nullptr;
    err = 0;
done:
    av_freep(&type_name);
    av_freep(&name);
    av_freep(&device);
    av_dict_free(&options);
    return err;
invalid:
    av_log(nullptr, AV_LOG_ERROR,
           "Invalid device specification \"%s\": %s\n", arg, errmsg);
    err = AVERROR(EINVAL);
    goto done;
fail:
    av_log(nullptr, AV_LOG_ERROR,
           "Device creation failed: %d.\n", err);
    av_buffer_unref(&device_ref);
    goto done;
}

void hw_device_free_all(void)
{
    int i;
    for (i = 0; i < nb_hw_devices; i++) {
        av_freep(&hw_devices[i]->name);
        av_buffer_unref(&hw_devices[i]->device_ref);
        av_freep(&hw_devices[i]);
    }
    av_freep(&hw_devices);
    nb_hw_devices = 0;
}

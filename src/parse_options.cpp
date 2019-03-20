#include "parse_options.h"

Parse_options::Parse_options()
{
    pp = new Parse_Parameter();
}

Parse_options::~Parse_options()
{
    delete pp;
}

int Parse_options::parse(int i_argc, char **pp_argv)
{
    int c = 0;
    char videobf[20] = {'\0'};
    char audiobf[20] = {'\0'};
    // if(1 == i_argc){
    //     usage();
    //     return -1;
    // }
    while ( (c = getopt_long(i_argc, pp_argv, "i:c:o:m:Mx:y:fk:ehd", long_options, nullptr)) != -1 )
    {
        switch ( c )
        {
        case 'i':
            if((string(optarg).find("/dev/video") != string::npos)&&
                    (string(optarg).find("hw:") != string::npos)){
                sscanf(optarg,"%s %s",videobf,audiobf);
                if((string(videobf).find("/dev/video") != string::npos)){
                    pp->v.en = 1;
                    pp->v.val = string(videobf);
                }else if((string(audiobf).find("/dev/video") != string::npos)){
                    pp->v.en = 1;
                    pp->v.val = string(audiobf);
                }

                if((string(videobf).find("hw:") != string::npos)){
                    pp->a.en = 1;
                    pp->a.val = string(videobf);
                }else if((string(audiobf).find("hw:") != string::npos)){
                    pp->a.en = 1;
                    pp->a.val = string(audiobf);
                }
            }else if(string(optarg).find("/dev/video") != string::npos){
                sscanf(optarg,"%s",videobf);
                pp->v.en = 1;
                pp->v.val = string(videobf);
            }else if(string(optarg).find("hw:") != string::npos){
                sscanf(optarg,"%s",audiobf);
                pp->a.en = 1;
                pp->a.val = string(audiobf);
            }
            if((0 == pp->v.en) && (0 == pp->a.en)){
                pp->i.en = 1;
                pp->i.val = string(optarg);
            }
            break;
        case 'c':
            pp->c.en = 1;
            pp->c.val = string(optarg);
            break;
        case 'o':
            pp->o.en = 1;
            pp->o.val = string(optarg);
            break;
        case 'm':
            pp->m.en = 1;
            pp->m.val = string(optarg);
            break;
        case 'M':
            pp->M = 1;
            break;
        case 'x':
            pp->x.en = 1;
            pp->x.val = string(optarg);
            break;
        case 'y':
            pp->y.en = 1;
            pp->y.val = string(optarg);
            break;
        case 'f':
            pp->f = 1;
            break;
        case 'k':
            pp->k.en = 1;
            pp->k.val = string(optarg);
            break;
        case 'e':
            pp->e = 1;
            break;
        case 'h':
            pp->h = 1;
            break;
        case 'd':
            pp->d = 1;
            break;
        default:
            usage();
            return -1;
        }
    }
    if(optind < i_argc ){
        usage();
        return -1;
    }
    if(pp->h){
        usage();
        return 0;
    }
    if(pp->d){
        cout<<"the tool's version is  1.0.0.0.1"<<endl;
        return 0;
    }
    if(pp->e){
        enumAVDevice();
        return 0;
    }
    if(pp->M){
        enum_muxer_format();
        return 0;
    }
    return 1;
}

Parse_Parameter* Parse_options::getParseInformation()
{
    return pp;
}

void Parse_options::usage()
{
    printf("\n");
    printf("Usage:\n");
    printf("-i : input the video file path,eg: -i video.ts,eg: -i /dev/video0,eg:-i hw,1,0,eg:-i '/dev/video0 hw:1,0'\n");
    printf("-c : select gpu video encoders(h264_vaapi,hevc_vaapi,mjpeg_vaapi,mpeg2_vaapi,vp8_vaapi,vp9_vaapi,default h264_vaapi)\n");
    printf("-o : output file path,eg: -o video.ts ,eg: -o rtp://192.168.1.88:1234\n,"
           "eg: -o  'video.yuv audio.pcm', eg: -o sdl2\n");

    printf("\n");
    printf("Advanced encode mode setting:\n");
    printf("-x : set video encode parameter <width,height,fps,bitrate>,default <rawdata_width,rawdata_height,rawdata_fps,app_default_bitrate>,"
           "note:bitrate is at least 1000.\n");
    printf("-y : set audio encoder parameter <sample rate>,default <rawdata_sample rate>,eg:-y 44100\n");
    printf("-k : turn on deinterlacing;\n"
           "mode                 Deinterlacing mode (from 0 to 4)\n"
           "default              Use the highest-numbered (and therefore possibly most advanced) deinterlacing algorithm(only playing or encoding).\n"
           "bob                  Use the bob deinterlacing algorithm (only encoding).\n"
           "weave                Use the weave deinterlacing algorithm(only encoding).\n"
           "motion_adaptiv       Use the motion adaptive deinterlacing algorithm(only encoding).\n"
           "motion_compensated   Use the motion compensated deinterlacing algorithm(only encoding).\n");
    printf("-m : encoded muxer format,default auto distingulsh;eg: -m mpegts;eg: -m flv;eg:-m rtsp\n");
    printf("-M : enum Demuxing and Muxing format supported\n");
    printf("\n");

    printf("-f : toggle full screen\n");
    printf("-e : enum audio and video device\n");
    printf("-h : help information\n");
    printf("-d : the tool's version\n");
    printf("While playing:\n"
           "ESC,control+c: quit\n"
           "F11          : toggle full screen\n");

    return ;
}

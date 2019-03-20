#ifndef CAPTURE_H
#define CAPTURE_H
#include "mfile.h"
#include "head.h"

class Capture
{
public:
    Capture();
    ~Capture();
    int check_parameter(Parse_Parameter *pp);
    int check_hardware_id(Parse_Parameter *pp);
    int execute_parameter(Parse_Parameter *pp);
private:
    int player(Capture_parse_ctx &parsectx);
    int raw_recoder(Capture_parse_ctx &parsectx);
    int enc_recoder_and_push_stream(Capture_parse_ctx &parsectx);
    int show_input_information(Capture_parse_ctx &parsectx);
    int av_show_input_information(string &filename);

};

#endif // CAPTURE_H

#ifndef PARSE_OPTIONS_H
#define PARSE_OPTIONS_H
#include "head.h"


class Parse_options
{
public:
    Parse_options();
    ~Parse_options();
    int parse(int i_argc, char **pp_argv);
    Parse_Parameter* getParseInformation();
private:
    Parse_Parameter* pp = {nullptr};
    void usage(void);
    struct option long_options[64] =
    {
    { "input",                  required_argument, nullptr, 'i' },
    { "encode",                 required_argument, nullptr, 'c' },
    { "output",                 required_argument, nullptr, 'o' },
    { "video advanced",         required_argument, nullptr, 'x' },
    { "audio advanced",         required_argument, nullptr, 'y' },
    { "encoded muxers format",  required_argument, nullptr, 'm' },
    { "enum encoded muxers format",   no_argument, nullptr, 'M' },
    { "fullscreen"    ,         no_argument,       nullptr, 'f' },
    { "deinterlacing",          required_argument, nullptr, 'k' },
    { "enum",                   no_argument,       nullptr, 'e' },
    { "help",                   no_argument,       nullptr, 'h' },
    { "version",                no_argument,       nullptr, 'd' },
    { nullptr, 0, nullptr, 0 }};
};

#endif // PARSE_OPTIONS_H

#ifndef AVFILE_H
#define AVFILE_H
#include "head.h"

class AVFile
{
public:
    AVFile();
    ~AVFile();
    int check_parameter(Parse_Parameter *pp);
    int execute_parameter(Parse_Parameter *pp);
private:
    int player(char *avfilename);
    int show_information(char *avfilename);
};

#endif // AVFILE_H

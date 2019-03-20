#include "head.h"
#include "parse_options.h"
#include "capture.h"
#include "avfile.h"
int main(int i_argc, char **pp_argv)
{
    Parse_options* po = new Parse_options();
    if(po->parse(i_argc,pp_argv) < 1){
        delete po;
        return -1;
    }
    Parse_Parameter *pp = po->getParseInformation();
    Capture *cap = new Capture();
    if(cap->check_parameter(pp)<0){
        delete cap;
        cap = nullptr;
        //  cout<<"Invalid parameter input,please view usage help ,'-h'."<<endl;
    }else if(cap->check_hardware_id(pp)<0){
        delete cap;
        cap = nullptr;
        cout<<"The software only supports TBS capture card,please check your hardware."<<endl;
    }else{
        cap->execute_parameter(pp);
        delete cap;
        cap = nullptr;
        delete po;
        printf("\ntbsplayer exit!!!\n");
        return 0;
    }


    AVFile *avf = new AVFile();
    if(avf->check_parameter(pp)<0){
        delete avf;
        avf = nullptr;
        cout<<"Invalid parameter input,please view usage help ,'-h'."<<endl;
    }else{
        avf->execute_parameter(pp);
        delete avf;
        avf = nullptr;
        delete po;
        return 0;
    }

    delete po;
    return 0;
}


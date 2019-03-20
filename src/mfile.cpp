#include "mfile.h"

Mfile::Mfile()
{
    fp = nullptr;
}

Mfile::~Mfile()
{
    if(nullptr != fp){
        fclose(fp);
    }
}

int Mfile::mopen(string filename, string mode)
{
    if ((fp = fopen(filename.data(),mode.data())) == nullptr){
        cout<<"fopen erro"<<endl;
        return -1;
    }
    return 0;
}

size_t Mfile::dofile(void *__ptr, size_t __size, string mode)
{
    if(fp == nullptr){
        cout<<"do file erro,fp is nullptr"<<endl;
        return 0;
    }
    if(!mode.compare("read")){
        return fread(__ptr,__size,1,fp);
    }else if(!mode.compare("write")){
        return fwrite(__ptr,__size,1,fp);
    }else{
        cout <<"non-existent mode "<<endl;
    }
    return 0;
}

#ifndef MFILE_H
#define MFILE_H
#include "head.h"
class Mfile
{
public:
    Mfile();
    ~Mfile();
    int mopen(string filename,string mode);
    size_t dofile(void *__restrict __ptr, size_t __size,string mode);
private:
    FILE *fp;
};

#endif // MFILE_H

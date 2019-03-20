#!/bin/bash
#########################################################################
# File Name: copy_tbsplay_so.sh
# Author: Maurice
# mail: pengzhiwen@turbosight.com
# Created Time: 2019-03-05 17:35:20
#########################################################################
ldd ../bin/tbsplay > lib1.txt
awk '{print $3}' lib1.txt > lib2.txt
cat lib2.txt | while read line

do 
    echo "line->${line}"
    cp $line  ./share/
done

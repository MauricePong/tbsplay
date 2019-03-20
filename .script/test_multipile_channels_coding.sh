#!/bin/bash
#########################################################################
# File Name: test_multipile_channels_coding.sh
# Author: Maurice
# mail: pengzhiwen@turbosight.com
# Created Time: 2019-03-04 18:44:04
#########################################################################

for((i=0;i<$1;i++));
do
    {
        let "j=1+i"
        let "port=1000+i"
        ../bin/tbsplay -i "/dev/video$i hw:$j,0" -c $2 -x 1920,1080,30,$5 $3 $4 -o rtp://192.168.8.81:$port
    }&
        usleep 1;
done
wait


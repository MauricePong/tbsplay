#!/bin/bash
ps -efww|grep "tbsplay" |grep -v grep|cut -c 9-15 |xargs kill -9

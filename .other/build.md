# Centos7.6 

## 1. CentOS 7 upgrade linux-4.19:

 yum -y install perl-devel perl-ExtUtils-CBuilder perl-ExtUtils-MakeMaker ncurses-devel  elfutils-libelf-devel openssl-devel bc git wget

  wget https://cdn.kernel.org/pub/linux/kernel/v4.x/linux-4.19.tar.xz && tar -xvf linux-4.19.tar.xz && cd linux-4.19

  make mrproper

  make menuconfig

  make -j4

  make modules_install

  ls /lib/modules

  make install

  grub2-set-default 0

  reboot

  uname -r

  ls /dev/dri/renderD128 

## 2. CentOS 7 requires an additional extension source:

yum -y install epel-release

  su -c 'yum localinstall --nogpgcheck https://download1.rpmfusion.org/free/el/rpmfusion-free-release-7.noarch.rpm \
  https://download1.rpmfusion.org/nonfree/el/rpmfusion-nonfree-release-7.noarch.rpm'

  rpm --import http://li.nux.ro/download/nux/RPM-GPG-KEY-nux.ro

  rpm -Uvh http://li.nux.ro/download/nux/dextop/el7/x86_64/nux-dextop-release-0-1.el7.nux.noarch.rpm

## 3. Install some dependent libraries:

yum install sdl2-static

yum install sdl2-devel

yum install lame-devel

  yum install libvorbis-devel

  yum install libffi-devel

  yum install libdrm-devel

  yum install expat-devel

  yum install xmlto 

  yum install graphviz 

  yum install cmake 

  yum install automake libtool

<!--//yum install xorg-x11*-->
  yum install xorg-x11-drv-intel-devel

  yum install libpciaccess-devel 

  yum install mesa-dri-drivers

  export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/lib64/:$PKG_CONFIG_PATH

  export LD_LIBRARY_PATH=/home/pzw/pzwtest/:/usr/local/lib/dri/:/usr/local/lib/:/usr/lib64:/usr/local/share/:/usr/local/lib64/:$LD_LIBRARY_PATH

Compile and install wayland

 git clone git://anongit.freedesktop.org/wayland/wayland
cd wayland

./autogen.sh

 make
// make install

### #Compile and install libva-2.4.0

  git clone -b 2.4.0 https://github.com/intel/libva.git

  cd libva

  ./autogen.sh

  ./configure

  make

  make install

### #Compile and install libva-intel-driver-2.2.1

  git clone -b v2.2-branch https://github.com/intel/intel-vaapi-driver.git

  cd libva-intel-driver

  ./autogen.sh

  ./configure

  make

  make install

### #Compile and install x264 x265 vpx fdk-aac

### #Compile and install ffmpeg library

./configure   --disable-shared --enable-static --enable-gpl --enable-nonfree --enable-version3  --enable-asm  --enable-libfdk_aac --enable-libx264 --enable-pthreads  --prefix=/home/pzw/ffmpeg_project/src   --extra-libs="-lrtmp"  --extra-libs=-ldl --enable-libmp3lame  --extra-cflags='-I/usr/local/include -DREDIRECT_DEBUG_LOG' --extra-ldflags='-L/usr/local/lib -gl -lva -lva-drm -lva-x11'   --enable-libvpx  --enable-libx265 --enable-libvorbis --disable-optimizations

#### test:

./ffmpeg -hwaccel vaapi -hwaccel_device /dev/dri/renderD128 -hwaccel_output_format vaapi -i ../../sudo.mkv  -f null -

 ./ffmpeg -vaapi_device /dev/dri/renderD128 -i ../../sudo.mkv  -vf 'format=nv12,hwupload' -c:v h264_vaapi -qp 18 output.mp4

ffmpeg -vaapi_device /dev/dri/renderD128 -i ../../sudo.mkv -vf 'format=nv12,hwupload' -c:v vp9_vaapi -b:v 5M output.webm

ffmpeg -vaapi_device /dev/dri/renderD128 -i ../../sudo.mkv -vf 'format=p010,hwupload' -c:v hevc_vaapi -b:v 15M -profile 2 output.mp4

#### reference:

https://blog.csdn.net/gogoytgo/article/details/49426087

https://blog.csdn.net/zhangwu1241/article/details/52354604/

http://ju.outofmemory.cn/entry/146111

http://trac.ffmpeg.org/wiki/Hardware/VAAPI

https://www.freedesktop.org/wiki/Software/vaapi/

https://developer.nvidia.com/ffmpeg

#### qt projetct:

TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt


TARGET = tbsplay
INCLUDEPATH += .
DESTDIR     = $$PWD/../bin
CONFIG      += qt warn_off
SOURCES += \
    ../src/main.cpp \
    ../src/get_media_devices.c \
    ../src/capture.cpp \
    ../src/avfile.cpp \
    ../src/parse_options.cpp \
    ../src/mfile.cpp \
    ../src/head.cpp

HEADERS += \
    ../src/get_media_devices.h \
    ../src/head.h \
    ../src/capture.h \
    ../src/avfile.h \
    ../src/parse_options.h \
    ../src/mfile.h

FFMPEGVADIR = /usr/local
INCLUDEPATH +=  $$FFMPEGVADIR/include

LIBS += $$FFMPEGVADIR/lib/libavdevice.a
LIBS += $$FFMPEGVADIR/lib/libavfilter.a
LIBS += $$FFMPEGVADIR/lib/libavformat.a
#LIBS += $$FFMPEGVADIR/lib/libavdevice.a
LIBS += $$FFMPEGVADIR/lib/libavcodec.a
#LIBS += $$FFMPEGVADIR/lib/libavfilter.a
LIBS += $$FFMPEGVADIR/lib/libavutil.a
LIBS += $$FFMPEGVADIR/lib/libpostproc.a
LIBS += $$FFMPEGVADIR/lib/libswresample.a
LIBS += $$FFMPEGVADIR/lib/libswscale.a

LIBS  +=/lib64/libSDL2.a
LIBS  +=/lib64/libSDL2_test.a
LIBS  +=/lib64/libSDL2main.a
LIBS  += $$FFMPEGVADIR/lib/libx264.a
LIBS  +=$$FFMPEGVADIR/lib/libfdk-aac.a
LIBS  +=$$FFMPEGVADIR/lib/libvpx.a
LIBS  +=$$FFMPEGVADIR/lib/libx265.a

LIBS  +=$$FFMPEGVADIR/lib/libva.a
LIBS  +=$$FFMPEGVADIR/lib/libva-glx.a
LIBS  +=$$FFMPEGVADIR/lib/libva-drm.a
LIBS  +=$$FFMPEGVADIR/lib/libva-x11.a
LIBS  +=$$FFMPEGVADIR/lib/libwayland-client.a
LIBS  +=$$FFMPEGVADIR/lib/libwayland-cursor.a
LIBS  +=$$FFMPEGVADIR/lib/libwayland-egl.a
LIBS  +=$$FFMPEGVADIR/lib/libwayland-server.a
LIBS  +=$$FFMPEGVADIR/lib/dri/i965_drv_video.a

LIBS += -lm -lxcb -lxcb-shm \
-lxcb-shape -lxcb-xfixes -lbz2 -lz -llzma \
-lpthread -ldl -ldrm -lX11 -lXext -lXfixes  \
-lrt -lXau -lmp3lame -lasound -lvorbis -lvorbisenc



#### Makefile:

#############################################################################

Makefile for building: ../bin/tbsplay_x$(ARCH)

#############################################################################
MAKEFILE      = Makefile

####### Compiler, tools and options

ARCH = $(shell getconf LONG_BIT)
ifeq ($(ARCH), 64)
	DARCH = 64
else
	DARCH = 86
endif
CC            = gcc
CXX           = g++
DEFINES       =
CFLAGS        = -m$(ARCH) -pipe -O2 -Wno-strict-aliasing -W -fPIC $(DEFINES)  -msse -msse2 -msse3 -mmmx -m3dnow -openMP #-wall
CXXFLAGS      = -m$(ARCH) -pipe -O2 -std=gnu++11 -Wall -W -fPIC $(DEFINES) -msse -msse2 -msse3 -mmmx -m3dnow -openMP
INCPATH       = -I. -isystem /usr/local/include -I. -isystem /usr/include/libdrm 
LIBSPATH      = /usr/local/lib
CHK_DIR_EXISTS= test -d
MKDIR         = mkdir -p
COPY          = cp -f
COPY_FILE     = cp -f
COPY_DIR      = cp -f -R
INSTALL_FILE  = install -m 644 -p
INSTALL_PROGRAM = install -m 755 -p
INSTALL_DIR   = cp -f -R
DEL_FILE      = rm -f
SYMLINK       = ln -f -s
DEL_DIR       = rmdir
MOVE          = mv -f
TAR           = tar -cf
COMPRESS      = gzip -9f
DISTNAME      = tbsplay
LINK          = g++
LFLAGS        = -m$(ARCH) -Wl,-O1
LIBS          = $(SUBLIBS)  /usr/local/lib/libavdevice.a /usr/local/lib/libavfilter.a /usr/local/lib/libavformat.a /usr/local/lib/libavcodec.a /usr/local/lib/libavutil.a /usr/local/lib/libpostproc.a /usr/local/lib/libswresample.a /usr/local/lib/libswscale.a /lib64/libSDL2.a /lib64/libSDL2_test.a /lib64/libSDL2main.a /usr/local/lib/libx264.a /usr/local/lib/libfdk-aac.a /usr/local/lib/libvpx.a /usr/local/lib/libx265.a /usr/local/lib/libva.a /usr/local/lib/libva-glx.a /usr/local/lib/libva-drm.a /usr/local/lib/libva-x11.a /usr/local/lib/libwayland-client.a /usr/local/lib/libwayland-cursor.a /usr/local/lib/libwayland-egl.a /usr/local/lib/libwayland-server.a  /usr/local/lib/dri/i965_drv_video.a -lxcb -lxcb-shm -lxcb-shape -lxcb-xfixes -lasound -lbz2 -lz -llzma -lmp3lame -ldrm -lX11 -lXext -lXfixes -lXau -lvorbis -lvorbisenc -lpthread  -ldl -lm -lrt 
AR            = ar cqs

####### Output directory
####### Files
OBJECTS       = main.o \
		get_media_devices.o \
		capture.o \
		avfile.o \
		parse_options.o \
		mfile.o \
		head.o
#OBJECTS       = $(DISTNAME).o
DESTDIR       = ../bin
TARGET        = $(DESTDIR)/$(DISTNAME)

first: all
####### Build rules
$(TARGET):  $(OBJECTS)  
	@$(CHK_DIR_EXISTS) $(DESTDIR) || $(MKDIR) $(DESTDIR)
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)
all: Makefile $(TARGET)
dist: distdir FORCE
	(cd `dirname $(DISTDIR)` && $(TAR) $(DISTNAME).tar $(DISTNAME) && $(COMPRESS) $(DISTNAME).tar) && $(MOVE) `dirname $(DISTDIR)`/$(DISTNAME).tar.gz . && $(DEL_FILE) -r $(DISTDIR)
distdir: FORCE
	@$(CHK_DIR_EXISTS) $(DISTDIR) || $(MKDIR) $(DISTDIR)
	$(COPY_FILE) --parents $(DIST) $(DISTDIR)/
clean: compiler_clean 
	-$(DEL_FILE) $(OBJECTS)
distclean: clean 
	-$(DEL_FILE) $(OBJECTS)
	-$(DEL_FILE) $(TARGET)
####### Sub-libraries
check: first
compiler_clean: 
####### Compile
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $< 
%.o: %.c
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $< 
install:  FORCE
uninstall:  FORCE
FORCE:


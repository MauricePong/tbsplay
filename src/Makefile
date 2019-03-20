#############################################################################
# Makefile for building: ../bin/tbsplay_x$(ARCH)
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


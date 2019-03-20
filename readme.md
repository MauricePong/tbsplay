# tbsplay

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

  export LD_LIBRARY_PATH=/usr/local/lib/dri/:/usr/local/lib/:/usr/lib64:/usr/local/share/:/usr/local/lib64/:$LD_LIBRARY_PATH

<!--//Compile and install wayland-->
<!--// git clone git://anongit.freedesktop.org/wayland/wayland-->
<!--// cd wayland-->
<!--// ./autogen.sh-->
<!--// make-->
<!--// make install-->

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

## 4. Install TBS capture card driver

View: https://github.com/tbsdtv/linux_media/wiki

## 5.Add related struct:

/usr/include/linux/videodev2.h

struct v4l2_tbs_data{
	u32 baseaddr;
	u32 reg;
	__u32 value;
};

#define VIDIOC_TBS_G_CTL	_IOWR('V', 105, struct v4l2_tbs_data)

#define VIDIOC_TBS_S_CTL	_IOWR('V', 106, struct v4l2_tbs_data)

## 6.Usage:

-i : input the audio and  video file path,eg: -i video.ts ;eg: -i /dev/video0 ;eg:-i hw,1,0 ;eg:-i '/dev/video0 hw:1,0'

  -c : select gpu video encoders(h264_vaapi,hevc_vaapi,mjpeg_vaapi,mpeg2_vaapi,vp8_vaapi,vp9_vaapi,default h264_vaapi)

  -o : output file path,eg: -o video.ts ;eg: -o rtp://192.168.1.88:1234;eg: -o  'video.yuv audio.pcm' ;eg: -o sdl2

  Advanced encode mode setting:

  -x : set video encoder parameter <width,height,fps,bitrate>,default <rawdata_width,rawdata_height,rawdata_fps,app_default_bitrate>,note:bitrate is at least 1000.

  -y : set audio encoder parameter <sample rate>,default <rawdata_sample rate>,eg:-y 44100

  -f : toggle full screen

  -k : turn on deinterlacing

  -e : enum audio and video devices

  -h : help information

  -d : the tool's version

  While playing:

  ESC,control+c: quit

  F11          : toggle full screen

### #example:

 ./tbsplay  -i  "/dev/video0 hw1,0"  -o sdl2    <!--//Play the audio and video of the capture card port HDMI port 0 .-->

 ./tbsplay  -i  "/dev/video0 hw1,0"  -o  "raw_video0.yuv raw_audio1.pcm"    <!--//Record the original audio and video data of the capture card HDMI port 0.-->

 ./tbsplay  -i  "/dev/video0 hw1,0"  -o  av.ts    <!--//Record the encoded audio and video data of the capture card HDMI  port 0.-->

./tbsplay  -i  "/dev/video0 hw1,0"  -o  rtp://192.168.1.188:1234    <!--//Send the encoded audio and video data of the capture card HDMI  port 0 to 192.168.1.188:1234 through rtp.-->

./tbsplay  -i   av.ts  -o sdl2  <!--//Play av.ts .-->

./tbsplay  -i    rtp://192.168.1.188:1234  -o sdl2  <!--//Play  network stream  rtp://192.168.1.188:1234-->


#ARCH=armc
#ARCH=x86c
#ARCH=amlogic
ARCH=allwinner-h2

ifeq ($(ARCH), allwinner-h2)
   export ARCH=arm
   export CROSS_COMPILE=/home/jiangxiujie/h2222/lichee/out/sun8iw7p1/android/common/buildroot/external-toolchain/bin/arm-linux-gnueabi-
   LINUX_PATH =/home/jiangxiujie/h2222/lichee/linux-3.4
endif

ifeq ($(ARCH), armc)
    export ARCH=arm64
    export CROSS_COMPILE=/opt/toolchain/mstar/linaro-aarch64_linux-2014.09_843419-patched/bin/aarch64-linux-gnu-
    LINUX_PATH :=/home/jiangxiujie/mstar-828-tv/Mstar-828/vendor/mstar/kernel/3.10.40
endif

ifeq ($(ARCH), amlogic)
    export ARCH=arm64
    export CROSS_COMPILE=/opt/toolchain/amlogic/gcc-linaro-aarch64-linux-gnu-4.9-2014.09_linux/bin/aarch64-linux-gnu-
    LINUX_PATH :=/home/jiangxiujie/z1111/Amlogic-905/common
endif

ifeq ($(ARCH), x86c)
    LINUX_PATH :=/usr/src/linux-headers-4.4.0-75-generic
endif
obj-m += sunxi-ir-rx.o

all:
	make -C $(LINUX_PATH) M=`pwd` modules
clean:
	make -C $(LINUX_PATH) M=`pwd` modules clean

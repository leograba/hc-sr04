export ARCH=arm
export CROSS_COMPILE=/home/prjs/toolchain-linaro/gcc-linaro/bin/arm-linux-gnueabihf-
KDIR ?=/home/prjs/colibri_vf/build_from_source/linux-toradex

obj-m += hcsr04.o

all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean

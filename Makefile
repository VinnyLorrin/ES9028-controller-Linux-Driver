KERNEL_SRC = /lib/modules/$(shell uname -r)/source
BUILD_DIR := $(shell pwd)
DTC_DIR = /lib/modules/$(shell uname -r)/build/scripts/dtc/
VERBOSE = 0

OBJS    = hifibunny-codec.o hifibunny-q2m.o

obj-m := $(OBJS)

all:
	make -C $(KERNEL_SRC) SUBDIRS=$(BUILD_DIR) KBUILD_VERBOSE=$(VERBOSE) modules

clean:
	make -C $(KERNEL_SRC) SUBDIRS=$(BUILD_DIR) clean
	rm -f hifibunny-q2m-overlay.dtb
	rm -f hifibunny-q2m.dtbo

dtbs:
	$(DTC_DIR)/dtc -@ -I dts -O dtb -o hifibunny-q2m-overlay.dtb hifibunny-q2m-overlay.dts
	$(DTC_DIR)/dtc -@ -H epapr -I dts -O dtb -o hifibunny-q2m.dtbo hifibunny-q2m-overlay.dts

modules_install:
	cp hifibunny-codec.ko /lib/modules/$(shell uname -r)/kernel/sound/soc/codecs/
	cp hifibunny-q2m.ko /lib/modules/$(shell uname -r)/kernel/sound/soc/bcm/
	depmod -a

modules_remove:
	rm /lib/modules/$(shell uname -r)/kernel/sound/soc/codecs/hifibunny-codec.ko
	rm /lib/modules/$(shell uname -r)/kernel/sound/soc/bcm/hifibunny-q2m.ko
	depmod -a

install:
	modprobe hifibunny-codec
	modprobe hifibunny-q2m

remove:
	modprobe -r hifibunny-q2m
	modprobe -r hifibunny-codec

install_dtbo:
	cp hifibunny-q2m.dtbo /boot/overlays/

remove_dtbo:
	rm -f /boot/overlays/hifibunny-q2m-overlay.dtb
	rm -f /boot/overlays/hifibunny-q2m.dtbo

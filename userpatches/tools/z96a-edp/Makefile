# On the Z96A board (native aarch64), with matching headers installed:
#   sudo apt-get install -y ./linux-headers-current-rockchip64_*Pacfb*.deb
#   make
#   sudo insmod z96a_edp_hack.ko bist=1
#
# Cross-build from x86 host against the Pacfb worktree (after a prior kernel build):
#   make KDIR=/home/evanee/mygit/armbian-rp2/cache/sources/linux-kernel-worktree/6.18__rockchip64__arm64 \
#        ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-

obj-m += z96a_edp_hack.o

KDIR ?= /lib/modules/$(shell uname -r)/build
ARCH ?= $(shell uname -m | sed 's/aarch64/arm64/;s/x86_64/x86/')

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) clean

.PHONY: all clean

obj-m += KVM-RDTSC-SHIELD.o
SRCDIR = $(PWD)
KVM-RDTSC-SHIELD-objs := main.o rdtsc.o linkedList.o kernel-hook/hook.o
MCFLAGS += -std=gnu11 -I$(PWD)/include -O3
ccflags-y += ${MCFLAGS}
CC += ${MCFLAGS}
KDIR := /lib/modules/$(shell uname -r)/build
KOUTPUT := $(PWD)/build
KOUTPUT_MAKEFILE := $(KOUTPUT)/Makefile
LD += -S

all: $(KOUTPUT_MAKEFILE)
	make -C $(KDIR) M=$(KOUTPUT) src=$(SRCDIR) modules

$(KOUTPUT):
	mkdir -p "$@"
	mkdir -p "$@"/kernel-hook

$(KOUTPUT_MAKEFILE): $(KOUTPUT)
	touch "$@"

clean:
	make -C $(KDIR) M=$(KOUTPUT) src=$(SRCDIR) clean
	$(shell rm $(KOUTPUT_MAKEFILE))
	rmdir $(KOUTPUT)/kernel-hook
	rmdir $(KOUTPUT)

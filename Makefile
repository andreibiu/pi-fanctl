NAME     := fanctl
NAME_ALG := $(NAME)_alg
NAME_MOD := $(NAME)_mod

ifneq ($(KERNELRELEASE),)

obj-m        := $(NAME).o
$(NAME)-src  := $(NAME_MOD).c
$(NAME)-objs := $(NAME_MOD).o $(NAME_ALG).o

else

CPL          := gcc
CPL_FLAGS    := -O3 -w -ffreestanding -c
CONFIG_DEFS  := $(shell python3 ./parse_config.py $(NAME).cfg)
OVERLAYS_DIR := /boot/overlays/
KERN_DIR     ?= /lib/modules/$(shell uname -r)/build

all: build install clean

build: build_alg build_mod build_dtb

build_alg: $(NAME_ALG).c
    ifeq ($(CONFIG_DEFS),)
	$(error Error durring configuration parsing)
    else
	$(CPL) $(CONFIG_DEFS) $(CPL_FLAGS) $<
	mv $(NAME_ALG).o $(NAME_ALG).o_shipped
	echo "" > .$(NAME_ALG).o.cmd
    endif

build_mod: $(NAME_MOD).c
	mv $(NAME_ALG).c .$(NAME_ALG).c
	make -C "$(KERN_DIR)" $(if $(DEBUG),CFLAGS_MODULE=-DDEBUG,) M=$$PWD modules; \
	mv .$(NAME_ALG).c $(NAME_ALG).c

build_dtb: $(NAME).dts
	dtc -@ -I dts -O dtb -o $(NAME).dtbo $<

install:
	cp $(NAME).dtbo $(OVERLAYS_DIR)
	make -C "$(KERN_DIR)" M=$$PWD modules_install
	depmod -A

clean:
	make -C "$(KERN_DIR)" M=$$PWD clean
	rm -f $(NAME_ALG).o_shipped $(NAME).dtbo

endif

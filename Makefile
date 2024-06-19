NAME      := fanctl
NAME_ALG  := $(NAME)_alg
NAME_MOD  := $(NAME)_mod

ifneq ($(KERNELRELEASE),)

obj-m        := $(NAME).o
$(NAME)-src  := $(NAME_MOD).c
$(NAME)-objs := $(NAME_MOD).o $(NAME_ALG).o
ccflags-y    := -O3 $(if $(DEBUG),-DDEBUG,) $(if $(KERN_VER_GE_6_6_28_RPI),-DKERN_VER_GE_6_6_28_RPI,)

else

SRC_DIR      := src
BUILD_DIR    := build
CPL          := gcc
CPL_FLAGS    := -O3 -w -ffreestanding -c
OVERLAYS_DIR := /boot/overlays/
KERN_DIR     ?= /lib/modules/$(shell uname -r)/build

.PHONY: all build build_dtb build_alg build_mod install install_dtb install_mod clean

all: build install clean

build: build_dtb build_alg build_mod

install: install_dtb install_mod

build_dtb: $(BUILD_DIR)/$(NAME).dts
	dtc -@ -I dts -O dtb -o $(BUILD_DIR)/$(NAME).dtbo $<

build_alg: $(SRC_DIR)/$(NAME_ALG).c $(SRC_DIR)/$(NAME).h
	$(CPL) $(CPL_FLAGS) -c $< -o $(BUILD_DIR)/$(basename $(notdir $<)).o_shipped
	echo "" > $(BUILD_DIR)/.$(NAME_ALG).o.cmd

build_mod: $(BUILD_DIR)/$(NAME_MOD).c $(BUILD_DIR)/$(NAME).h $(BUILD_DIR)/Makefile
	make -C "$(KERN_DIR)" M=$(realpath $(BUILD_DIR)) modules

install_dtb:
	cp $(BUILD_DIR)/$(NAME).dtbo $(OVERLAYS_DIR)

install_mod:
	make -C "$(KERN_DIR)" M=$(realpath $(BUILD_DIR)) modules_install
	depmod -A

clean:
	rm -rf $(BUILD_DIR)
	mkdir $(BUILD_DIR)

$(BUILD_DIR)/%.c: $(SRC_DIR)/%.c
	ln -sf $(realpath $<) $@

$(BUILD_DIR)/%.h: $(SRC_DIR)/%.h
	ln -sf $(realpath $<) $@

$(BUILD_DIR)/%: %
	ln -sf $(realpath $<) $@

endif

##
## Copyright 2010-2023 Cypress Semiconductor Corporation
## Copyright 2024 Franco Venturi
##
## SPDX-License-Identifier: GPL-3.0-or-later
##

FX3FWROOT=/opt/FX3SDK/cyfx3sdk

#all:compile

include $(FX3FWROOT)/fw_build/fx3_fw/fx3_build_config.mak

MODULE = fx3-firmware

all:$(MODULE).img

SOURCE= app.c 	\
	dfc_modes.c	\
	gpif.c	\
	i2c.c	\
	si5351.c	\
	uart.c	\
	usb.c	\
	usbdscr.c	\
	cyfxtx.c	\
	modes/adc_modes.c	\
	modes/dual_adc.c	\
	modes/single_adc.c	\
	modes/uart_only.c

ifeq ($(CYFXBUILD),arm)
SOURCE_ASM=cyfx_startup.S
else
SOURCE_ASM=cyfx_gcc_startup.S
endif

C_OBJECT=$(SOURCE:%.c=./%.o)
A_OBJECT=$(SOURCE_ASM:%.S=./%.o)

EXES = $(MODULE).$(EXEEXT)

$(MODULE).$(EXEEXT): $(A_OBJECT) $(C_OBJECT)
	$(LINK)

cyfxtx.c:
	cp $(FX3FWROOT)/fw_build/fx3_fw/cyfxtx.c .
	@# set CY_U3P_SYS_MEM_TOP to 0x40080000 as per this KB article:
	@# https://community.infineon.com/t5/Knowledge-Base-Articles/Modifying-the-Default-Memory-Map-in-FX3-CX3-FX3S-SD3-FX2G2-and-SD2-KBA229114/ta-p/249462
	sed -i 's/^\(#define CY_U3P_SYS_MEM_TOP  *\)(0x40078000)/\1(0x40080000)/' cyfxtx.c

cyfx_startup.S:
	cp $(FX3FWROOT)/fw_build/fx3_fw/cyfx_startup.S .

cyfx_gcc_startup.S:
	cp $(FX3FWROOT)/fw_build/fx3_fw/cyfx_gcc_startup.S .

$(C_OBJECT) : %.o : %.c
	$(COMPILE)

$(A_OBJECT) : %.o : %.S
	$(ASSEMBLE)

ELF2IMG = ./elf2img

clean:
	#rm -f ./$(MODULE).$(EXEEXT) ./$(MODULE).map ./$(MODULE).img
	rm -f ./$(MODULE).$(EXEEXT) ./$(MODULE).map
	rm -f ./*.o ./modes/*.o
	rm -f cyfxtx.c cyfx_startup.S cyfx_gcc_startup.S


compile: $(C_OBJECT) $(A_OBJECT) $(EXES)

$(ELF2IMG) : $(FX3FWROOT)/util/elf2img/elf2img.c
	gcc -O -Wall -o $@ $<

$(MODULE).img: $(EXES) $(ELF2IMG) 
	$(ELF2IMG) -i $< -o $@ -v

#[]#

# ----------------------------
# Makefile Options
# ----------------------------

NAME = fac
DESCRIPTION = "Factory game"
COMPRESSED = NO
LDHAS_ARG_PROCESSING = 1

CFLAGS = -Wall -Wextra -Oz
CXXFLAGS = -Wall -Wextra -Oz
#EXTRA_C_SOURCES = ../agon_ccode/common/util.c

# Heap size to 352k allowing a 96k program
INIT_LOC = 040000
BSSHEAP_LOW = 058000
BSSHEAP_HIGH = 0AFFFF

# ----------------------------

include $(shell cedev-config --makefile)

check: bin/$(NAME).bin
	@FILE_SIZE=$(shell stat -L -c %s bin/fac.bin)
	@if [ $(shell stat -L -c %s $<) -gt 98303 ]; then echo "WARNING !! Executable is greater than 96k!"; fi;

install: check bin/$(NAME).bin
	#srec_cat bin/$(NAME).bin -binary -offset 0x40000 -o bin/$(NAME).hex -intel
	cp bin/$(NAME).bin $(NAME)
	cp bin/$(NAME).bin ~/agon/fab/sdcard/bin/
	cp bin/$(NAME).bin ~/agon/sdcard_sync/bin/
	rsync -rvu ./ ~/agon/sdcard_sync/fac


run: install
	cd ~/agon/fab-agon-emulator ;\
	./fab-agon-emulator --firmware console8 --vdp src/vdp/vdp_console8.so 

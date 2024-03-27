# ----------------------------
# Makefile Options
# ----------------------------

NAME = fac
DESCRIPTION = "Factory game"
COMPRESSED = NO
LDHAS_ARG_PROCESSING = 1

CFLAGS = -Wall -Wextra -Oz
CXXFLAGS = -Wall -Wextra -Oz

# Heap size to 384k allowing a 64k program
INIT_LOC = 040000
BSSHEAP_LOW = 050000
BSSHEAP_HIGH = 0AFFFF

# ----------------------------

include $(shell cedev-config --makefile)

check: bin/$(NAME).bin
	@FILE_SIZE=$(shell stat -L -c %s bin/fac.bin)
	@if [ $(shell stat -L -c %s $<) -gt 65535 ]; then echo "WARNING !! Executable is greater than 64k!"; fi;

install: bin/$(NAME).bin
	srec_cat bin/$(NAME).bin -binary -offset 0x40000 -o bin/$(NAME).hex -intel
	cp bin/$(NAME).bin $(NAME)
	rsync -rvu ./ ~/agon/sdcard_sync/fac


run: install
	cd ~/agon/fab-agon-emulator ;\
	./fab-agon-emulator --firmware console8 --vdp src/vdp/vdp_console8.so 

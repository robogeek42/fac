#------------------------------------
# Master makefile for all three parts
#------------------------------------

all: fac.bin fed.bin loader.bin
	@echo "Done"

fac.bin:
	$(MAKE) -C fac install

fed.bin:
	$(MAKE) -C fed install

loader.bin:
	$(MAKE) -C loader install

install:
	./install.sh ~/agon/fab/sdcard/fac
	./install.sh ../sdcard_sync/fac

clean:
	rm -rf fac/bin
	rm -rf fac/obj
	rm -rf fed/bin
	rm -rf fed/obj
	rm fac.bin
	rm fed.bin

.PHONY: clean fac.bin fed.bin loader.bin

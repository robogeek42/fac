#------------------------------------
# Master makefile for all three parts
#------------------------------------

BINARIES = fac.bin fed.bin loader.bin facnl.bin fednl.bin preload.bin

all: $(BINARIES)
	@echo "Done"

fac.bin:
	@echo "--------------------------------"
	@echo $@
	$(MAKE) -C fac install

facnl.bin:
	@echo "--------------------------------"
	@echo $@
	$(MAKE) -C fac_noload install

fed.bin:
	@echo "--------------------------------"
	@echo $@
	$(MAKE) -C fed install

fednl.bin:
	@echo "--------------------------------"
	@echo $@
	$(MAKE) -C fed_noload install

preload.bin:
	@echo "--------------------------------"
	@echo $@
	$(MAKE) -C preload install

loader.bin:
	@echo "--------------------------------"
	@echo $@
	$(MAKE) -C loader install

install:
	./install.sh ~/agon/fab/sdcard/fac
	./install.sh ../sdcard_sync/fac

clean:
	rm -rf obj bin
	rm -rf */bin
	rm -rf */obj
	rm -f $(BINARIES)

.PHONY: clean $(BINARIES)

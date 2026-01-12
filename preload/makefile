# ----------------------------
# Makefile Options
# ----------------------------

NAME = preload
DESCRIPTION = "FAC: Preload Images and Sounds"
COMPRESSED = NO
LDHAS_ARG_PROCESSING = 1

CFLAGS = -Wall -Wextra -Oz -I../src
CXXFLAGS = -Wall -Wextra -Oz -I../src
EXTRA_C_SOURCES  = ../src/util.c
EXTRA_C_SOURCES += ../src/colmap.c
EXTRA_C_SOURCES += ../src/item.c
EXTRA_C_SOURCES += ../src/listdir.c
EXTRA_C_SOURCES += ../src/progbar.c
EXTRA_C_SOURCES += ../src/thinglist.c

EXTRA_HEADERS = ../src/util.h
EXTRA_HEADERS += ../src/colmap.h
EXTRA_HEADERS += ../src/progbar.h
EXTRA_HEADERS += ../src/globals.h
EXTRA_HEADERS += ../src/hud.h
EXTRA_HEADERS += ../src/images.h
EXTRA_HEADERS += ../src/keydefines.h
EXTRA_HEADERS += ../src/file_handling.h

# Heap size to 352k allowing a 96k program
INIT_LOC = 040000
BSSHEAP_LOW = 058000
BSSHEAP_HIGH = 0AFFFF

# ----------------------------

include $(shell cedev-config --makefile)

install: bin/$(NAME).bin
	cp bin/$(NAME).bin ../$(NAME).bin


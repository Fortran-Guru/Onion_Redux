INCLUDE_SHMVAR=1
INCLUDE_CJSON=1
include ../common/config.mk

TARGET = serverBrowser
LDFLAGS += $(LDFLAGS) -lSDL -lSDL_ttf -lSDL_image -lSDL_mixer -lSDL_rotozoom -pthread -lz -lsqlite3

include ../common/commands.mk
include ../common/recipes.mk

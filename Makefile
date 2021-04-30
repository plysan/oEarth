.DEFAULT_GOAL := compile

CC = g++
CXXFLAGS = -std=c++17 -O2
LDFLAGS = -lglfw -lvulkan -ldl -lpthread
UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
LDFLAGS += -lX11 -lXxf86vm -lXrandr -lXi
endif

SHADER := $(filter-out %.spv,$(wildcard shader.*))
SPV := $(patsubst %,%.spv,$(SHADER))
SRC := $(wildcard *.cpp)
OBJ := $(patsubst %.cpp,%.o,$(SRC))
DEP := $(patsubst %.cpp,%.d,$(SRC))
PROG := $(patsubst %.cpp,%,$(SRC))

DIR_EXT_LIBS := extlibs
DIR_TEXTURES := textures

%.spv: %
	glslc $< -o $@

$(DIR_EXT_LIBS)/stb_image.h:
	wget https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -P $(DIR_EXT_LIBS)
	touch $@

$(DIR_TEXTURES)/texture.jpg:
	wget https://vulkan-tutorial.com/images/texture.jpg -P $(DIR_TEXTURES)
	touch $@

compile_main: $(DEP) $(DIR_EXT_LIBS)/stb_image.h
	@$(MAKE) $(PROG)

ifneq ($(filter clean,$(MAKECMDGOALS)),clean)
-include $(DEP)
endif

%.d: %.cpp
	@$(CC) -MM -MG $(CXXFLAGS) $< | sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@

%: %.d

compile: compile_main index $(SPV)

test: compile $(DIR_TEXTURES)/texture.jpg
	./main

clean:
	@$(RM) $(DEP) $(OBJ) $(PROG) $(SPV) cscope.out tags

index: cscope.out tags

cscope.out: $(SRC) $(DEP)
	cscope -bR

tags: $(SRC) $(DEP)
	ctags -R

.PHONY: compile compile_main test clean index

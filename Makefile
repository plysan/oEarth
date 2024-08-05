.DEFAULT_GOAL := compile

CC = g++
CXXFLAGS = -std=c++17
LDFLAGS = -lglfw -ltiff -lvulkan -ldl -lpthread
UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
LDFLAGS += -lX11 -lXxf86vm -lXrandr -lXi
else
LDFLAGS += -Wl,-rpath,${VULKAN_LIB}
endif

SHADER := $(filter-out %.spv %.h,$(wildcard shaders/*))
SPV := $(patsubst %,%.spv,$(SHADER))
SRC := $(wildcard *.cpp) $(wildcard libs/*.cpp) $(wildcard utils/*.cpp)
OBJ := $(patsubst %.cpp,%.o,$(SRC))
DEP := $(patsubst %.cpp,%.d,$(SRC))
PROG := $(patsubst %.cpp,%,$(SRC))

DIR_EXT_LIBS := extlibs
DIR_TEXTURES := textures

%.spv: % vars.h
	glslc --target-env=vulkan1.2 $< -o $@

$(DIR_EXT_LIBS)/stb_image.h:
	wget https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -P $(DIR_EXT_LIBS)
	touch $@

$(DIR_EXT_LIBS)/stb_image_write.h:
	wget https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h -P $(DIR_EXT_LIBS)
	touch $@

$(DIR_TEXTURES)/texture.jpg:
	wget https://vulkan-tutorial.com/images/texture.jpg -P $(DIR_TEXTURES)
	touch $@

compile_main: $(DIR_EXT_LIBS)/stb_image.h $(DIR_EXT_LIBS)/stb_image_write.h $(DEP) $(OBJ)
	@$(CC) $(CXXFLAGS) $(LDFLAGS) -o main $(OBJ)

ifneq ($(filter clean,$(MAKECMDGOALS)),clean)
-include $(DEP)
endif

%.d: %.cpp
	@$(CC) -MM -MG $(CXXFLAGS) $< | sed 's/$(notdir $*)\.o[ :]*/$(subst /,\/,$*).o $(subst /,\/,$@) : /g' > $@

%: %.d

compile: compile_main index $(SPV)

test: CXXFLAGS += -DDEBUG -g
test: run

run: compile $(DIR_TEXTURES)/texture.jpg
	./main

clean:
	@$(RM) $(DEP) $(OBJ) $(PROG) $(SPV) cscope.out tags

index: cscope.out tags

cscope.out: $(SRC) $(DEP)
	cscope -bR

tags: $(SRC) $(DEP)
	ctags -R

.PHONY: test run compile compile_main clean index

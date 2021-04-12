.DEFAULT_GOAL := compile

CFLAGS = -std=c++17 -O2
LDFLAGS = -lglfw -lvulkan -ldl -lpthread
UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
LDFLAGS += -lX11 -lXxf86vm -lXrandr -lXi
endif
DIR_EXT_LIBS := extlibs
DIR_TEXTURES := textures

VKDemo: main.cpp stb_image.h
	g++ $(CFLAGS) -o VKDemo main.cpp -I$(DIR_EXT_LIBS) $(LDFLAGS)

stb_image.h:
	wget https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -P $(DIR_EXT_LIBS)

texture.jpg:
	wget https://vulkan-tutorial.com/images/texture.jpg -P $(DIR_TEXTURES)

shader.vert.spv: shader.vert
	glslc shader.vert -o shader.vert.spv

shader.frag.spv: shader.frag
	glslc shader.frag -o shader.frag.spv

.PHONY: compile test clean

compile: VKDemo shader.vert.spv shader.frag.spv

test: compile texture.jpg
	./VKDemo

clean:
	rm -f VKDemo shader.vert.spv shader.frag.spv

CFLAGS = -std=c++17 -O2
LDFLAGS = -lglfw -lvulkan -ldl -lpthread
UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
LDFLAGS += -lX11 -lXxf86vm -lXrandr -lXi
endif

VulkanTest: main.cpp
	g++ $(CFLAGS) -o VulkanTest main.cpp $(LDFLAGS)

VertShader: shader.vert
	glslc shader.vert -o shader.vert.spv

FragShader: shader.frag
	glslc shader.frag -o shader.frag.spv

.PHONY: test clean

test: VulkanTest VertShader FragShader
	./VulkanTest

clean:
	rm -f VulkanTest

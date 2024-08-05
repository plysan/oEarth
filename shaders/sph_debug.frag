#version 450
#extension GL_EXT_debug_printf : enable

#include "graph.h"

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inVel;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(inVel.w / 13.4, inVel.w / 13.4, 1, 1);
}

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_debug_printf : enable

#include "graph.h"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 scatter_rgba;
layout(location = 2) in float sun_proportion;
layout(location = 3) in vec3 sun_n_cs;
layout(location = 4) in vec3 world_offset_n_cs;
layout(location = 5) in vec3 lat_n_cs;
layout(location = 6) in vec3 lng_n_cs;
layout(location = 7) in vec3 vertex_pos_n_cs;
layout(location = 8) in float cam_vertex_length;
layout(location = 10) in vec2 waterOffsetCoord;
layout(location = 11) in float bathy;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 4) uniform sampler2D compImg;

layout(location = 0) out vec4 outColor;

void main() {
    if (ubo.target == TARGET_SKY) {
        outColor.rgb = scatter_rgba.rgb;
    } else if (ubo.target == TARGET_SWE) {
        outColor.rgb = vec3(0.5);
    } else {
        outColor.rgb = scatter_rgba.rgb + (1 - scatter_rgba.a) * texture(texSampler, fragTexCoord).rgb * sun_proportion;
    }
}

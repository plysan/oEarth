#version 450
#extension GL_EXT_debug_printf : enable

#include "graph.h"

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inVel;
layout(location = 0) out vec4 outPos;
layout(location = 1) out vec4 outVel;

void main() {
    outPos = inPos;
    outVel = inVel;
    vec3 down_n = normalize(-ubo.word_offset);
    vec3 lng_n = normalize(cross(vec3(0, -1, 0), -down_n));
    vec3 lat_n = cross(-down_n, lng_n);
    vec2 coord = (inPos.yx - ubo.model[0].yx - vec2(ubo.waterRadiusM)) / ubo.waterRadiusM - ubo.waterOffset.yx * 2;
    vec3 cam_pos = (ubo.v_inv * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 pos = cam_pos - dot(cam_pos, down_n) * down_n + (coord.x * lat_n + coord.y * lng_n) * ubo.waterRadius + (ubo.model[0].z - inPos.z / earthRadiusM) * down_n;
    gl_Position = ubo.proj * ubo.view * vec4(pos, 1.0);
    gl_PointSize = atan(pow(inPos.w, 1.0/3) / earthRadiusM / length(pos - cam_pos)) / (FOV_RANGE / 180) * HEIGHT / 4;
}

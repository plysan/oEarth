#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 scatter_rgba;
layout(location = 2) in float sun_proportion;
layout(binding = 1) uniform sampler2D texSampler;

layout(std140, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 wordOffset;
    int target;
} ubo;

layout(location = 0) out vec4 outColor;

void main() {
    if (ubo.target == 1) {
        outColor.rgb = scatter_rgba.rgb * scatter_rgba.a;
    } else {
        outColor.rgb = scatter_rgba.rgb * scatter_rgba.a + (1 - scatter_rgba.a) * texture(texSampler, fragTexCoord).rgb * sun_proportion;
    }
}

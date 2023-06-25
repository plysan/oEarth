#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../vars.h"
#include "common.h"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 scatter_rgba;
layout(location = 2) in float sun_proportion;
layout(location = 3) in vec3 sun_n_cs;
layout(location = 4) in vec3 world_offset_n_cs;
layout(location = 5) in vec3 lat_n_cs;
layout(location = 6) in vec3 lng_n_cs;
layout(location = 7) in vec3 vertex_pos_n_cs;
layout(location = 8) in float cam_vertex_length;
layout(location = 9) in vec3 cam_param; // (dlat, dlng, pix_span)
layout(location = 10) in vec2 waterOffsetCoord;
layout(binding = 1) uniform sampler2D texSampler;

layout(std140, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 p_inv;
    mat4 v_inv;
    vec3 word_offset;
    int target;
    vec2 freqCoord;
    vec2 terrainOffset;
    float height;
    float time;
} ubo;

layout(location = 0) out vec4 outColor;

vec3 water_refrac = vec3(0.25, 0.7, 1);
vec3 sun_color = vec3(1) * 4;
float wave_h_domain = 1.0 / 10;
float wave_v0 = 0.00000003;
float dispersions[1][3] = {
    // h, wx, wy
    {1.0, 0.1, 0.1},
};

vec3 getViewRefl(vec2 delta, float h_coe, vec3 facet_n_cs, inout float fresnel_sky, inout float ref_port_sun) {
    vec4 compH = texture(compNorImg, waterOffsetCoord);
    vec2 procH = vec2(0.0);
    for (int i = 0; i < dispersions.length(); i++) {
    }
    vec3 surface_n_cs = normalize(world_offset_n_cs
            + (compH.x - cos((fragTexCoord.x + delta.x) / wave_h_domain + ubo.time) * h_coe) * lat_n_cs
            + (compH.y - cos((fragTexCoord.y + delta.y) / wave_h_domain + ubo.time) * h_coe) * lng_n_cs);
    float fresnel_sun = 0.02 + 0.98 * pow(1.0 - dot(sun_n_cs, surface_n_cs), 5.0); // Schlick's approximation
    ref_port_sun = exp(-10000 * pow(length(cross(facet_n_cs, surface_n_cs)), 4));
    ref_port_sun = fresnel_sun * ref_port_sun;
    fresnel_sky = 0.02 + 0.98 * pow(1.0 - dot(-vertex_pos_n_cs, surface_n_cs), 5.0);
    vec3 n_view_refl = reflect(vertex_pos_n_cs, surface_n_cs);
    if (dot(n_view_refl, world_offset_n_cs) <= 0) {
        // approximate surface that has no reflection
        fresnel_sky = 0;
        n_view_refl = world_offset_n_cs;
    }
    return n_view_refl;
}

void sampleN(int n, float h_coe, vec3 facet_n_cs, inout vec4 scatter_rgba_refl, inout float fresnel_sky, inout float ref_port_sun) {
    float fresnel_sky_tmp;
    float ref_port_sun_tmp;
    fresnel_sky = 0;
    ref_port_sun = 0;
    scatter_rgba_refl = vec4(0);
    float delta = 1.0 / n;
    float idx = -0.5 + delta / 2;
    for (int i=0; i<n; i++) {
        vec3 n_view_refl = getViewRefl(cam_param.xy * idx, h_coe, facet_n_cs, fresnel_sky_tmp, ref_port_sun_tmp);
        scatter_rgba_refl += texture4D(getScatterAngles(world_offset_n_cs, n_view_refl, sun_n_cs, 0)) / n;
        fresnel_sky += fresnel_sky_tmp / n;
        ref_port_sun += ref_port_sun_tmp / n;
        idx += delta;
    }
}

void main() {
    if (ubo.target == TARGET_SKY) {
        outColor.rgb = scatter_rgba.rgb;
    } else if (ubo.target == TARGET_WATER) {
        vec3 facet_n_cs = normalize(-vertex_pos_n_cs + sun_n_cs);
        float wave_h = waveDomainSize * wave_h_domain;
        float wave_span = wave_h / cam_param.z;
        float wave_v = wave_v0 * min(1, wave_span);
        float h_coe = wave_v / wave_h;

        vec4 scatter_rgba_refl;
        float fresnel_sky;
        float ref_port_sun;
        if (wave_span > 0.28) {
            if (wave_span < 2) {
                if (wave_span < 1) {
                    if (wave_span < 0.5) {
                        sampleN(4, h_coe, facet_n_cs, scatter_rgba_refl, fresnel_sky, ref_port_sun);
                    } else {
                        sampleN(3, h_coe, facet_n_cs, scatter_rgba_refl, fresnel_sky, ref_port_sun);
                    }
                } else {
                    sampleN(2, h_coe, facet_n_cs, scatter_rgba_refl, fresnel_sky, ref_port_sun);
                }
            } else {
                vec3 n_view_refl = getViewRefl(cam_param.xy, h_coe, facet_n_cs, fresnel_sky, ref_port_sun);
                scatter_rgba_refl = texture4D(getScatterAngles(world_offset_n_cs, n_view_refl, sun_n_cs, 0));
            }
        } else {
            vec3 n_view_refl = getViewRefl(cam_param.xy, 0, facet_n_cs, fresnel_sky, ref_port_sun);
            scatter_rgba_refl = texture4D(getScatterAngles(world_offset_n_cs, n_view_refl, sun_n_cs, 0));
        }

        outColor.rgb = scatter_rgba.rgb + (1 - scatter_rgba.a) * (
            scatter_rgba_refl.rgb * fresnel_sky
            + (1 - fresnel_sky) * water_refrac * scatter_rgba_refl.rgb
            + sun_color * scatter_rgba_refl.a * ref_port_sun
        );
    } else {
        outColor.rgb = scatter_rgba.rgb + (1 - scatter_rgba.a) * texture(texSampler, fragTexCoord).rgb * sun_proportion;
    }
}

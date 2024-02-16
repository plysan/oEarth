#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_debug_printf : enable

#include "../vars.h"
#include "common.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 scatter_rgba;
layout(location = 2) out float sun_proportion;
layout(location = 3) out vec3 sun_n_cs;
layout(location = 4) out vec3 world_offset_n_cs; // from earth center
layout(location = 5) out vec3 lat_n_cs;
layout(location = 6) out vec3 lng_n_cs;
layout(location = 7) out vec3 vertex_pos_n_cs;
layout(location = 8) out float cam_vertex_length;
layout(location = 9) out vec3 cam_param;
layout(location = 10) out vec2 waterOffsetCoord;
layout(location = 11) out float bathy;

layout(std140, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 p_inv;
    mat4 v_inv;
    vec3 word_offset; // from earth center
    int target;
    vec2 freqCoord;
    vec2 waterOffset;
    vec2 bathyUvMid;
    vec2 bathyRadius;
    float waterRadius;
    float height;
    float time;
} ubo;

layout(binding = 5) uniform sampler2D bathymetry;

float atmosphere_radius_square = atmosphereTopRadius * atmosphereTopRadius;
float scatter_height = (length((ubo.view * vec4(-ubo.word_offset, 1.0)).xyz) - earthRadius) / atmosphereThickness;
float green_pow_coefficient = 1 / pow(greenAvgWlength/redAvgWlength, 4);
float blue_pow_coefficient = 1 / pow(blueAvgWlength/redAvgWlength, 4);
//TODO
vec3 sun_ws = vec3(20000.0, 0.0, 0.0);

float dPixZenith = 0.0010908305661643995; // sin(yFov/yRes)
float wave_h_domain = 1.0 / 10;
float wave_v0 = 0.00000003;
const float meterToRad = 1.0 / earthRadiusM;

//#define DBG_STATIC
float waveHGen(vec2 freqCoord) {
    float h=0, hi, wi, w_num;
    vec2 w_dir;
    for (int i=0; i<w_count; i++) {
        w_dir = vec2(w_dirs[i][0], w_dirs[i][1]);
        w_num = length(w_dir);
        wi = w_dir.x * freqCoord.x + w_dir.y * freqCoord.y;
#ifndef DBG_STATIC
        wi += pi2 * cO * (-ubo.time) / (waveDomainSizeM / w_num);
#endif
        hi = w_amps[i] * sin(wi) * sqrt(g * bathy) / cO;
        h += hi;
    }
    return h;
}

void handle_water(inout vec3 vertex_pos_cs) {
    vec3 cam_vertex_dir = normalize((ubo.v_inv * vec4((ubo.p_inv * vec4(inPosition, 1)).xyz, 0)).xyz);
    vec3 cam_inv = (ubo.v_inv * vec4(0,0,0,1)).xyz;
    vec3 cam_earthCenter_dir = normalize(-ubo.word_offset - cam_inv);
    float cos_b = dot(cam_earthCenter_dir, cam_vertex_dir);
    if (cos_b < 0) {
        gl_Position = ubo.proj * ubo.view * ubo.model * vec4(cam_earthCenter_dir, 1);
        return;
    }
    float sin_b = sqrt(1 - pow(cos_b, 2));
    float cot_b = cos_b / sin_b;
    cam_vertex_length = ubo.height / cos_b;
    if (cam_vertex_length < 0.01) {
        if (cam_vertex_length > 0.0002) {
            float x = sqrt(pow(cot_b, 2) - 2 * ubo.height) - cot_b; // approximate using y=-x^2/2
            cam_vertex_length = length(vec2(0, ubo.height) - vec2(x, 1 - cosh(x)));
        }
    } else {
        float d = earthRadius / sin_b;
        float sin_a = (earthRadius + ubo.height) / d;
        float cos_a = -sqrt(1 - pow(sin_a, 2));
        cam_vertex_length = (earthRadius + ubo.height) * cos_b + earthRadius * cos_a;
    }
    vec3 surface_pos_ms = cam_inv + cam_vertex_dir * cam_vertex_length;
    vertex_pos_cs = (ubo.view * ubo.model * vec4(surface_pos_ms, 1)).xyz;

    cam_earthCenter_dir = (ubo.view * vec4(cam_earthCenter_dir, 0)).xyz;
    lng_n_cs = normalize(cross((ubo.view * vec4(0, -1, 0, 0)).xyz, -cam_earthCenter_dir));
    lat_n_cs = cross(-cam_earthCenter_dir, lng_n_cs);
    vec3 down_n_cs = cross(lat_n_cs, lng_n_cs);
    vec3 dir_coord = vertex_pos_cs - down_n_cs * dot(down_n_cs, vertex_pos_cs);
    float lat_dist = dot(lat_n_cs, dir_coord);
    float lng_dist = dot(lng_n_cs, dir_coord);

    float cos_zenith = dot(down_n_cs, normalize(vertex_pos_cs));
    float pix_span = cam_vertex_length * dPixZenith / cos_zenith;
    const float worldToFreqCoe = pi2 / waveDomainSize;
    float dLat = pix_span * dot(lat_n_cs, normalize(dir_coord)) * worldToFreqCoe;
    float dLng = pix_span * dot(lng_n_cs, normalize(dir_coord)) * worldToFreqCoe;
    cam_param = vec3(dLat, dLng, pix_span);

    float wave_size_v = wave_v0;
    if (cam_vertex_length / wave_size_v > 200) {
        wave_size_v *= 1 - smoothstep(200, 300, cam_vertex_length / wave_v0) / 100;
    }
    vec2 distCoord = vec2(lng_dist, lat_dist);
    bathy = waterDeepM - texture(bathymetry, ubo.bathyUvMid + distCoord / ubo.bathyRadius / 2).x;
    bathy = max(0.1, bathy);
    fragTexCoord = (ubo.freqCoord + distCoord * worldToFreqCoe) * (cO / sqrt(g * bathy));

    waterOffsetCoord = vec2(lng_dist, lat_dist) / ubo.waterRadius / 2 + ubo.waterOffset + vec2(0.5);
    float compH;
    if (waterOffsetCoord.x < 1 && waterOffsetCoord.x > 0 && waterOffsetCoord.y < 1 && waterOffsetCoord.y > 0) {
        compH = texture(compImg, waterOffsetCoord).x;
    } else {
        compH = waveHGen(fragTexCoord);
    }
    float water_height = compH * meterToRad;
    gl_Position = ubo.proj * vec4(((ubo.view * ubo.model * vec4(surface_pos_ms, 1)).xyz + world_offset_n_cs * water_height), 1);
}

void main() {
    sun_n_cs = normalize((ubo.view * vec4(sun_ws, 1)).xyz);
    vec3 world_offset_cs = (ubo.view * vec4(-ubo.word_offset,1)).xyz; // from cam
    world_offset_n_cs = normalize(-world_offset_cs);
    vec3 vertex_pos_cs;
    if (ubo.target == TARGET_WATER) {
        handle_water(vertex_pos_cs);
    } else {
        gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
        vertex_pos_cs = (ubo.view * ubo.model * vec4(inPosition,1)).xyz;
    }
    if (ubo.target == TARGET_TERRAIN) {
        fragTexCoord = inTexCoord;
        vec3 vert_n_cs = (ubo.view * vec4(inNormal, 0)).xyz;
        sun_proportion = clamp(dot(vert_n_cs, sun_n_cs), 0, 1);
        sun_proportion *= sun_proportion;
    }

    vec3 sphereCenter_vertex_cs = vertex_pos_cs - world_offset_cs;
    float sphereCenter_vertex_length = length(sphereCenter_vertex_cs);
    vec3 sphereCenter_vertex_normal_cs = normalize(sphereCenter_vertex_cs);
    vec3 vertex_pos_normal_cs = normalize(vertex_pos_cs);
    float height_coord_linear;
    vec3 up_sample_n_cs;
    if (scatter_height > 1.0) {
        height_coord_linear = 1.0;
        float viewer_vertex_sphereCenter_cos = dot(normalize(-sphereCenter_vertex_cs), -normalize(vertex_pos_cs));
        float viewer_vertex_sphereCenter_acos = acos(viewer_vertex_sphereCenter_cos);
        float vertex_pos_length = length(vertex_pos_cs);
        if (ubo.target == TARGET_SKY) {
            float vertex_sphereCenter_intersec_cos = cos(pi - 2 * viewer_vertex_sphereCenter_acos);
            float camera_offset = vertex_pos_length - sqrt(2 * atmosphere_radius_square * (1 - vertex_sphereCenter_intersec_cos));
            up_sample_n_cs = normalize(vertex_pos_normal_cs * camera_offset - world_offset_cs);
        } else {
            float vertex_atmosTopIntersect_sphereCenter_angle = asin(sphereCenter_vertex_length*sin(viewer_vertex_sphereCenter_acos)/atmosphereTopRadius);
            float camera_offset = vertex_pos_length - sqrt(pow(sphereCenter_vertex_length, 2) + pow(atmosphereTopRadius, 2)
                - 2*atmosphereTopRadius*sphereCenter_vertex_length*cos(pi-vertex_atmosTopIntersect_sphereCenter_angle-viewer_vertex_sphereCenter_acos));
            up_sample_n_cs = normalize(vertex_pos_normal_cs * camera_offset - world_offset_cs);
        }
    } else {
        height_coord_linear = scatter_height;
        up_sample_n_cs = world_offset_n_cs;
    }
    vertex_pos_n_cs = normalize(vertex_pos_cs);
    vec4 scatter_rgba_first = texture4D(getScatterAngles(up_sample_n_cs, vertex_pos_n_cs, sun_n_cs, height_coord_linear));
    scatter_rgba = scatter_rgba_first;
    scatter_rgba = clamp(scatter_rgba, vec4(0), vec4(1));
    scatter_rgba.a = max(scatter_rgba.r, max(scatter_rgba.g, scatter_rgba.b));
    if (ubo.target == TARGET_TERRAIN) {
        up_sample_n_cs = sphereCenter_vertex_normal_cs;
        height_coord_linear = clamp(sphereCenter_vertex_length - earthRadius, 0, atmosphereThickness) / atmosphereThickness;

        vec4 scatter_rgba_second = texture4D(getScatterAngles(up_sample_n_cs, vertex_pos_n_cs, sun_n_cs, height_coord_linear));
        float first_second_optical_depth = scatter_rgba_first.a / scatter_rgba_second.a;
        vec3 scatter_rgba_offset = vec3(scatter_rgba_second.r * first_second_optical_depth,
                                        scatter_rgba_second.g * pow(first_second_optical_depth, green_pow_coefficient),
                                        scatter_rgba_second.b * pow(first_second_optical_depth, blue_pow_coefficient));
        scatter_rgba.rgb = clamp(scatter_rgba_first.rgb - scatter_rgba_offset, vec3(0), vec3(1));
        scatter_rgba.a = clamp(max(scatter_rgba.r, max(scatter_rgba.g, scatter_rgba.b)), 0, 1);
    }
}

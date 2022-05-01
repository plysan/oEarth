#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../vars.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 scatter_rgba;
layout(location = 2) out float sun_proportion;
layout(binding = 2) uniform sampler3D scatterSampler;

layout(std140, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 wordOffset;
    int target;
} ubo;

layout(location = 3) out vec3 vertex_ws;
layout(location = 4) out vec3 vertex_origin_dir_cs;
layout(location = 5) out vec3 vertex_sun_dir_cs;

float pi = 3.141592653589793;
float atmosphere_radius_square = atmosphereTopRadius * atmosphereTopRadius;
float scatter_height = (length((ubo.view * vec4(-ubo.wordOffset, 1.0)).xyz) - earthRadius) / atmosphereThickness;
float scatter_texture_size = scatterTextureSunAngleSize * scatterTextureHeightSize;
float view_angle_factor = float(scatter_texture_size - 1) / scatter_texture_size;
float scatter_texture_offset = 1.0 / (scatter_texture_size * 2);
float scatter_texture_scale_factor = float(scatterTextureSunAngleSize - 1) / (scatter_texture_size);
int scatter_texture_4thd_size_sub_1 = int(pow(float(scatterTextureHeightSize), 2)) - 1;
float green_pow_coefficient = 1 / pow(greenAvgWlength/redAvgWlength, 4);
float blue_pow_coefficient = 1 / pow(blueAvgWlength/redAvgWlength, 4);
//TODO
vec3 sun_ws = vec3(20000.0, 0.0, 0.0);


vec4 getScatterAngles(vec3 up_n_cs, vec3 origin_vertex_dir_n_cs, vec3 sun_n_cs, float height_coord_linear, float height_sealevel){
    float view_angle_horizon_cos = -(sqrt(pow(earthRadius+height_sealevel, 2) - pow(earthRadius, 2)) / (earthRadius+height_sealevel));
    float scatter_view_angle_cos = dot(up_n_cs, origin_vertex_dir_n_cs);
    float scatter_view_angle_coord;
    if (scatter_view_angle_cos > view_angle_horizon_cos) {
        scatter_view_angle_coord = (0.5+pow((scatter_view_angle_cos-view_angle_horizon_cos)/(1-view_angle_horizon_cos), 0.2)*0.5) * view_angle_factor + scatter_texture_offset;
    } else {
        scatter_view_angle_coord = (0.5-pow((view_angle_horizon_cos-scatter_view_angle_cos)/(1+view_angle_horizon_cos), 0.2)*0.5) * view_angle_factor + scatter_texture_offset;
    }
    float scatter_sun_angle_vertical = (dot(up_n_cs, sun_n_cs)+1)/2 * scatter_texture_scale_factor + scatter_texture_offset;
    float scatter_sun_angle_horizontal = (dot(cross(up_n_cs, origin_vertex_dir_n_cs), cross(up_n_cs, sun_n_cs))+1)/2 * scatter_texture_scale_factor + scatter_texture_offset;
    return vec4(scatter_sun_angle_horizontal, scatter_sun_angle_vertical, scatter_view_angle_coord, pow(height_coord_linear, 0.5));
}

vec4 texture4D(vec4 scatter_params)
{
    int height_unormalize = int(scatter_params.w * scatter_texture_4thd_size_sub_1);
    float height_middle_offset = scatter_params.w * scatter_texture_4thd_size_sub_1 - height_unormalize;
    float y = float(height_unormalize / scatterTextureHeightSize) / scatterTextureHeightSize;
    float x = float(height_unormalize % scatterTextureHeightSize) / scatterTextureHeightSize;
    vec4 color_0 = texture(scatterSampler, vec3(scatter_params.x+x, scatter_params.y+y, scatter_params.z));
    height_unormalize++;
    y = float(height_unormalize / scatterTextureHeightSize) / scatterTextureHeightSize;
    x = float(height_unormalize % scatterTextureHeightSize) / scatterTextureHeightSize;
    vec4 color_1 = texture(scatterSampler, vec3(scatter_params.x+x, scatter_params.y+y, scatter_params.z));
    vec4 result = color_0 * (1.0 - height_middle_offset) + color_1 * height_middle_offset;
    return result;
}

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    if (ubo.target == 0) {
        fragTexCoord = inTexCoord;
    }

    vertex_ws = (ubo.model * vec4(inPosition,1)).xyz;

    vec3 vertex_pos_cs = (ubo.view * ubo.model * vec4(inPosition,1)).xyz;
    vertex_origin_dir_cs = vec3(0,0,0) - vertex_pos_cs;

    vec3 sun_cs = (ubo.view * vec4(sun_ws,1)).xyz;
    vertex_sun_dir_cs = sun_cs + vertex_origin_dir_cs;

    vec3 up_n_cs;
    float height_coord_linear;
    vec3 origin_vertex_dir_n_cs = -normalize(vertex_origin_dir_cs);
    vec3 sun_n_cs = normalize(sun_cs);

    vec3 sphereCenter_pos_cs = (ubo.view * vec4(-ubo.wordOffset,1)).xyz;
    vec3 sphereCenter_vertex_cs = vertex_pos_cs - sphereCenter_pos_cs;
    float sphereCenter_vertex_length = length(sphereCenter_vertex_cs);
    vec3 sphereCenter_vertex_normal_cs = normalize(sphereCenter_vertex_cs);
    vec3 vertex_pos_normal_cs = normalize(vertex_pos_cs);
    if (scatter_height > 1.0) {
        height_coord_linear = 1.0;
        float viewer_vertex_sphereCenter_cos = dot(normalize(-sphereCenter_vertex_cs), -normalize(vertex_pos_cs));
        float viewer_vertex_sphereCenter_acos = acos(viewer_vertex_sphereCenter_cos);
        float vertex_pos_length = length(vertex_pos_cs);
        if (ubo.target == 1) {
            float vertex_sphereCenter_intersec_cos = cos(pi - 2 * viewer_vertex_sphereCenter_acos);
            float camera_offset = vertex_pos_length - sqrt(2 * atmosphere_radius_square * (1 - vertex_sphereCenter_intersec_cos));
            up_n_cs = normalize(vertex_pos_normal_cs * camera_offset - sphereCenter_pos_cs);
        } else if (ubo.target == 0) {
            float vertex_atmosTopIntersect_sphereCenter_angle = asin(sphereCenter_vertex_length*sin(viewer_vertex_sphereCenter_acos)/atmosphereTopRadius);
            float camera_offset = vertex_pos_length - sqrt(pow(sphereCenter_vertex_length, 2) + pow(atmosphereTopRadius, 2)
                - 2*atmosphereTopRadius*sphereCenter_vertex_length*cos(pi-vertex_atmosTopIntersect_sphereCenter_angle-viewer_vertex_sphereCenter_acos));
            up_n_cs = normalize(vertex_pos_normal_cs * camera_offset - sphereCenter_pos_cs);
        }
    } else {
        height_coord_linear = scatter_height;
        up_n_cs = normalize(-sphereCenter_pos_cs);
    }
    vec4 scatter_rgba_first = texture4D(getScatterAngles(up_n_cs, origin_vertex_dir_n_cs, sun_n_cs, height_coord_linear, height_coord_linear*atmosphereThickness));
    scatter_rgba = scatter_rgba_first;
    scatter_rgba = clamp(scatter_rgba, vec4(0), vec4(1));
    scatter_rgba.a = max(scatter_rgba.r, max(scatter_rgba.g, scatter_rgba.b));
    if (scatter_rgba.a > 0) {
        scatter_rgba.rgb /= scatter_rgba.a;
    }
    if (ubo.target == 0) {
        sun_proportion = clamp(dot(sphereCenter_vertex_normal_cs, sun_n_cs), 0, 1);
        sun_proportion *= sun_proportion;

        up_n_cs = sphereCenter_vertex_normal_cs;
        float height_sealevel = clamp(sphereCenter_vertex_length - earthRadius, 0, atmosphereThickness);
        height_coord_linear = height_sealevel/atmosphereThickness;

        vec4 scatter_rgba_second = texture4D(getScatterAngles(up_n_cs, origin_vertex_dir_n_cs, sun_n_cs, height_coord_linear, height_sealevel));
        float first_second_optical_depth = scatter_rgba_first.a / scatter_rgba_second.a;
        vec3 scatter_rgba_offset = vec3(scatter_rgba_second.r * first_second_optical_depth,
                                        scatter_rgba_second.g * pow(first_second_optical_depth, green_pow_coefficient),
                                        scatter_rgba_second.b * pow(first_second_optical_depth, blue_pow_coefficient));
        scatter_rgba.rgb = clamp(scatter_rgba_first.rgb - scatter_rgba_offset, vec3(0), vec3(1));
        scatter_rgba.a = clamp(max(scatter_rgba.r, max(scatter_rgba.g, scatter_rgba.b)), 0, 1);
        if(scatter_rgba.a > 0) {
            scatter_rgba.rgb /= scatter_rgba.a;
        }
    }
}

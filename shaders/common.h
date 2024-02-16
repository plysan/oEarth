layout(binding = 2) uniform sampler3D scatterSampler;
layout(binding = 3) uniform sampler2D compImg;
layout(binding = 4) uniform sampler2D compNorImg;

float scatter_texture_size = scatterTextureSunAngleSize * scatterTextureHeightSize;
float view_angle_factor = float(scatter_texture_size - 1) / scatter_texture_size;
float scatter_texture_offset = 1.0 / (scatter_texture_size * 2);
float scatter_texture_scale_factor = float(scatterTextureSunAngleSize - 1) / (scatter_texture_size);
int scatter_texture_4thd_size_sub_1 = int(pow(float(scatterTextureHeightSize), 2)) - 1;
const float cO = sqrt(g * waterDeepM);

vec4 getScatterAngles(vec3 up_n_cs, vec3 vertex_pos_n_cs, vec3 sun_n_cs, float height_coord_linear){
    float height_sealevel = height_coord_linear * atmosphereThickness;
    float view_angle_horizon_cos = -(sqrt(pow(earthRadius+height_sealevel, 2) - pow(earthRadius, 2)) / (earthRadius+height_sealevel));
    float scatter_view_angle_cos = dot(up_n_cs, vertex_pos_n_cs);
    float scatter_view_angle_coord;
    if (scatter_view_angle_cos > view_angle_horizon_cos) {
        scatter_view_angle_coord = (0.5+pow((scatter_view_angle_cos-view_angle_horizon_cos)/(1-view_angle_horizon_cos), 0.2)*0.5) * view_angle_factor + scatter_texture_offset;
    } else {
        scatter_view_angle_coord = (0.5-pow((view_angle_horizon_cos-scatter_view_angle_cos)/(1+view_angle_horizon_cos), 0.2)*0.5) * view_angle_factor + scatter_texture_offset;
    }
    float scatter_sun_angle_vertical = (dot(up_n_cs, sun_n_cs)+1)/2 * scatter_texture_scale_factor + scatter_texture_offset;
    float scatter_sun_angle_horizontal = (dot(cross(up_n_cs, vertex_pos_n_cs), cross(up_n_cs, sun_n_cs))+1)/2 * scatter_texture_scale_factor + scatter_texture_offset;
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



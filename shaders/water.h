#include "common.h"

#define lIdx gl_LocalInvocationIndex

const uint subGroupSizeN1 = subGroupSize - 1;

struct Particle
{
  vec3 pos;
  float mass;
  vec3 vel;
  float dens;
};

layout(std140, binding = 0) uniform UniformBufferObject {
    ivec2 iOffset;
    vec2 bathyUvBl;
    vec2 freqCoordBl;
    vec2 freqCoordTr;
    vec2 freqCoordDel;
    float bathyUvDel;
    int stage;
    float time;
    float scale;
    float dt;
    float dxd; // dx * 2
} ubo;

vec4 waveGen(bool only_in, float bathy, ivec2 coord, vec2 v_dir, vec2 u_dir) {
    if (bathy < 0.1) return vec4(0);
    float vi = 0, ui = 0, h = 0, wi, hi, v_comp, u_comp, w_num, cos_in = 0, in_u = 0, in_v = 0;
    float co = sqrt(g * bathy);
    vec2 w_dir;
    for (int i=0; i<w_count; i++) {
        w_dir = vec2(w_dirs[i][0], w_dirs[i][1]);
        w_num = length(w_dir);
        w_dir = normalize(w_dir);
        v_comp = dot(v_dir, w_dir);
        u_comp = dot(u_dir, w_dir);
        in_v += w_amps[i] * v_comp;
        in_u += w_amps[i] * u_comp;
        if (only_in && v_comp < f0) continue;
        wi = w_num * w_dir.x * (ubo.freqCoordBl.x + coord.x * ubo.freqCoordDel.x)
           + w_num * w_dir.y * (ubo.freqCoordBl.y + coord.y * ubo.freqCoordDel.y);
        wi *= cO / co;
#ifndef DBG_STATIC
        wi += pi2 * cO * (-ubo.time - ubo.dt) / (waveDomainSizeM / w_num);
#endif
        hi = w_amps[i] * sin(wi) * co / cO;
        h += hi;
        vi += g / co * hi * v_comp;
        ui += g / co * hi * u_comp;
    }
    return vec4(h, vi, ui, in_v == 0 ? 0 : cos(atan(in_u / in_v)));
}


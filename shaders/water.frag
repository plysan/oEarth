#version 450
#extension GL_EXT_debug_printf : enable

#include "graph.h"

layout (location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;
layout(binding = 3) uniform sampler2D depthImg;

vec3 water_refrac = vec3(0.25, 0.7, 1);
vec3 sun_color = vec3(1) * 4;
vec3 upW = normalize(ubo.word_offset.xyz);

vec3 calPixM(vec2 uv) {
    vec4 pix = vec4(uv / ubo.fbRes, 0, 0);
    pix.z = texture(depthImg, pix.xy).r;
    pix = inverse(ubo.proj) * (vec4(pix.xy, pix.z, 1) * 2 - 1);
    pix /= pix.w;
    pix = inverse(ubo.view) * pix;
    return pix.xyz;
}

void main()
{
    vec3 lngW = normalize(cross(vec3(0, -1, 0), upW));
    vec3 latW = cross(upW, lngW);
    vec3 pixM = calPixM(gl_FragCoord.xy);
    vec4 camM = inverse(ubo.view) * vec4(0, 0, 0, 1);
    vec3 camToPix = pixM.xyz - camM.xyz;
    vec3 camToPixH = camToPix - dot(upW, camToPix) * upW;
    camToPixH *= earthRadiusM;
    vec2 coord = vec2(dot(latW, camToPixH.xyz), dot(lngW, camToPixH.xyz));
    vec3 pixNei1 = calPixM(gl_FragCoord.xy + vec2(0, 1));
    vec3 pixNei2 = calPixM(gl_FragCoord.xy + vec2(1, 0));
    vec3 waterNorm = normalize(cross(pixM - pixNei1, pixM - pixNei2));
    float fresnel_sky = 0, ref_port_sun = 0;
    vec3 pixToCam = normalize(-camToPix);
    float fresnel_sun = 0.02 + 0.98 * pow(1.0 - dot(sunDirW, waterNorm), 5.0); // Schlick's approximation
    vec3 facet_n_cs = normalize(pixToCam + sunDirW);
    ref_port_sun = exp(-10000 * pow(length(cross(facet_n_cs, waterNorm)), 4));
    ref_port_sun = fresnel_sun * ref_port_sun;
    fresnel_sky = 0.02 + 0.98 * pow(1.0 - dot(pixToCam, waterNorm), 5.0);
    vec3 n_view_refl = reflect(-pixToCam, waterNorm);
    if (dot(n_view_refl, upW) <= 0) { // approximate surface that has no reflection
        fresnel_sky = 0;
        n_view_refl = upW;
    }
    vec4 scatter_rgba_refl = texture4D(getScatterAngles(upW, n_view_refl, sunDirW, 0));
    outColor.rgb = scatter_rgba_refl.rgb * fresnel_sky
            + (1 - fresnel_sky) * water_refrac * scatter_rgba_refl.rgb
            + sun_color * scatter_rgba_refl.a * ref_port_sun;
}

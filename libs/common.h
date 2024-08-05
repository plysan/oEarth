#ifndef COMMON_H
#define COMMON_H

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct ImageSource {
    char *data;
    int w;
    int h;
    int comp;
    glm::vec2 tl_coord;
    glm::vec2 span;
};

struct FrameParam {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 p_inv;
    glm::mat4 v_inv;
    glm::vec3 wordOffset;
    int target;
    glm::vec2 freqCoord;
    glm::vec2 waterOffset;
    glm::vec2 bathyUvMid;
    glm::vec2 bathyRadius;
    float waterRadius;
    float waterRadiusM;
    float height;
    float time;
    union {
        VkDrawIndirectCommand terrainParam;
        VkDrawIndexedIndirectCommand skyParam;
        VkDrawIndexedIndirectCommand water_param;
    };
    glm::mat4 pad1;
    glm::mat3 pad2;
    glm::vec2 pad3;
};

#endif // COMMON_H

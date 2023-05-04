#ifndef WATER_H
#define WATER_H

#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "common.h"
#include "../vars.h"

struct WaterParam {
    glm::ivec2 wOffset;
    glm::vec2 bathyCoord;
    int stage;
    glm::mat3 d0;
    glm::mat4 d1;
    glm::mat4 d2;
    glm::mat4 d3;
    glm::vec2 v0;
};

struct WaterGrid {
    VkPipelineLayout pipLayout;
    VkDescriptorSetLayout descSetLayout;
    VkPipeline pip;
    std::vector<VkDescriptorSet> descSets;
    VkDescriptorPool descPool;
    void createImgs(VkQueue queue, VkCommandPool commandPool, glm::dvec3 cameraPos);
    std::vector<VkImage> compImgs;
    std::vector<VkDeviceMemory> compImgMems;
    std::vector<VkImageView> compImgViews;
    std::vector<VkSampler> compImgSamplers;
    uint32_t compImgCount = 2;
    uint32_t compImgSets = 2;
    uint32_t compImgStages = 2;
    VkImage normalImg;
    VkDeviceMemory normalImgMem;
    VkImageView normalImgView;
    VkSampler normalSampler;
    VkImage bathymetryImg;
    VkDeviceMemory bathymetryImgMem;
    VkImageView bathymetryImgView;
    VkSampler bathymetrySampler;
    int bathyRes = 2048;
    long gridsPerDegree = waterRes * (long)compDomainsPerDegree;
    std::vector<WaterParam> waterParam;
    VkBuffer uBuf = NULL;
    VkDeviceMemory uBufMem = NULL;
    void* uBufData = NULL;

    std::vector<Vertex> vertices;
    std::vector<int> indices;
    void init();
    void genWaterGrid(int x, int y);
    void recordCmd(VkCommandBuffer cmdBuf, int descSetNum);
    glm::dvec2 updateWOffset(glm::dvec2 word_offset_coord, double cos_lat);

    glm::ivec2 bathyICoord;
    glm::ivec2 bathyCoord;
};

#endif // WATER_H

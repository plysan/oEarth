#ifndef WATER_H
#define WATER_H

#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "common.h"
#include "../vars.h"

struct WaterParam {
    glm::ivec2 iOffset;
    glm::vec2 bathyUvBl;
    glm::vec2 freqCoordBl;
    glm::vec2 freqCoordTr;
    glm::vec2 freqCoordDel;
    float bathyUvDel;
    int stage;
    float time;
    float scale;
    float dt;
    float dx;
    glm::mat4 d1;
    glm::mat4 d2;
    glm::mat4 d3;
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
    std::vector<WaterParam> waterParam;
    VkBuffer uBuf = NULL;
    VkDeviceMemory uBufMem = NULL;
    void* uBufData = NULL;
    glm::dvec2 bathyRadius;

    std::vector<Vertex> vertices;
    std::vector<int> indices;
    void init();
    void genWaterGrid(int x, int y);
    void recordCmd(VkCommandBuffer cmdBuf, int descSetNum);
    glm::dvec2 updateWOffset(glm::dvec2 worldCoord, double &waterRadius, glm::vec2 &bathyUvMid, double cos_lat, int count);
};

#endif // WATER_H

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
    float dxd;
    glm::mat4 d1;
    glm::mat4 d2;
    glm::mat4 d3;
};

struct Particle
{
    glm::vec3 pos;
    float mass;
    glm::vec3 vel;
    float dens;
    //float pres;
};

struct WaterGrid {
    void init(VkQueue queue, VkCommandPool commandPool, glm::dvec3 cameraPos);
    void initRender(int width, int height, VkRenderPass renderPass, VkExtent2D swapChainExtent, VkDescriptorSetLayout gDsl);
    void cleanupRender();
    void recordCmd(VkCommandBuffer cmdBuf, int descSetNum);
    VkDescriptorPool descPool;

    VkPipelineLayout plDebug;
    VkPipeline pipDebug;

    void initBoundaryPip();
    VkPipelineLayout plBoundary;
    VkDescriptorSetLayout dslBoundary;
    VkPipeline pipBoundary;
    std::vector<VkDescriptorSet> dsBoundary;
    std::vector<VkBuffer> ptclBuf;
    std::vector<VkDeviceMemory> ptclBufMem;
    std::vector<void*> ptclBufData;
    VkBuffer ptclMap;
    VkDeviceMemory ptclMapMem;

    void initWaterPip();
    void createImgs(VkQueue queue, VkCommandPool commandPool, glm::dvec3 cameraPos);
    VkPipelineLayout plWater;
    VkDescriptorSetLayout dslWater;
    VkPipeline pipWater;
    std::vector<VkDescriptorSet> dsWater;
    std::vector<VkImage> compImgs;
    std::vector<VkDeviceMemory> compImgMems;
    std::vector<VkImageView> compImgViews;
    std::vector<VkSampler> compImgSamplers;
    VkImage normalImg;
    VkDeviceMemory normalImgMem;
    VkImageView normalImgView;
    VkSampler normalSampler;
    void stageBathymetry(glm::dvec3 cameraPos);
    void updateBathymetry(VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer cmdBuf);
    glm::dvec2 bathyRadius;
    glm::dvec2 bathyPos;
    glm::dvec2 bathyRadiusStaging;
    glm::dvec2 bathyPosStaging;
    void* bathyData = NULL;
    VkBuffer bathyStagingImg;
    VkDeviceMemory bathyStagingImgMem;
    VkImage bathymetryImg;
    VkDeviceMemory bathymetryImgMem;
    VkImageView bathymetryImgView;
    VkSampler bathymetrySampler;
    uint bathyRes = 2048;
    WaterParam waterParam;
    std::vector<VkBuffer> uBuf;
    std::vector<VkDeviceMemory> uBufMem;
    std::vector<void*> uBufData;

    std::vector<Vertex> vertices;
    std::vector<int> indices;
    void genWaterGrid(int x, int y);
    void updateWOffset(glm::dvec2 worldCoord, double &waterRadius, FrameParam &fParamWater, double cos_lat, int count, int pingpongIdx);

    void cleanup();
};

#endif // WATER_H

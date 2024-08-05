#include <iostream>
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/common.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../vars.h"
#include "../utils/coord.h"
#include "../utils/vulkan_boilerplate.h"
#include "water.h"
#include "globe.h"
#include "control.h"

void WaterGrid::createImgs(VkQueue queue, VkCommandPool cmdPool, glm::dvec3 cameraPos) {
    compImgs.resize(pingPongCount);
    compImgMems.resize(pingPongCount);
    compImgViews.resize(pingPongCount);
    compImgSamplers.resize(pingPongCount);
    int maxRes = glm::max(waterRes, bathyRes);
    std::vector<float> data(maxRes * maxRes * 4, 0);
    createInitialImage(waterRes, waterRes, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, normalImg, normalImgMem, queue, cmdPool, data.data());
    normalImgView = createImageView(normalImg, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 2);
    createSampler(normalSampler);

    for (int j = 0; j < waterRes; j++) { //x
        for (int k = 0; k < waterRes; k++) { //y
            static glm::vec2 p1 = glm::vec2(512.0f, 512.0f);
            float d1 = (j-p1.x)*(j-p1.x) + (k-p1.y)*(k-p1.y);
            //if (d1 < 1600) data[(waterRes * k + j) * 4] += 100.0f - glm::sqrt(d1);
        }
    }

    for (size_t i = 0; i < compImgs.size(); i++) {
        createInitialImage(waterRes, waterRes, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_TILING_OPTIMAL,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, compImgs.at(i), compImgMems.at(i), queue, cmdPool, data.data());
        compImgViews.at(i) = createImageView(compImgs.at(i), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 2);
        createSampler(compImgSamplers.at(i));
    }

    std::fill(data.begin(), data.end(), 0.0f);
    createBuffer(bathyRes * bathyRes * 16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                bathyStagingImg, bathyStagingImgMem);
    vkMapMemory(logicalDevice, bathyStagingImgMem, 0, VK_WHOLE_SIZE, 0, &bathyData);
    createInitialImage(bathyRes, bathyRes, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bathymetryImg, bathymetryImgMem, queue, cmdPool, NULL);
    bathymetryImgView = createImageView(bathymetryImg, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 2);
    createSampler(bathymetrySampler);

    DrawParam cmd{0, 1, 0, 0};
    VkDeviceSize bufSize = sizeof(DrawParam) + sizeof(uint32_t) * lwgCount * 2 + sizeof(LwgMap) * lwgCount + sizeof(Particle) * maxParticles;
    std::vector<uint32_t> mapData((bufSize - sizeof(DrawParam)) / sizeof(uint32_t), 0);
    for (int i = 0; i < lwgCount; i++) {
        mapData[i] = i * maxPadding;
        mapData[lwgCount + i] = maxPadding;
    }
    ptclBuf.resize(pingPongCount);
    ptclBufMem.resize(pingPongCount);
    ptclBufData.resize(pingPongCount);
    for (int i = 0; i < pingPongCount; i++) {
        createBuffer(bufSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            ptclBuf[i], ptclBufMem[i]);
        vkMapMemory(logicalDevice, ptclBufMem[i], sizeof(DrawParam), bufSize - sizeof(DrawParam), 0, &ptclBufData[i]);
        memcpy(ptclBufData[i], mapData.data(), bufSize - sizeof(DrawParam));
        vkUnmapMemory(logicalDevice, ptclBufMem[i]);
        vkMapMemory(logicalDevice, ptclBufMem[i], 0, sizeof(DrawParam), 0, &ptclBufData[i]);
        memcpy(ptclBufData[i], &cmd, sizeof(DrawParam));
        vkUnmapMemory(logicalDevice, ptclBufMem[i]);
    }

    bufSize = sizeof(WaterParam);
    uBuf.resize(pingPongCount);
    uBufMem.resize(pingPongCount);
    uBufData.resize(pingPongCount);
    for (int i = 0; i < pingPongCount; i++) {
        createBuffer(bufSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uBuf[i], uBufMem[i]);
        vkMapMemory(logicalDevice, uBufMem[i], 0, bufSize, 0, &uBufData[i]);
        memcpy(uBufData[i], &waterParam, bufSize);
    }
}

void WaterGrid::initWaterPip() {
    createDescriptorSetLayout(dslWater,
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
        {VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT,
            VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT});
    VkShaderModule smWater = createShaderModule(readFile("shaders/water.comp.spv"));
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &dslWater, 0, nullptr};
    if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &plWater) != VK_SUCCESS)
        throw std::runtime_error("failed to create SWE pipeline layout!");
    VkComputePipelineCreateInfo computePipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, 0, 0,
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 0, 0, VK_SHADER_STAGE_COMPUTE_BIT, smWater, "main", 0},
        plWater, 0, 0};
    if (vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipWater) != VK_SUCCESS)
        throw std::runtime_error("failed to create SWE pipeline!");
    vkDestroyShaderModule(logicalDevice, smWater, nullptr);
    std::vector<VkDescriptorSetLayout> compLayouts(pingPongCount, dslWater);
    VkDescriptorSetAllocateInfo compDescSetAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, 0, descPool, pingPongCount, compLayouts.data()};
    dsWater.resize(pingPongCount);
    if (vkAllocateDescriptorSets(logicalDevice, &compDescSetAllocInfo, dsWater.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate SWE descriptor sets!");
}

void WaterGrid::initBoundaryPip() {
    createDescriptorSetLayout(dslBoundary,
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
        {VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT,
            VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_COMPUTE_BIT});
    VkShaderModule smSphSort = createShaderModule(readFile("shaders/water_boundary.comp.spv"));
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &dslBoundary, 0, nullptr};
    if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &plBoundary) != VK_SUCCESS)
        throw std::runtime_error("failed to create SPH pipeline layout!");
    VkComputePipelineCreateInfo computePipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, 0, 0,
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 0, 0, VK_SHADER_STAGE_COMPUTE_BIT, smSphSort, "main", 0},
        plBoundary, 0, 0};
    if (vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipBoundary) != VK_SUCCESS)
        throw std::runtime_error("failed to create SPH pipeline!");
    vkDestroyShaderModule(logicalDevice, smSphSort, nullptr);

    std::vector<VkDescriptorSetLayout> compLayouts(pingPongCount, dslBoundary);
    VkDescriptorSetAllocateInfo compDescSetAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, 0, descPool, pingPongCount, compLayouts.data()};
    dsBoundary.resize(pingPongCount);
    if (vkAllocateDescriptorSets(logicalDevice, &compDescSetAllocInfo, dsBoundary.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate SPH descriptor sets!");
}

void WaterGrid::init(VkQueue queue, VkCommandPool cmdPool, glm::dvec3 cameraPos) {
    assert(waterRes % (subGroupSize * subGroupSize) == 0);
    std::cout << "WaterParam size:" << sizeof(WaterParam) << ", align:" << ubAlign << '\n';
    assert(sizeof(WaterParam) % ubAlign == 0);
    assert(subGroupsPerSphLane <= maxLane);
    assert(subGroupSize <= maxLane);
    assert(st2MaxSize <= maxLane);
    assert(st2MaxSize >= subGroupsPerSphLane);
    assert(maxLane % subGroupSize == 0);
    assert(lwgCount <= maxLane);
    createImgs(queue, cmdPool, cameraPos);
    createDescriptorPool(descPool, pingPongCount * 2/*SWE + SPH*/, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, pingPongCount * 2/*water + boundary*/},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, pingPongCount * 2/*water + boundary bathy*/},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, pingPongCount * (3/*water*/ + 2/*boundary*/)},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, pingPongCount * (3/*water*/ + 2/*boundary*/)},
    });
    initWaterPip();
    initBoundaryPip();
    VkDescriptorBufferInfo uboInfo[2] = {
        {uBuf[0], 0, sizeof(WaterParam)},
        {uBuf[1], 0, sizeof(WaterParam)},
    };
    VkDescriptorImageInfo normalInfo{normalSampler, normalImgView, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo bathymetryInfo{bathymetrySampler, bathymetryImgView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo gridInfos[2] = {
        {compImgSamplers[0], compImgViews[0], VK_IMAGE_LAYOUT_GENERAL},
        {compImgSamplers[1], compImgViews[1], VK_IMAGE_LAYOUT_GENERAL},
    };
    VkDescriptorBufferInfo ptclBufInfo[2] = {
        {ptclBuf[0], 0, VK_WHOLE_SIZE},
        {ptclBuf[1], 0, VK_WHOLE_SIZE},
    };
    VkWriteDescriptorSet waterWrite[14] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr, &uboInfo[0], nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &normalInfo, nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[0], 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &bathymetryInfo, nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[0], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         nullptr, &ptclBufInfo[0], nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[0], 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         nullptr, &ptclBufInfo[1], nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[0], 5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &gridInfos[0], nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[0], 6, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &gridInfos[1], nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr, &uboInfo[1], nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &normalInfo, nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[1], 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &bathymetryInfo, nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[1], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         nullptr, &ptclBufInfo[1], nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[1], 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         nullptr, &ptclBufInfo[0], nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[1], 5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &gridInfos[1], nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsWater[1], 6, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &gridInfos[0], nullptr, nullptr},
    };
    vkUpdateDescriptorSets(logicalDevice, sizeof(waterWrite) / sizeof(VkWriteDescriptorSet), waterWrite, 0, 0);
    VkWriteDescriptorSet boundaryWrite[12] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsBoundary[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr, &uboInfo[0], nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsBoundary[0], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &normalInfo, nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsBoundary[0], 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &bathymetryInfo, nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsBoundary[0], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         nullptr, &ptclBufInfo[0], nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsBoundary[0], 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         nullptr, &ptclBufInfo[1], nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsBoundary[0], 5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &gridInfos[1], nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsBoundary[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr, &uboInfo[1], nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsBoundary[1], 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &normalInfo, nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsBoundary[1], 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &bathymetryInfo, nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsBoundary[1], 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         nullptr, &ptclBufInfo[1], nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsBoundary[1], 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         nullptr, &ptclBufInfo[0], nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, dsBoundary[1], 5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          &gridInfos[0], nullptr, nullptr},
    };
    vkUpdateDescriptorSets(logicalDevice, sizeof(boundaryWrite) / sizeof(VkWriteDescriptorSet), boundaryWrite, 0, 0);
}

void WaterGrid::initRender(int width, int height, VkRenderPass renderPass, VkExtent2D swapChainExtent, VkDescriptorSetLayout gDsl) {
    createGraphicsPipeline(plDebug, pipDebug, gDsl, renderPass, swapChainExtent,
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST, "sph_debug", sizeof(Particle), {offsetof(Particle, pos), offsetof(Particle, vel)}, {VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT});
}

void WaterGrid::cleanupRender() {
    vkDestroyPipeline(logicalDevice, pipDebug, nullptr);
    vkDestroyPipelineLayout(logicalDevice, plDebug, nullptr);
}

void WaterGrid::genWaterGrid(int x, int y) {
    vertices.clear();
    indices.clear();
    float x_half = (x - 1) / 2.0f, y_half = (y - 1) / 2.0f;
    for (int i = 0; i < y; i++) {
        for (int j = 0; j < x; j++) {
            vertices.push_back({glm::vec3((j - x_half) / x_half, (i - y_half) / y_half, 0.0f)});
        }
    }
    for (int i = 0; i < y - 1; i++) {
        for (int j = 0; j < x - 1; j++) {
            indices.push_back(i * x + j);
            indices.push_back((i + 1) * x + j + 1);
            indices.push_back((i + 1) * x + j);
            indices.push_back(i * x + j);
            indices.push_back(i * x + j + 1);
            indices.push_back((i + 1) * x + j + 1);
        }
    }
}

void WaterGrid::stageBathymetry(glm::dvec3 cameraPos) {
    glm::dvec2 worldCoord = dPos2DCoord(cameraPos);
    bathyPosStaging = worldCoord;
    double cos_lat = glm::cos(worldCoord.x);
    worldCoord = worldCoord / glm::pi<double>() * 180.0;
    double waterRadius = (glm::max(0.0001/*replace me*/, glm::length(cameraPos) - 1.0)) * glm::tan(glm::pi<double>()/3);
    double bathyRadiusLat = waterRadius * 2;
    bathyRadiusStaging = glm::dvec2(bathyRadiusLat, bathyRadiusLat / cos_lat);
    fillBathymetry(worldCoord - bathyRadiusStaging / glm::pi<double>() * 180.0, worldCoord + bathyRadiusStaging / glm::pi<double>() * 180.0, static_cast<float*>(bathyData), bathyRes);
}

void WaterGrid::updateBathymetry(VkQueue graphicsQueue, VkCommandPool cmdPool, VkCommandBuffer cmdBuf) {
    bool init = cmdBuf == NULL;
    if (init) cmdBuf = beginCmdBuf(cmdPool);
    pipBarrier(cmdBuf, bathymetryImg,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    copyBufferToImage(cmdBuf, bathyStagingImg, bathymetryImg, bathyRes, bathyRes, 1, graphicsQueue, cmdPool);
    pipBarrier(cmdBuf, bathymetryImg,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    if (init) endCmdBuf(graphicsQueue, cmdPool, cmdBuf, true);
    bathyPos = bathyPosStaging;
    bathyRadius = bathyRadiusStaging;
}

void WaterGrid::updateWOffset(glm::dvec2 worldCoord, double &waterRadius, FrameParam &fParamWater, double cos_lat, int count, int pingpongIdx) {
    static double waterRadiusOld = waterRadius / waterRes / 10;
    static double waterBathyPropo = waterRadius / bathyRadius.x;
    static double bathyUvBlOrig = 0.5 - 0.5 * waterBathyPropo, bathyUvDel = 1.0 / (waterRes-1) * waterBathyPropo;
    static glm::dvec2 waterDel = glm::dvec2(waterRadius * 2 / (waterRes-1), waterRadius * 2 / (waterRes-1) / cos_lat);
    static double dxd = waterDel.x * earthRadiusM * 2;
    double scale = waterRadius / waterRadiusOld;
    glm::dvec2 waterCoord, waterCoordFrac;
    glm::ivec2 waterICoord, waterICoordDel;
    static glm::dvec2 worldICoord;
    static glm::ivec2 waterICoordOld = glm::round(worldCoord / waterDel);
    static int wGridMoveStep = 1;
    if (scale < 1.5 && scale > 2.0/3) {
        scale = 1.0;
        waterCoord = worldCoord / waterDel;
        waterICoord = glm::ivec2(waterCoord);
        waterICoord -= waterICoord % wGridMoveStep;
        waterRadius = waterRadiusOld;
        waterCoordFrac = waterCoord - (glm::dvec2)waterICoord;
        waterICoordDel = waterICoord - waterICoordOld;
        waterICoordDel = glm::ivec2(waterICoordDel.y, waterICoordDel.x);
    } else {
        waterRadiusOld = waterRadius;
        waterBathyPropo = waterRadius / bathyRadius.x;
        bathyUvDel = 1.0 / (waterRes-1) * waterBathyPropo;
        bathyUvBlOrig = 0.5 - 0.5 * waterBathyPropo;
        waterDel = glm::dvec2(waterRadius * 2 / (waterRes-1), waterRadius * 2 / (waterRes-1) / cos_lat);
        dxd = waterDel.x * earthRadiusM * 2;
        glm::dvec2 waterCoord = worldCoord / waterDel;
        waterICoord = glm::ivec2(waterCoord);
        waterICoord -= waterICoord % wGridMoveStep;
        waterCoordFrac = waterCoord - (glm::dvec2)waterICoord;
        waterICoordDel = glm::ivec2(0);
    }
    waterICoordOld = waterICoord;
    waterParam.iOffset = waterICoordDel;
    fParamWater.model[0].x = waterICoordDel.x * dxd / 2;
    fParamWater.model[0].y = waterICoordDel.y * dxd / 2;
    waterParam.dt = control.dt;
    waterParam.dxd = dxd;

    worldICoord = (glm::dvec2)waterICoord * waterDel;
    glm::dvec2 bathyUvBl = glm::dvec2(bathyUvBlOrig);
    glm::dvec2 bathyDeviate = glm::dvec2(bathyPos - worldICoord) / (bathyRadius * 2.0);
    bathyUvBl -= bathyDeviate;
    bathyUvBl = glm::dvec2(bathyUvBl.y, bathyUvBl.x);
    waterParam.bathyUvBl = bathyUvBl;
    fParamWater.bathyUvMid = (0.5 - glm::dvec2(bathyDeviate.y, bathyDeviate.x));
    waterParam.bathyUvDel = bathyUvDel;
    waterParam.time = control.dt * count;

    waterParam.scale = scale;
    static glm::dvec2 waveRectSize = glm::dvec2(waveDomainSize, waveDomainSize / cos_lat);
    static double pi2d = glm::pi<double>() * 2;
    glm::dvec2 freqRadiusXY = glm::dvec2(glm::abs(waterRadius / waveDomainSize * pi2d));
    glm::dvec2 freqCoordMod = glm::fmod((glm::dvec2)waterICoord * waterDel / waveRectSize * pi2d, pi2d);
    freqCoordMod = glm::dvec2(freqCoordMod.y, freqCoordMod.x);
    waterParam.freqCoordBl = freqCoordMod - freqRadiusXY;
    waterParam.freqCoordTr = freqCoordMod + freqRadiusXY;
    waterParam.freqCoordDel = freqRadiusXY * 2.0 / (waterRes-1.0);
    memcpy(uBufData[pingpongIdx], &waterParam, sizeof(WaterParam));
    fParamWater.waterOffset = glm::dvec2(waterCoordFrac.y, waterCoordFrac.x) / static_cast<double>(waterRes - 1);
}

void WaterGrid::recordCmd(VkCommandBuffer cmdBuf, int descSetNum) {
    uint32_t dynOffset = 0;
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipWater);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, plWater, 0, 1, &dsWater[descSetNum], 1, &dynOffset);
    vkCmdDispatch(cmdBuf, maxLane / subGroupSize, maxLane / subGroupSize, 1);
    pipBarrier(cmdBuf, compImgs[(descSetNum+1)%2], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    VkBufferMemoryBarrier ptclBufBarr{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, ptclBuf[(descSetNum+1)%2], sizeof(DrawParam), VK_WHOLE_SIZE};
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &ptclBufBarr, 0, nullptr);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipBoundary);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, plBoundary, 0, 1, &dsBoundary[descSetNum], 1, &dynOffset);
    vkCmdDispatch(cmdBuf, std::max(static_cast<uint32_t>(4), waterRes * 4 / maxLane), 1, 1);
}

void WaterGrid::cleanup() {
    vkDestroyPipelineLayout(logicalDevice, plBoundary, nullptr);
    vkDestroyDescriptorSetLayout(logicalDevice, dslBoundary, nullptr);
    vkDestroyPipeline(logicalDevice, pipBoundary, nullptr);
    for (auto buf : ptclBuf) vkDestroyBuffer(logicalDevice, buf, nullptr);
    for (auto bufMem : ptclBufMem) vkFreeMemory(logicalDevice, bufMem, nullptr);
    vkDestroyBuffer(logicalDevice, ptclMap, nullptr);
    vkFreeMemory(logicalDevice, ptclMapMem, nullptr);
    vkDestroyPipelineLayout(logicalDevice, plWater, nullptr);
    vkDestroyDescriptorSetLayout(logicalDevice, dslWater, nullptr);
    vkDestroyPipeline(logicalDevice, pipWater, nullptr);
    for (auto img : compImgs) vkDestroyImage(logicalDevice, img, nullptr);
    for (auto imgMem : compImgMems) vkFreeMemory(logicalDevice, imgMem, nullptr);
    for (auto imgView : compImgViews) vkDestroyImageView(logicalDevice, imgView, nullptr);
    for (auto imgSampler : compImgSamplers) vkDestroySampler(logicalDevice, imgSampler, nullptr);
    vkDestroyImage(logicalDevice, normalImg, nullptr);
    vkFreeMemory(logicalDevice, normalImgMem, nullptr);
    vkDestroyImageView(logicalDevice, normalImgView, nullptr);
    vkDestroySampler(logicalDevice, normalSampler, nullptr);
    vkDestroyBuffer(logicalDevice, bathyStagingImg, nullptr);
    vkFreeMemory(logicalDevice, bathyStagingImgMem, nullptr);
    vkDestroyImage(logicalDevice, bathymetryImg, nullptr);
    vkFreeMemory(logicalDevice, bathymetryImgMem, nullptr);
    vkDestroyImageView(logicalDevice, bathymetryImgView, nullptr);
    vkDestroySampler(logicalDevice, bathymetrySampler, nullptr);
    for (auto buf : uBuf) vkDestroyBuffer(logicalDevice, buf, nullptr);
    for (auto bufMem : uBufMem) vkFreeMemory(logicalDevice, bufMem, nullptr);
}

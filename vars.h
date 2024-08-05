//define variables used in both cpu and gpu

#ifndef VARS_H
#define VARS_H

const int TARGET_TERRAIN = 0;
const int TARGET_SKY = 1;
const int TARGET_WATER = 2;
const float FOV_RANGE = 100.0f;
const uint WIDTH = 600;
const uint HEIGHT = 250;

const float earthRadius = 1.0f;
const float atmosphereTopRadius = 1.011f;
const float atmosphereThickness = atmosphereTopRadius - earthRadius;
const int scatterTextureSunAngleSize = 13;
const int scatterTextureHeightSize = 5;
const float colorSrgbOffset = 0.0089498;
const float colorSrgbCoefficient = 0.7904051;
const float redAvgWlength = 0.685;
const float greenAvgWlength = 0.5325;
const float blueAvgWlength = 0.4725;
const float waveDomainSize = 0.0001f;
const float waveDomainSizeM = 637.8f;
const float g = 9.81;
const float euler = 2.71828182846;
const float pi = 3.141592653589793;
const float pi2 = 2 * pi;
const uint waterRes = 1024;
const float waterDeepM = 20;
const int w_count = 1;
const float w_amps[w_count] = {1.0};
const float w_dirs[w_count][2] = {{1, 4}};
const float earthRadiusM = 6378000;
const int pingPongCount = 2;
const uint maxLane = 1024; // must equal to maxComputeWorkGroupInvocations
const uint maxLaneN1 = maxLane - 1;
const uint subGroupSize = 32; // must equal to maxSubgroupSize
const uint radixRectSize = 4;
const uint radixRectSizeN1 = radixRectSize - 1;
const uint radixRange = radixRectSize * radixRectSize;
const uint subGroupsPerSphLane = maxLane / subGroupSize;
const uint stMaxSize = radixRange * subGroupsPerSphLane;
const uint st2MaxSize = stMaxSize / subGroupSize > subGroupSize ? stMaxSize / subGroupSize : subGroupSize;
const uint maxPadding = subGroupSize;
const uint ptclMapRadixSize = 4 * radixRectSizeN1;
const uint lwgCount = maxLane / subGroupSize * maxLane / subGroupSize;
const uint maxLwgParticles = 353;
const uint maxParticles = maxLwgParticles * lwgCount;

struct LwgMap {
    uint boundaryIdx[ptclMapRadixSize];
    uint boundarySize[ptclMapRadixSize];
};

// VkDrawIndirectCommand
struct DrawParam {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint firstInstance;
};

#endif // VARS_H

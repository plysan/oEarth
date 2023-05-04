//define variables used in both cpu and gpu

#ifndef VARS_H
#define VARS_H

const int TARGET_TERRAIN = 0;
const int TARGET_SKY = 1;
const int TARGET_WATER = 2;

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
const float waveDomainSize = 0.00001f;
const int compDomainsPerDegree = 10;
const float pi = 3.141592653589793;
const float pi2 = 2 * pi;
const int localSize = 32;
const int waterRes = 1024;
const int compsPerBathySide = 8;

#endif // VARS_H

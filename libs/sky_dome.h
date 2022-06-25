#ifndef SKY_DOME_H
#define SKY_DOME_H

#include <vector>
#include "common.h"

struct SkyDome {
    std::vector<Vertex> vertices;
    std::vector<int> indices;
    ImageSource scatterTexture;
    void genSkyDome(glm::vec3 cameraPos);
    void genScatterTexure();
};

#endif // SKY_DOME_H

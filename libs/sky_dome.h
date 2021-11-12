#ifndef SKY_DOME_H
#define SKY_DOME_H

#include "common.h"
#include <vector>

struct SkyDome {
    std::vector<Vertex> vertices;
    std::vector<int> indices;
    TextureSource texture;
    void genSkyDome(int latCount, int lngCount, float height);
};

#endif // SKY_DOME_H

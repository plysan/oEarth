#ifndef WATER_H
#define WATER_H

#include <vector>
#include <glm/glm.hpp>

#include "common.h"

struct WaterGrid {
    std::vector<Vertex> vertices;
    std::vector<int> indices;
    void genWaterGrid(int x, int y);
};

#endif // WATER_H

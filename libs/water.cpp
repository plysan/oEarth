#include <iostream>
#include <vector>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "../vars.h"
#include "../utils/coord.h"
#include "water.h"

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


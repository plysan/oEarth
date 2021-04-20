#ifndef TETRAHEDRAL_GLOBE_H
#define TETRAHEDRAL_GLOBE_H

#include "common.h"

void genTetrahedralGlobe(std::vector<Vertex> &vertices, std::vector<uint16_t> &indices, double latitude, double longitude, double height);

#endif // TETRAHEDRAL_GLOBE_H

#ifdef TETRAHEDRAL_GLOBE_IMPL

void genTetrahedralGlobe(std::vector<Vertex> &vertices, std::vector<uint16_t> &indices, double latitude, double longitude, double height) {
    vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
    };
    indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4
    };
}

#endif // TETRAHEDRAL_GLOBE_IMPL

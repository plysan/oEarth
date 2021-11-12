#include <iostream>
#include <glm/gtx/rotate_vector.hpp>

#include "common.h"
#include "../utils/coord.h"
#include "sky_dome.h"

void SkyDome::genSkyDome(int latCount, int lngCount, float height) {
    vertices.clear();
    //TODO 90 dynamic
    float latInterval = 90.0f/(latCount - 1);
    float lngInterval = 360.0f/(lngCount - 1);
    for (int i=0; i<latCount; i++) {
        for (int j=0; j<lngCount; j++) {
            glm::vec3 pos = coord2Pos(i*latInterval, j*lngInterval, height);
            // north pole rotate to coord(0, 90) to align with glm view coordinate's original lookat direction
            pos = glm::rotate(pos, 90.0f/180.0f*glm::pi<float>(), glm::vec3(-1.0f, 0.0f, 0.0f));
            vertices.push_back({pos, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}});
        }
    }
    indices.clear();
    for (int i=0; i<latCount-1; i++) {
        for (int j=0; j<lngCount-1; j++) {
            int begin = j + lngCount * i;
            indices.push_back(begin);
            indices.push_back(begin + lngCount);
            indices.push_back(begin + lngCount + 1);
            indices.push_back(begin + lngCount + 1);
            indices.push_back(begin + 1);
            indices.push_back(begin);
        }
    }
    std::cout << "Sky indices: " << indices.size() << ", vertices: " << vertices.size() << '\n';
}

#ifndef UTILS_H
#define UTILS_H

#include <glm/glm.hpp>

glm::vec3 lvec3(glm::dvec3 data);

glm::dvec3 hvec3(glm::vec3 data);

glm::dvec3 coord2Pos(float lat, float lng, float att);

#endif // UTILS_H

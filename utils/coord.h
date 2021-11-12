#ifndef UTILS_H
#define UTILS_H

#include <glm/glm.hpp>

glm::vec3 lvec3(glm::dvec3 data);
glm::dvec3 hvec3(glm::vec3 data);

glm::vec3 coord2Pos(float lat, float lng, float att);
glm::dvec3 coord2DPos(float lat, float lng, float att);
glm::mat4 rotByLookAt(glm::vec3 dir);

#endif // UTILS_H

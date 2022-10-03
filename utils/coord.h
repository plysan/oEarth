#ifndef UTILS_H
#define UTILS_H

#include <glm/glm.hpp>

glm::vec3 lvec3(glm::dvec3 data);
glm::dvec3 hvec3(glm::vec3 data);

glm::vec3 coord2Pos(float lat, float lng, float att);
glm::dvec3 coord2DPos(float lat, float lng, float att);
glm::dvec3 dCoord2DPos(double lat, double lng, double att);
glm::mat4 rotByLookAt(glm::vec3 dir);
glm::dvec2 dPos2DCoord(glm::dvec3 dPos);

#endif // UTILS_H

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/constants.hpp>

glm::vec3 lvec3(glm::dvec3 data) {
    return glm::vec3((float)data.x, (float)data.y, (float)data.z);
}

glm::dvec3 hvec3(glm::vec3 data) {
    return glm::dvec3((double)data.x, (double)data.y, (double)data.z);
}

glm::dvec3 coord2Pos(float lat, float lng, float att) {
    glm::dvec3 origin = glm::dvec3(-1.0, 0.0, 0.0);
    origin = glm::rotate(origin, lat/180.0*glm::pi<double>(), glm::dvec3(0.0, 1.0, 0.0));
    origin = glm::rotate(origin, lng/180.0*glm::pi<double>(), glm::dvec3(0.0, 0.0, 1.0));
    origin *= (1.0 + att);
    return origin;
}

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

// Based on vulkan coordinate system.
// North pole at (0, -1, 0)
// lat:0;lng:0 at (1, 0, 0)
glm::vec3 coord2Pos(float lat, float lng, float att) {
    glm::vec3 origin = glm::vec3(1.0f, 0.0f, 0.0f);
    origin = glm::rotate(origin, lat/180.0f*glm::pi<float>(), glm::vec3(0.0f, 0.0f, -1.0f));
    origin = glm::rotate(origin, lng/180.0f*glm::pi<float>(), glm::vec3(0.0f, -1.0f, 0.0f));
    if (att != 0.0f) {
        origin *= (1.0f + att);
    }
    return origin;
}

glm::dvec3 coord2DPos(float lat, float lng, float att) {
    glm::dvec3 origin = glm::dvec3(1.0, 0.0, 0.0);
    origin = glm::rotate(origin, lat/180.0*glm::pi<double>(), glm::dvec3(0.0, 0.0, -1.0));
    origin = glm::rotate(origin, lng/180.0*glm::pi<double>(), glm::dvec3(0.0, -1.0, 0.0));
    if (att != 0.0f) {
        origin *= (1.0 + att);
    }
    return origin;
}

glm::mat4 rotByLookAt(glm::vec3 dir) {
    // Initial direction should point at direction (0, 0, 1) in vulkan coordinate
    // All axes of glm view coordinate are flipped compared to vulkan coordinate
    return glm::inverse(glm::lookAt(glm::vec3(0.0f), -dir, glm::vec3(0.0f, 1.0f, 0.0f)));
}

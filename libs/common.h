#ifndef COMMON_H
#define COMMON_H

#include <glm/glm.hpp>
#include <vector>
#include <GLFW/glfw3.h>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct ImageSource {
    char *data;
    int w;
    int h;
    int comp;
    glm::dvec2 tl_coord;
    double span;
};

struct Camera {
    glm::dvec3 pos;
    glm::vec3 dir;
    glm::vec3 up;
    glm::mat4 proj;
    glm::vec3 vlct;
    float aspect;
};

struct GlobeInfo {
    std::vector<Vertex> vertices;
    ImageSource mega_texture;
    virtual void genGlobe(glm::dvec3 p_cam_pos) = 0;
};

void updateCamera(Camera &camera, GLFWwindow *window);

#endif // COMMON_H

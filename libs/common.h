#ifndef COMMON_H
#define COMMON_H

#include <glm/glm.hpp>
#include <vector>
#include <GLFW/glfw3.h>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
};

struct TextureSource {
    unsigned char *data;
    int w;
    int h;
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
    int vert_cur;
    TextureSource texture;
    virtual void genGlobe(glm::dvec3 p_cam_pos) = 0;
};

void updateCamera(Camera &camera, GLFWwindow *window);

#endif // COMMON_H

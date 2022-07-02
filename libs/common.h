#ifndef COMMON_H
#define COMMON_H

#include <glm/glm.hpp>

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
    glm::vec2 tl_coord;
    glm::vec2 span;
};

#endif // COMMON_H

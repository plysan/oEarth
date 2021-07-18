#ifndef COMMON_H
#define COMMON_H

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

struct GlobeInfo {
    std::vector<Vertex> vertices;
    TextureSource texture;
};

struct Camera {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    float aspect;
};

void updateCamera(Camera &camera);

#endif // COMMON_H

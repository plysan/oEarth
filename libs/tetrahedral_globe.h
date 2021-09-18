#ifndef TETRAHEDRAL_GLOBE_H
#define TETRAHEDRAL_GLOBE_H

#include "common.h"

struct DemSource {
    glm::vec2 mid_coord;
    glm::vec2 rad;
    int16_t *data;
    int w;
    int h;
    long length;
};

// start from tip, counter clock wise
struct TriNode {
    bool is_pole;
    int level;
    glm::vec3 vert_1;
    glm::vec3 vert_2;
    glm::vec3 vert_3;
    glm::vec2 tex_1;
    glm::vec2 tex_2;
    glm::vec2 tex_3;
    std::vector<TriNode> child;
    TriNode *parent;
    DemSource *dem;
};

class TetrahedraGlobe : public GlobeInfo {
    glm::dvec3 cam_pos;
    DemSource dem_source;
    void upLevel(TriNode &node);
    void loadDem(DemSource &source);
    short readDem(DemSource &source, glm::vec2 &coord);
    void collect(TriNode& node);
public:
    void genGlobe(glm::dvec3 p_cam_pos);
};

#endif // TETRAHEDRAL_GLOBE_H


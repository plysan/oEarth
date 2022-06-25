#ifndef TETRAHEDRAL_GLOBE_H
#define TETRAHEDRAL_GLOBE_H

#include "common.h"

// start from tip, counter clock wise
struct TriNode {
    bool is_pole;
    int level;
    ImageSource *dem_source;
    glm::dvec3 vert_1;
    glm::dvec3 vert_2;
    glm::dvec3 vert_3;
    glm::dvec2 coord_1;
    glm::dvec2 coord_2;
    glm::dvec2 coord_3;
    glm::vec2 tex_1;
    glm::vec2 tex_2;
    glm::vec2 tex_3;
    std::vector<TriNode> child;
    TriNode *parent;
    std::string tex_source_0;
    std::string tex_source_1;
};

class TetrahedraGlobe : public GlobeInfo {
    void upLevel(TriNode &node);
    void collect(TriNode& node);
    void setTexLayout(std::vector<TriNode> &nodes);
public:
    glm::dvec3 camOffset;
    void genGlobe(glm::dvec3 camPos);
};

#endif // TETRAHEDRAL_GLOBE_H


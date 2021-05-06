#ifndef TETRAHEDRAL_GLOBE_H
#define TETRAHEDRAL_GLOBE_H

#include "common.h"

void genTetrahedralGlobe(std::vector<Vertex> &vertices, std::vector<uint16_t> &indices, double latitude, double longitude, double height);

#endif // TETRAHEDRAL_GLOBE_H

#ifdef TETRAHEDRAL_GLOBE_IMPL

#include <iostream>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/string_cast.hpp>

struct TriNode {
    bool is_pole;
    glm::vec3 vert_1;
    glm::vec3 vert_2;
    glm::vec3 vert_3;
    glm::vec2 tex_1;
    glm::vec2 tex_2;
    glm::vec2 tex_3;
    std::vector<TriNode> child;
};

void upLevel(TriNode &node, int level) {
    if (level > 4) {
        return;
    }
    glm::vec3 mid_vert_1 = glm::normalize(node.vert_1 + node.vert_2);
    glm::vec3 mid_vert_2 = glm::normalize(node.vert_2 + node.vert_3);
    glm::vec3 mid_vert_3 = glm::normalize(node.vert_3 + node.vert_1);
    glm::vec2 mid_tex_1, mid_tex_2, mid_tex_3;
    mid_tex_2.x = (node.tex_2.x + node.tex_3.x) / 2.0f;
    mid_tex_2.y = glm::angle(mid_vert_2, glm::vec3(0.0f, 0.0f, 1.0f)) / glm::pi<double>();
    if (node.is_pole) {
        mid_tex_1.x = node.tex_2.x;
        mid_tex_1.y = (node.tex_1.y + node.tex_2.y) / 2.0f;
        mid_tex_3.x = node.tex_3.x;
        mid_tex_3.y = mid_tex_1.y;
    } else {
        if (node.tex_1.x == node.tex_2.x) {
            mid_tex_1.x = node.tex_2.x;
        } else {
            mid_tex_1.x = glm::orientedAngle(glm::vec2(1.0f, 0.0f), glm::normalize(glm::vec2(mid_vert_1.x, mid_vert_1.y))) / glm::pi<double>() / 2.0f;
            if (mid_tex_1.x < 0) {
                mid_tex_1.x += 1;
            }
        }
        if (node.tex_1.x == node.tex_3.x) {
            mid_tex_3.x = node.tex_3.x;
        } else {
            mid_tex_3.x = glm::orientedAngle(glm::vec2(1.0f, 0.0f), glm::normalize(glm::vec2(mid_vert_3.x, mid_vert_3.y))) / glm::pi<double>() / 2.0f;
            if (mid_tex_3.x < 0) {
                mid_tex_3.x += 1;
            }
        }
        mid_tex_1.y = glm::angle(mid_vert_1, glm::vec3(0.0f, 0.0f, 1.0f)) / glm::pi<double>();
        mid_tex_3.y = glm::angle(mid_vert_3, glm::vec3(0.0f, 0.0f, 1.0f)) / glm::pi<double>();
    }

    node.child.push_back({node.is_pole ? true : false, node.vert_1, mid_vert_1, mid_vert_3,
            node.tex_1, mid_tex_1, mid_tex_3});
    node.child.push_back({false, mid_vert_1, node.vert_2, mid_vert_2,
            mid_tex_1, node.tex_2, mid_tex_2});
    node.child.push_back({false, mid_vert_3, mid_vert_2, node.vert_3,
            mid_tex_3, mid_tex_2, node.tex_3});
    node.child.push_back({false, mid_vert_2, mid_vert_3, mid_vert_1,
            mid_tex_2, mid_tex_3, mid_tex_1});
    for (auto &child_node : node.child) {
        upLevel(child_node, level + 1);
    }
}

void collect(std::vector<Vertex> &vertices, TriNode& node) {
    if (node.child.empty()) {
        vertices.push_back({node.vert_1, {0.0f, 0.0f, 0.0f}, node.tex_1});
        vertices.push_back({node.vert_2, {0.0f, 0.0f, 0.0f}, node.tex_2});
        vertices.push_back({node.vert_3, {0.0f, 0.0f, 0.0f}, node.tex_3});
        return;
    } else {
        for (auto &child_node : node.child) {
            collect(vertices, child_node);
        }
    }
}

void genTetrahedralGlobe(std::vector<Vertex> &vertices, std::vector<uint16_t> &indices, double latitude, double longitude, double height) {
    clock_t begin = clock();
    indices = {
        1,2,3,
    };
    std::vector<TriNode> nodes = {
        {true, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.125f, 0.0f}, {0.0f, 0.5f}, {0.25f, 0.5f}},
        {true, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.375f, 0.0f}, {0.25f, 0.5f}, {0.5f, 0.5f}},
        {true, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.625f, 0.0f}, {0.5f, 0.5f}, {0.75f, 0.5f}},
        {true, {0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.875f, 0.0f}, {0.75f, 0.5f}, {1.0f, 0.5f}},
        {true, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.125f, 1.0f}, {0.25f, 0.5f}, {0.0f, 0.5f}},
        {true, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.375f, 1.0f}, {0.5f, 0.5f}, {0.25f, 0.5f}},
        {true, {0.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.625f, 1.0f}, {0.75f, 0.5f}, {0.5f, 0.5f}},
        {true, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.875f, 1.0f}, {1.0f, 0.5f}, {0.75f, 0.5f}},
    };
    for (auto &node : nodes) {
        upLevel(node, 1);
    }
    for (auto &node : nodes) {
        collect(vertices, node);
    }
    std::cout << "Execution time: " << (double)(clock() - begin)/CLOCKS_PER_SEC << "s, Points: " << vertices.size() << '\n';
}

#endif // TETRAHEDRAL_GLOBE_IMPL

#ifndef TETRAHEDRAL_GLOBE_H
#define TETRAHEDRAL_GLOBE_H

#include "common.h"

void genTetrahedralGlobe(GlobeInfo &globe, double latitude, double longitude, double height);

#endif // TETRAHEDRAL_GLOBE_H

#ifdef TETRAHEDRAL_GLOBE_IMPL

#include <string>
#include <iostream>
#include <sstream>
#include <tiffio.h>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/string_cast.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "../extlibs/stb_image.h"


struct DemSource {
    glm::vec2 mid_coord;
    glm::vec2 rad;
    uint16_t *data;
    int w;
    int h;
    long length;
};
struct DemSource dem_source;

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

void upLevel(TriNode &node) {
    if (node.level > 6) {
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

    node.child.push_back({node.is_pole ? true : false, node.level + 1, node.vert_1, mid_vert_1, mid_vert_3,
            node.tex_1, mid_tex_1, mid_tex_3});
    node.child.push_back({false, node.level + 1, mid_vert_1, node.vert_2, mid_vert_2,
            mid_tex_1, node.tex_2, mid_tex_2});
    node.child.push_back({false, node.level + 1, mid_vert_3, mid_vert_2, node.vert_3,
            mid_tex_3, mid_tex_2, node.tex_3});
    node.child.push_back({false, node.level + 1, mid_vert_2, mid_vert_3, mid_vert_1,
            mid_tex_2, mid_tex_3, mid_tex_1});
    for (auto &child_node : node.child) {
        upLevel(child_node);
    }
}

void loadDem(DemSource &source) {
    TIFF* tif = TIFFOpen("textures/dem.tif", "r");
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &source.w);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &source.h);
    tmsize_t strip_size = TIFFStripSize(tif)>>1;
    uint32_t strips = TIFFNumberOfStrips(tif);
    std::cout << "DEM dimension: (" << std::dec << source.w << ", " << source.h << ") " << strip_size << " * " << strips << '\n';
    source.data = (uint16_t*)_TIFFmalloc(source.w * source.h * 2);
    for (int strip=0; strip<strips; strip++) {
        TIFFReadEncodedStrip(tif, strip, source.data+strip*strip_size, (tsize_t) -1);
    }
    for (int y=0; y<source.h; y+=20) {
        for (int x=0; x<source.w; x+=10) {
            std::cout << (source.data[y * source.w + x]-32767 > 0 ? '*' : ' ');
        }
        std::cout << '\n';
    }
    TIFFClose(tif);
    return;
}

short readDem(DemSource &source, glm::vec2 &coord) {
    uint16_t x = (uint16_t)(coord.x * source.w);
    uint16_t y = (uint16_t)(coord.y * source.h);
    return source.data[source.w * y + x]-32767;
}

void collect(GlobeInfo &globe, TriNode& node) {
    if (node.child.empty()) {
        globe.vertices.push_back({node.vert_1 * (float)((100000.0f + readDem(dem_source, node.tex_1)) / 100000.0f), {0.0f, 0.0f, 0.0f}, node.tex_1});
        globe.vertices.push_back({node.vert_2 * (float)((100000.0f + readDem(dem_source, node.tex_2)) / 100000.0f), {0.0f, 0.0f, 0.0f}, node.tex_2});
        globe.vertices.push_back({node.vert_3 * (float)((100000.0f + readDem(dem_source, node.tex_3)) / 100000.0f), {0.0f, 0.0f, 0.0f}, node.tex_3});
        return;
    } else {
        for (auto &child_node : node.child) {
            collect(globe, child_node);
        }
    }
}

void genTetrahedralGlobe(GlobeInfo &globe, double latitude, double longitude, double height) {
    clock_t begin = clock();
    TriNode root;
    root.parent = NULL;
    loadDem(dem_source);
    int comp;
    stbi_uc* pixels = stbi_load("textures/texture.jpg", &globe.texture.w, &globe.texture.h, &comp, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
    std::cout << "Read texture dimension (" << std::dec << globe.texture.w << ',' << globe.texture.h << ")\n";
    globe.texture.data = pixels;

    std::vector<TriNode> nodes = {
        {true, 0, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.125f, 0.0f}, {0.0f, 0.5f}, {0.25f, 0.5f}},
        {true, 0, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.375f, 0.0f}, {0.25f, 0.5f}, {0.5f, 0.5f}},
        {true, 0, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.625f, 0.0f}, {0.5f, 0.5f}, {0.75f, 0.5f}},
        {true, 0, {0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.875f, 0.0f}, {0.75f, 0.5f}, {1.0f, 0.5f}},
        {true, 0, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.125f, 1.0f}, {0.25f, 0.5f}, {0.0f, 0.5f}},
        {true, 0, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.375f, 1.0f}, {0.5f, 0.5f}, {0.25f, 0.5f}},
        {true, 0, {0.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.625f, 1.0f}, {0.75f, 0.5f}, {0.5f, 0.5f}},
        {true, 0, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.875f, 1.0f}, {1.0f, 0.5f}, {0.75f, 0.5f}},
    };
    for (auto &node : nodes) {
        upLevel(node);
    }
    for (auto &node : nodes) {
        collect(globe, node);
    }
    std::cout << "Execution time: " << (double)(clock() - begin)/CLOCKS_PER_SEC << "s, triangles: " << globe.vertices.size() / 3 << '\n';
}

#endif // TETRAHEDRAL_GLOBE_IMPL

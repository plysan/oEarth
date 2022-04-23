#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <tiffio.h>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../utils/coord.h"
#include "tetrahedral_globe.h"
#include "common.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../extlibs/stb_image.h"


//TODO macro
float getTexX(glm::dvec3 &vert) {
    float texX = glm::orientedAngle(glm::vec2(-1.0f, 0.0f), glm::normalize(glm::vec2(vert.x, vert.z))) / glm::pi<double>() / 2.0f;
    if (texX < 0) {
        texX += 1;
    }
    return texX;
}

void TetrahedraGlobe::upLevel(TriNode &node) {
    if (node.level > 12) {
        return;
    }
    glm::dvec3 mid_vert_1 = glm::normalize(node.vert_1 + node.vert_2);
    glm::dvec3 mid_vert_2 = glm::normalize(node.vert_2 + node.vert_3);
    glm::dvec3 mid_vert_3 = glm::normalize(node.vert_3 + node.vert_1);
    glm::dvec3 center_pos = glm::normalize(node.vert_1 + node.vert_2 + node.vert_3);
    float cam_dist = glm::distance(center_pos, camOffset);
    float triangle_size = glm::length(node.vert_1 - node.vert_2);
    //std::cout << "cam dist: " << cam_dist << " tri_size: " << triangle_size << '\n';
    //std::cout << "center: " << glm::to_string(center_pos) << " camOffset: " << glm::to_string(camOffset) << '\n';
    //TODO
    if (node.level > 2 && cam_dist > triangle_size && cam_dist/triangle_size > 30.0f) {
        return;
    }
    glm::vec2 mid_tex_1, mid_tex_2, mid_tex_3;
    mid_tex_2.x = getTexX(mid_vert_2);
    mid_tex_2.y = glm::angle(mid_vert_2, glm::dvec3(0.0, -1.0, 0.0)) / glm::pi<double>();
    if (node.is_pole) {
        mid_tex_1.x = node.tex_2.x;
        mid_tex_1.y = (node.tex_1.y + node.tex_2.y) / 2.0f;
        mid_tex_3.x = node.tex_3.x;
        mid_tex_3.y = mid_tex_1.y;
    } else {
        if (node.tex_1.x == node.tex_2.x) {
            mid_tex_1.x = node.tex_2.x;
        } else {
            mid_tex_1.x = getTexX(mid_vert_1);
        }
        if (node.tex_1.x == node.tex_3.x) {
            mid_tex_3.x = node.tex_3.x;
        } else {
            mid_tex_3.x = getTexX(mid_vert_3);
        }
        mid_tex_1.y = glm::angle(mid_vert_1, glm::dvec3(0.0, -1.0, 0.0)) / glm::pi<double>();
        mid_tex_3.y = glm::angle(mid_vert_3, glm::dvec3(0.0, -1.0, 0.0)) / glm::pi<double>();
    }

    vert_cur += 12;
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

void TetrahedraGlobe::loadDem(DemSource &source) {
    TIFF* tif = TIFFOpen("textures/dem.tif", "r");
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &source.w);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &source.h);
    tmsize_t strip_size = TIFFStripSize(tif);
    uint32_t strips = TIFFNumberOfStrips(tif);
    std::cout << "DEM dimension: (" << std::dec << source.w << ", " << source.h << ") " << strip_size << " * " << strips << '\n';
    char* data = (char*)_TIFFmalloc(source.w * source.h * 2);
    for (int strip=0; strip<strips; strip++) {
        TIFFReadEncodedStrip(tif, strip, data+strip*strip_size, (tsize_t) -1);
    }
    source.data = (int16_t*)data;
    static int samples_x = 40, samples_y = 10;
    int delta_x = source.w / samples_x, delta_y = source.h / samples_y;
    for (int y = 0; y < source.h; y += delta_y) {
        for (int x = 0; x < source.w; x += delta_x) {
            std::cout << (source.data[y * source.w + x] > 0 ? '*' : ' ');
        }
        std::cout << '\n';
    }
    TIFFClose(tif);
    return;
}

short TetrahedraGlobe::readDem(DemSource &source, glm::vec2 &coord) {
    uint16_t x = (uint16_t)(coord.x * (source.w - 1));
    uint16_t y = (uint16_t)(coord.y * (source.h - 1));
    return source.data[source.w * y + x];
}

void TetrahedraGlobe::collect(TriNode& node) {
    double hFactor = 6371000.0;
    if (node.child.empty()) {
        vertices.push_back({lvec3(node.vert_1 * ((hFactor + readDem(dem_source, node.tex_1)) / hFactor) - camOffset), node.tex_1});
        vertices.push_back({lvec3(node.vert_2 * ((hFactor + readDem(dem_source, node.tex_2)) / hFactor) - camOffset), node.tex_2});
        vertices.push_back({lvec3(node.vert_3 * ((hFactor + readDem(dem_source, node.tex_3)) / hFactor) - camOffset), node.tex_3});
        return;
    } else {
        for (auto &child_node : node.child) {
            collect(child_node);
        }
    }
}

void TetrahedraGlobe::genGlobe(glm::dvec3 camPos) {
    vertices.clear();
    camOffset = camPos;
    clock_t begin = clock();
    TriNode root;
    root.parent = NULL;
    loadDem(dem_source);
    int comp;
    if (texture.data == NULL) {
        stbi_uc* pixels = stbi_load("textures/texture.jpg", &texture.w, &texture.h, &comp, STBI_rgb_alpha);
        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }
        std::cout << "Read texture dimension (" << std::dec << texture.w << ',' << texture.h << ")\n";
        texture.data = pixels;
    }

    // Based on vulkan coordinate system.
    // North pole at (0, -1, 0)
    // lat:0;lng:0 at (1, 0, 0)
    std::vector<TriNode> nodes = {
        {true, 0, {0.0, -1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}, {0.125f, 0.0f}, {0.0f, 0.5f}, {0.25f, 0.5f}},
        {true, 0, {0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}, {1.0, 0.0, 0.0}, {0.375f, 0.0f}, {0.25f, 0.5f}, {0.5f, 0.5f}},
        {true, 0, {0.0, -1.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.625f, 0.0f}, {0.5f, 0.5f}, {0.75f, 0.5f}},
        {true, 0, {0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0}, {0.875f, 0.0f}, {0.75f, 0.5f}, {1.0f, 0.5f}},
        {true, 0, {0.0, 1.0, 0.0}, {0.0, 0.0, -1.0}, {-1.0, 0.0, 0.0}, {0.125f, 1.0f}, {0.25f, 0.5f}, {0.0f, 0.5f}},
        {true, 0, {0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}, {0.375f, 1.0f}, {0.5f, 0.5f}, {0.25f, 0.5f}},
        {true, 0, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}, {0.625f, 1.0f}, {0.75f, 0.5f}, {0.5f, 0.5f}},
        {true, 0, {0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.875f, 1.0f}, {1.0f, 0.5f}, {0.75f, 0.5f}},
    };
    vert_cur = 24;
    for (auto &node : nodes) {
        upLevel(node);
    }
    for (auto &node : nodes) {
        collect(node);
    }
    _TIFFfree(dem_source.data);
    std::cout << "Execution time: " << (double)(clock() - begin)/CLOCKS_PER_SEC << "s, vertices: " << vertices.size() << '\n';
}

#include <string>
#include <iostream>
#include <iomanip>
#include <map>
#include <sstream>
#include <thread>
#include <sys/stat.h>
#include <tiffio.h>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/common.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../vars.h"
#include "../utils/coord.h"
#include "globe.h"
#include "common.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../extlibs/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../extlibs/stb_image_write.h"

bool debug_mega_texture = false;
bool debug_dem = false;

const std::string IMG_JPG = ".jpg";
const std::string IMG_TIF = ".tif";

static std::map<std::string, ImageSource*> image_cache;
static std::vector<TriNode*> texture_nodes;

//TODO macro
float getTexX(glm::dvec3 &vert) {
    float texX = glm::orientedAngle(glm::vec2(-1.0f, 0.0f), glm::normalize(glm::vec2(vert.x, vert.z))) / glm::pi<double>() / 2.0f;
    if (texX < 0) {
        texX += 1;
    }
    return texX;
}

void loadDem(std::string &fname, ImageSource &source) {
    TIFF* tif = TIFFOpen(fname.c_str(), "r");
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &source.w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &source.h);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &source.comp);
    source.comp /= 8;
    tmsize_t strip_size = TIFFStripSize(tif);
    uint32_t strips = TIFFNumberOfStrips(tif);
    std::cout << "DEM:" << fname << ", dimension: (" << std::dec << source.w << " X " << source.h << ") strip: " << strip_size << " * " << strips << '\n';
    char* data = (char*)_TIFFmalloc(source.w * source.h * source.comp);
    for (int strip=0; strip<strips; strip++) {
        TIFFReadEncodedStrip(tif, strip, data+strip*strip_size, (tsize_t) -1);
    }
    source.data = data;
    if (debug_dem) {
        float *data_f = (float*)data;
        static int samples_x = 40, samples_y = 10;
        int delta_x = source.w / samples_x, delta_y = source.h / samples_y;
        for (int y = 0; y < source.h; y += delta_y) {
            for (int x = 0; x < source.w; x += delta_x) {
                std::cout << (data_f[y * source.w + x] > 0 ? '*' : ' ');
            }
            std::cout << '\n';
        }
    }
    TIFFClose(tif);
    return;
}

inline bool ends_with(std::string const &value, std::string const &ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

ImageSource* getImgSource(std::string &key, bool *cached) {
    if (image_cache.find(key) == image_cache.end()) {
        if (cached != NULL) *cached = false;
        ImageSource *source = new ImageSource();
        if (ends_with(key, IMG_TIF)) {
            loadDem(key, *source);
        } else if (ends_with(key, IMG_JPG)) {
            int comp;
            source->data = (char*)stbi_load(key.c_str(), &source->w, &source->h, &comp, STBI_rgb_alpha);
        }
        if (source->data == NULL) {
            throw std::runtime_error("error reading image source: " + key + ".");
        }
        image_cache[key] = source;
        return source;
    }
    if (cached != NULL) *cached = true;
    return image_cache.find(key)->second;
}

void fillSubTex(TriNode &node) {
    if (node.child.empty()) {
        return;
    }
    glm::vec2 mid_tex_2 = (node.tex_2 + node.tex_3) / 2.0f;
    glm::vec2 mid_tex_1, mid_tex_3;
    if (node.is_pole) {
        mid_tex_1 = glm::vec2(node.tex_2.x, (node.tex_1.y + node.tex_2.y) / 2.0f);
        mid_tex_3 = glm::vec2(node.tex_3.x, mid_tex_1.y);
    } else {
        mid_tex_1 = (node.tex_1 + node.tex_2) / 2.0f;
        mid_tex_3 = (node.tex_3 + node.tex_1) / 2.0f;
    }
    if (node.child[0].tex_source_0.empty()) {
        node.child[0].tex_1 = node.tex_1;
        node.child[0].tex_2 = mid_tex_1;
        node.child[0].tex_3 = mid_tex_3;
    }
    if (node.child[1].tex_source_0.empty()) {
        node.child[1].tex_1 = mid_tex_1;
        node.child[1].tex_2 = node.tex_2;
        node.child[1].tex_3 = mid_tex_2;
    }
    if (node.child[2].tex_source_0.empty()) {
        node.child[2].tex_1 = mid_tex_3;
        node.child[2].tex_2 = mid_tex_2;
        node.child[2].tex_3 = node.tex_3;
    }
    if (node.child[3].tex_source_0.empty()) {
        node.child[3].tex_1 = mid_tex_2;
        node.child[3].tex_2 = mid_tex_3;
        node.child[3].tex_3 = mid_tex_1;
    }
    for (auto &child_node : node.child) {
        fillSubTex(child_node);
    }
}

bool testImageSource(int level, double latT, double lngL, std::string &file_name, const std::string &img_type) {
    struct stat buf;
    std::stringstream file_name_s;
    file_name_s << "textures/" << std::setprecision(16) << level << '_' << latT << '_' << lngL << img_type;
    file_name = file_name_s.str();
    if (stat(file_name_s.str().c_str(), &buf) == 0) {
        return true;
    }
    return false;
}

float getNodeHeight(TriNode &node, glm::dvec2 &coord) {
    static double earth_r_meter = 6371000.0;
    ImageSource *source = NULL;
    if (node.dem_source_1 != NULL && node.dem_source_1->tl_coord.y < coord.y) {
        source = node.dem_source_1;
    } else if (node.dem_source_0 != NULL) {
        source = node.dem_source_0;
    }
    if (source != NULL) {
        uint16_t x = (uint16_t)((coord.y - source->tl_coord.y) / source->span.y * (source->w - 1));
        uint16_t y = (uint16_t)((-coord.x + source->tl_coord.x) / source->span.x * (source->h - 1));
        float* data = (float*)source->data;
        return data[source->w * y + x] / earth_r_meter;
    }
    return -0.0001f;
}

void TetrahedraGlobe::upLevel(TriNode &node) {
    if (node.level > 18){
        return;
    }
    glm::dvec3 center_pos = (node.vert_1 + node.vert_2 + node.vert_3) / 3.0;
    float cam_dist = glm::distance(center_pos, camOffset);
    float triangle_size = glm::length(node.vert_1 - center_pos);
    if (node.level > 2 && cam_dist/triangle_size > 30.0f) {
        return;
    }

    double node_span_lat = node.coord_1.x - node.coord_2.x;
    double node_span_lng = node.coord_2.y - node.coord_3.y;
    double node_start_lat = node_span_lat > 0 ? node.coord_1.x : node.coord_2.x;
    double node_start_lng = node_span_lng > 0 ? node.coord_3.y : node.coord_2.y;
    bool span_node = abs(node_span_lng / node_span_lat) > 1.1;
    float dem_span_lat = abs(node_span_lat);
    float dem_span_lng = span_node ? abs(node_span_lng / 2) : abs(node_span_lng);

    std::string dem_source_key_0, dem_source_key_1;
    bool file_exists, cached;
    file_exists = testImageSource(node.level, node_start_lat, node_start_lng, dem_source_key_0, IMG_TIF);
    if (span_node) {
        file_exists &= testImageSource(node.level, node_start_lat, node_start_lng + dem_span_lng, dem_source_key_1, IMG_TIF);
    }
    if(file_exists) {
        node.dem_source_0 = getImgSource(dem_source_key_0, &cached);
        if (!cached) {
            node.dem_source_0->tl_coord = glm::vec2(node_start_lat, node_start_lng);
            node.dem_source_0->span = glm::vec2(dem_span_lat, dem_span_lng);
        }
        if (span_node) {
            node.dem_source_1 = getImgSource(dem_source_key_1, &cached);
            if (!cached) {
                node.dem_source_1->tl_coord = glm::vec2(node_start_lat, node_start_lng + dem_span_lng);
                node.dem_source_1->span = glm::vec2(dem_span_lat, dem_span_lng);
            }
        }
    }

    std::string source0, source1;
    file_exists = testImageSource(node.level, node_start_lat, node_start_lng, source0, IMG_JPG);
    if (span_node) {
        file_exists &= testImageSource(node.level, node_start_lat, node_start_lng + abs(node_span_lng / 2), source1, IMG_JPG);
    }
    if (file_exists) {
        node.tex_source_0 = source0;
        node.tex_source_1 = source1;
        texture_nodes.push_back(&node);
    }

    glm::dvec2 mid_coord_1, mid_coord_3;
    glm::dvec2 mid_coord_2 = (node.coord_2 + node.coord_3) / 2.0;
    if (node.is_pole) {
        mid_coord_1 = glm::vec2((node.coord_1.x + node.coord_2.x) / 2, node.coord_2.y);
        mid_coord_3 = glm::vec2(mid_coord_1.x, node.coord_3.y);
    } else {
        mid_coord_1 = (node.coord_1 + node.coord_2) / 2.0;
        mid_coord_3 = (node.coord_3 + node.coord_1) / 2.0;
    }
    glm::dvec3 mid_vert_1 = coord2DPos(mid_coord_1.x, mid_coord_1.y, getNodeHeight(node, mid_coord_1));
    glm::dvec3 mid_vert_2 = coord2DPos(mid_coord_2.x, mid_coord_2.y, getNodeHeight(node, mid_coord_2));
    glm::dvec3 mid_vert_3 = coord2DPos(mid_coord_3.x, mid_coord_3.y, getNodeHeight(node, mid_coord_3));

    node.child.push_back({node.is_pole ? true : false, node.level + 1, node.dem_source_0, node.dem_source_1,
            node.vert_1, mid_vert_1, mid_vert_3,
            node.coord_1, mid_coord_1, mid_coord_3});
    node.child.push_back({false, node.level + 1, node.dem_source_0, node.dem_source_1,
            mid_vert_1, node.vert_2, mid_vert_2,
            mid_coord_1, node.coord_2, mid_coord_2});
    node.child.push_back({false, node.level + 1, node.dem_source_0, node.dem_source_1,
            mid_vert_3, mid_vert_2, node.vert_3,
            mid_coord_3, mid_coord_2, node.coord_3});
    node.child.push_back({false, node.level + 1, node.dem_source_0, node.dem_source_1,
            mid_vert_2, mid_vert_3, mid_vert_1,
            mid_coord_2, mid_coord_3, mid_coord_1});
    for (auto &child_node : node.child) {
        upLevel(child_node);
    }
}

void TetrahedraGlobe::collect(TriNode& node) {
    if (node.child.empty()) {
        glm::vec3 normal = glm::normalize(glm::cross(node.vert_2 - node.vert_1, node.vert_3 - node.vert_1));
        vertices.push_back({lvec3(node.vert_1 - camOffset), normal, node.tex_1});
        vertices.push_back({lvec3(node.vert_2 - camOffset), normal, node.tex_2});
        vertices.push_back({lvec3(node.vert_3 - camOffset), normal, node.tex_3});
        return;
    } else {
        for (auto &child_node : node.child) {
            collect(child_node);
        }
    }
}

static int tex_node_size = 1024;
static int pixel_size = 4;
static int max_nodes_side = 4;
static int mega_row_size = tex_node_size * max_nodes_side;
static int node_row_size = max_nodes_side * tex_node_size * tex_node_size;
static int node_last_row_delta = node_row_size - mega_row_size;
static int node_last_row_start = tex_node_size * (tex_node_size - 1);

int dst0_idx_offset_iter(int row)           {return mega_row_size;}
int dst1_idx_offset_iter(int row)           {return 1 - mega_row_size;}
int src0_upleft_idx_offset_iter(int row)    {return -tex_node_size;}
int src0_downleft_idx_offset_iter(int row)  {return 1 + tex_node_size;}
int src0_upright_idx_offset_iter(int row)   {return 1 - tex_node_size;}
int src0_downright_idx_offset_iter(int row) {return tex_node_size;}
int src1_up_idx_offset_iter(int row)        {return -tex_node_size;}
int src1_down_idx_offset_iter(int row)      {return tex_node_size;}

bool advanceTexIdx(int &row, int &column, int &sub) {
    if (sub == 0) {
        sub++;
    } else {
        sub = 0;
        column++;
        if (column > max_nodes_side - 1) {
            column = 0;
            row++;
            if (row > max_nodes_side - 1) {
                return false;
            }
        }
    }
    return true;
}

bool advanceTexReverseIdx(int &row, int &column) {
    column--;
    if (column < 0) {
        column = max_nodes_side - 1;
        row--;
        if (row < 0) {
            return false;
        }
    }
    return true;
}

void TetrahedraGlobe::setTexLayout(std::vector<TriNode> &nodes) {
    if (mega_texture.data == NULL) {
        mega_texture.w = mega_texture.h = mega_row_size;
        mega_texture.data = new char[(int)pow(mega_row_size, 2) * pixel_size];
    }
    int tex_row=0, tex_column=0, tex_sub = 0;
    int tex_row_reverse = max_nodes_side - 1, tex_column_reverse = max_nodes_side - 1;

    std::string globle_file;
    if (testImageSource(0, 90.0, -180.0, globle_file, IMG_JPG)) {
        ImageSource *globe = getImgSource(globle_file, NULL);
        int *dst = (int *)mega_texture.data + (max_nodes_side - 1) * node_row_size + (max_nodes_side - 2) * tex_node_size;
        int *src = (int *)globe->data;
        int globe_row_size = tex_node_size * 2;
        for (int i = 0; i < tex_node_size; i++) {
            memcpy(dst, src, globe_row_size * pixel_size);
            dst += mega_row_size;
            src += globe_row_size;
        }
        if (!advanceTexReverseIdx(tex_row_reverse, tex_column_reverse) || !advanceTexReverseIdx(tex_row_reverse, tex_column_reverse)) {
            std::cout << "Too many texture nodes(0)\n";
            return;
        }
        glm::vec2 globe_tex_start = glm::vec2((max_nodes_side - 2.0) / max_nodes_side, (max_nodes_side - 1.0) / max_nodes_side);
        for (auto &node : nodes) {
            node.tex_1 = glm::vec2(globe_tex_start.x + (node.coord_1.y + 180.0) / 360.0 / max_nodes_side * 2, globe_tex_start.y + (90.0 - node.coord_1.x) / 180.0 / max_nodes_side);
            node.tex_2 = glm::vec2(globe_tex_start.x + (node.coord_2.y + 180.0) / 360.0 / max_nodes_side * 2, globe_tex_start.y + (90.0 - node.coord_2.x) / 180.0 / max_nodes_side);
            node.tex_3 = glm::vec2(globe_tex_start.x + (node.coord_3.y + 180.0) / 360.0 / max_nodes_side * 2, globe_tex_start.y + (90.0 - node.coord_3.x) / 180.0 / max_nodes_side);
        }
    }

    for (TriNode *node : texture_nodes) {
        bool node_up = node->coord_1.x > node->coord_2.x;
        if (node->is_pole) {
            if (tex_row_reverse == tex_row && tex_column_reverse < tex_column) {
                std::cout << "Too many texture nodes(1)\n";
                break;
            }
            ImageSource *source0 = getImgSource(node->tex_source_0, NULL);
            int *src = (int *)source0->data;
            int *dst = (int *)mega_texture.data + node_row_size * tex_row_reverse + tex_node_size * tex_column_reverse;
            for (int row = 0; row < tex_node_size; row++) {
                memcpy(dst, src, tex_node_size * pixel_size);
                src += tex_node_size;
                dst += mega_row_size;
            }
            if (node_up) {
                node->tex_1 = glm::vec2((float)(tex_column_reverse + 0.5) / max_nodes_side, (float)tex_row_reverse / max_nodes_side);
                node->tex_2 = glm::vec2((float)tex_column_reverse / max_nodes_side, (float)(tex_row_reverse + 1) / max_nodes_side);
                node->tex_3 = glm::vec2((float)(tex_column_reverse + 1) / max_nodes_side, (float)(tex_row_reverse + 1) / max_nodes_side);
            } else {
                node->tex_1 = glm::vec2((float)(tex_column_reverse + 0.5) / max_nodes_side, (float)(tex_row_reverse + 1) / max_nodes_side);
                node->tex_2 = glm::vec2((float)(tex_column_reverse + 1) / max_nodes_side, (float)tex_row_reverse / max_nodes_side);
                node->tex_3 = glm::vec2((float)tex_column_reverse / max_nodes_side, (float)tex_row_reverse / max_nodes_side);
            }
            if (!advanceTexReverseIdx(tex_row_reverse, tex_column_reverse)) {
                std::cout << "Too many texture nodes(2)\n";
                break;
            }
            continue;
        }

        if (tex_row_reverse < tex_row || tex_row_reverse == tex_row && tex_column > tex_column_reverse) {
            std::cout << "Too many texture nodes(3)\n";
            break;
        }

        bool span_node = abs((node->coord_2.y - node->coord_3.y) / (node->coord_1.x - node->coord_2.x)) > 1.1;
        bool node_left = node->coord_1.y == node->coord_2.y;
        bool tex_flip = node_up ^ tex_sub;

        int *dst0_idx_start = (int *)mega_texture.data + tex_row * node_row_size + tex_column * tex_node_size + (tex_sub ? node_last_row_delta : 0);
        int (*dst_idx_offset_iter)(int) = tex_sub ? &dst1_idx_offset_iter : &dst0_idx_offset_iter;

        ImageSource *source0 = getImgSource(node->tex_source_0, NULL);
        int *src0_idx_start = (int *)source0->data + (node_up ? node_last_row_start : 0);
        int (*src0_idx_offset_iter)(int) = node_up ? !node_left || span_node ? &src0_upright_idx_offset_iter : &src0_upleft_idx_offset_iter
                                                   : node_left || span_node ? &src0_downleft_idx_offset_iter : &src0_downright_idx_offset_iter;
        ImageSource *source1;
        int *src1_idx_start;
        int (*src1_idx_offset_iter)(int);
        if (span_node) {
            source1 = getImgSource(node->tex_source_1, NULL);
            src1_idx_start = (int *)source1->data + (node_up ? node_last_row_start : 0);
            src1_idx_offset_iter = node_up ? &src1_up_idx_offset_iter : &src1_down_idx_offset_iter;
        }

        for (int dst_row_iter = 0; dst_row_iter < tex_node_size; dst_row_iter++) {
            if (span_node) {
                int *dst0_idx_start_mid = dst0_idx_start + (tex_node_size - dst_row_iter) / 2;
                src1_idx_start += src1_idx_offset_iter(dst_row_iter);
                for (int src_idx = 0; src_idx < tex_node_size - dst_row_iter; src_idx += 2) {
                    int dst_idx = src_idx >> 1;
                    *(dst0_idx_start + dst_idx) = *(src0_idx_start + src_idx);
                    *(dst0_idx_start_mid + dst_idx) = *(src1_idx_start + src_idx);
                }
            } else {
                memcpy(dst0_idx_start, src0_idx_start, (tex_node_size - dst_row_iter) * pixel_size);
            }
            dst0_idx_start += dst_idx_offset_iter(dst_row_iter);
            src0_idx_start += src0_idx_offset_iter(dst_row_iter);
        }

        if (tex_sub) {
            node->tex_1 = glm::vec2((float)(tex_column + 1) / max_nodes_side, (float)tex_row / max_nodes_side);
            node->tex_2 = glm::vec2((float)tex_column / max_nodes_side, (float)(tex_row + 1) / max_nodes_side);
            node->tex_3 = glm::vec2((float)(tex_column + 1) / max_nodes_side, (float)(tex_row + 1) / max_nodes_side);
        } else {
            node->tex_1 = glm::vec2((float)tex_column / max_nodes_side, (float)(tex_row + 1) / max_nodes_side);
            node->tex_2 = glm::vec2((float)(tex_column + 1) / max_nodes_side, (float)tex_row / max_nodes_side);
            node->tex_3 = glm::vec2((float)tex_column / max_nodes_side, (float)tex_row / max_nodes_side);
        }
        if (tex_flip) {
            glm::vec2 tmp = node->tex_2;
            node->tex_2 = node->tex_3;
            node->tex_3 = tmp;
        }

        if (!advanceTexIdx(tex_row, tex_column, tex_sub)) {
            std::cout << "Too many texture nodes(4)\n";
            break;
        }
    }
    if (debug_mega_texture) {
        std::string outfile = "mega_texutre.jpg";
        int r = stbi_write_jpg(outfile.c_str(), mega_texture.w, mega_texture.h, 4, mega_texture.data, 90);
        std::cout << "exported mega texure: " << mega_texture.w << ',' << mega_texture.h << ", size:" << texture_nodes.size() << '\n';
    }
}

void TetrahedraGlobe::genGlobe(glm::dvec3 camPos) {
    vertices.clear();
    camOffset = camPos;
    clock_t begin = clock();
    TriNode root;
    root.parent = NULL;

    // Based on vulkan coordinate system.
    // North pole at (0, -1, 0)
    // lat:0;lng:0 at (1, 0, 0)
    std::vector<TriNode> nodes = {
        {true, 1, NULL, NULL, {0.0, -1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}, {90.0, -135.0}, {0.0, -180.0}, {0.0, -90.0}},
        {true, 1, NULL, NULL, {0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}, {1.0, 0.0, 0.0},  {90.0, -45.0}, {0.0, -90.0}, {0.0, 0.0}},
        {true, 1, NULL, NULL, {0.0, -1.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0},   {90.0, 45.0}, {0.0, 0.0}, {0.0, 90.0}},
        {true, 1, NULL, NULL, {0.0, -1.0, 0.0}, {0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0},  {90.0, 135.0}, {0.0, 90.0}, {0.0, 180.0}},
        {true, 1, NULL, NULL, {0.0, 1.0, 0.0}, {0.0, 0.0, -1.0}, {-1.0, 0.0, 0.0},  {-90.0, -135.0}, {0.0, -90.0}, {0.0, -180.0}},
        {true, 1, NULL, NULL, {0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0},   {-90.0, -45.0}, {0.0, 0.0}, {0.0, -90.0}},
        {true, 1, NULL, NULL, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0},    {-90.0, 45.0}, {0.0, 90.0}, {0.0, 0.0}},
        {true, 1, NULL, NULL, {0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, 1.0},   {-90.0, 135.0}, {0.0, 180.0}, {0.0, 90.0}},
    };
    texture_nodes.clear();
    for (auto &node : nodes) {
        upLevel(node);
    }
    setTexLayout(nodes);
    for (auto &node : nodes) {
        fillSubTex(node);
    }
    for (auto &node : nodes) {
        collect(node);
    }
    std::cout << "Execution time: " << (double)(clock() - begin)/CLOCKS_PER_SEC << "s, vertices: " << vertices.size() << '\n';
}

void fillBathymetry(glm::vec2 levelBl, int level, glm::vec2 dstBl, glm::vec2 dstTr, glm::vec2 dstUvBl, glm::vec2 dstUvTr, std::vector<float> &dstData, int dstDataSize) {
    std::string srcDataStr;
    glm::vec2 srcLevelBl = levelBl;
    int srcLevel = level;
    float levelSize;
    while(srcLevel > 0 && !testImageSource(srcLevel, srcLevelBl.x + levelSize, srcLevelBl.y, srcDataStr, IMG_TIF)) {
        srcLevel--;
        levelSize = 90.0 / glm::pow(2, srcLevel - 1);
        srcLevelBl = srcLevelBl - glm::fmod(srcLevelBl, levelSize);
    }
    if (srcLevel == 0) {
        std::cout << "canot find bathy" <<'\n';
        return;
    }
    ImageSource *srcImg = getImgSource(srcDataStr, NULL);
    float *srcData = (float*)srcImg->data;
    glm::ivec2 srcDataIdxSize = glm::ivec2(srcImg->h - 1, srcImg->w - 1);
    glm::vec2 dstCoordSize = dstTr - dstBl;
    glm::ivec2 dataIdxBl = (glm::ivec2)((dstUvBl - dstBl) / dstCoordSize * (float)(dstDataSize - 1));
    glm::ivec2 dataIdxTr = (glm::ivec2)((dstUvTr - dstBl) / dstCoordSize * (float)(dstDataSize - 1));
    glm::ivec2 srcIdx = (glm::ivec2)((dstUvBl - srcLevelBl) / levelSize * (glm::vec2)srcDataIdxSize);
    glm::vec2 srcIdxDel = (glm::dvec2)dstCoordSize / (dstDataSize - 1.0) / ((double)levelSize / (glm::dvec2)srcDataIdxSize);
    glm::vec2 srcIdxCur = srcIdx;
    for (int v = dataIdxBl.x; v <= dataIdxTr.x; v++) {
        for (int u = dataIdxBl.y; u <= dataIdxTr.y; u++) {
            glm::vec2 srcOf = glm::vec2(srcDataIdxSize.x - srcIdxCur.x, srcIdxCur.y);
            glm::ivec2 srcOi = glm::ivec2((int)srcOf.x, (int)srcOf.y);
            glm::vec2 srcPropo = srcOf - (glm::vec2)srcOi;
            float srcVal = (
                srcData[srcOi.x * srcImg->w + srcOi.y] * (2 - srcPropo.x - srcPropo.y)
              + srcData[glm::min(srcDataIdxSize.x, srcOi.x + 1) * srcImg->w + srcOi.y] * srcPropo.x
              + srcData[srcOi.x * srcImg->w + glm::min(srcDataIdxSize.y, srcOi.y + 1)] * srcPropo.y
            ) / 2;
            if (std::isnan(srcVal)) srcVal = 0.0f;
            dstData[(v * dstDataSize + u) * 4] = glm::clamp(srcVal + waterDeepM, 0.0f, waterDeepM);
            srcIdxCur.y += srcIdxDel.y;
        }
        srcIdxCur.x += srcIdxDel.x;
        srcIdxCur.y = srcIdx.y;
    }
}

void fillBathymetry(glm::vec2 dstBl, glm::vec2 dstTr, std::vector<float> &dstData, int dstDataSize) {
    glm::vec2 dstBlAbs = dstBl + glm::vec2(90.0f, 180.0f);
    int level = glm::ceil(glm::log2(90.0 / (dstTr.y - dstBl.y)) + 1);
    float levelSize = 90.0 / glm::pow(2, level - 1);
    glm::vec2 levelBl = dstBlAbs - glm::fmod(dstBlAbs, levelSize) - glm::vec2(90.0f, 180.0f);
    glm::vec2 levelTr = levelBl + glm::vec2(levelSize);
    fillBathymetry(levelBl, level, dstBl, dstTr, dstBl, levelTr, dstData, dstDataSize);
    bool spanLat = levelTr.x < dstTr.x;
    bool spanLng = levelTr.y < dstTr.y;
    if (spanLat) {
        fillBathymetry(glm::vec2(levelBl.x + levelSize, levelBl.y), level, dstBl, dstTr, glm::vec2(levelTr.x, dstBl.y), glm::vec2(dstTr.x, levelTr.y), dstData, dstDataSize);
    }
    if (spanLng) {
        fillBathymetry(glm::vec2(levelBl.x, levelBl.y + levelSize), level, dstBl, dstTr, glm::vec2(dstBl.x, levelTr.y), glm::vec2(levelTr.x, dstTr.y), dstData, dstDataSize);
    }
    if (spanLat && spanLng) {
        fillBathymetry(levelTr, level, dstBl, dstTr, levelTr, dstTr, dstData, dstDataSize);
    }
    if (debug_dem) {
        printf("Bathymetry:\n");
        for (int j = dstDataSize-1; j >= 0; j-=80) {
            for (int k = 0; k < dstDataSize; k+=30) {
                float h = dstData[(dstDataSize * j + k) * 4];
                std::cout << (h == 1 ? '*' : h == 0 ? '_' : '.');
            }
            printf("\n");
        }
    }
}

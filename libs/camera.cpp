#include <iostream>
#include <chrono>
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <GLFW/glfw3.h>

#include "../utils/coord.h"
#include "common.h"
#include "camera.h"


static const float FOV_MIN = 20.0f;
static const float FOV_RANGE = 100.0f;

glm::vec3 getCoveredViewFromAbove(Camera &camera) {
    glm::vec3 pos_above = camera.pos * 1.01;
    glm::vec3 earth_center_n = -glm::normalize(camera.pos);
    glm::vec3 camera_dor_n = glm::normalize(camera.dir);
    return pos_above;
}

void rotateCamera(Camera &camera, GLFWwindow *window) {
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float timeDelta = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
    lastTime = currentTime;

    camera.pos = glm::rotate(camera.pos, -timeDelta * glm::radians(1.0), glm::dvec3(0.0, 0.0, 1.0));
    camera.dir = glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - lvec3(camera.pos));
    camera.up = glm::vec3(0.0f, 0.0f, 1.0f);
    camera.proj = glm::perspective(glm::radians(camera.fov), camera.aspect, 0.1f, 10.0f);
    camera.proj[1][1] *= -1;
}

void freeCamera(Camera &camera, GLFWwindow *window) {
    static bool cursorDidabled = false;
    if (!cursorDidabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        cursorDidabled = true;
    }
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float timeDelta = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
    lastTime = currentTime;

    double xpos, ypos;
    int windowW, windowH;
    glfwGetWindowSize(window, &windowW, &windowH);
    glfwGetCursorPos(window, &xpos, &ypos);
    static float rotateFactor = 0.01f;
    static float aceleration = 0.00003f;
    float dx = (windowW/2 - xpos) * rotateFactor, dy = (windowH/2 - ypos) * rotateFactor;
    glm::vec3 up = glm::normalize(lvec3(camera.pos));
    camera.dir = glm::rotate(camera.dir, dx, up);
    glm::vec3 tmpAxis = glm::cross(camera.dir, up);
    camera.dir = glm::normalize(glm::rotate(camera.dir, dy, tmpAxis));
    glm::vec3 right = glm::cross(camera.dir, up);
    float vlctDelta = aceleration * timeDelta;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS){
        camera.vlct += camera.dir * vlctDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS){
        camera.vlct -= camera.dir * vlctDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS){
        camera.vlct += right * vlctDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS){
        camera.vlct -= right * vlctDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS){
        camera.vlct += up * vlctDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS){
        camera.vlct -= up * vlctDelta;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS){
        camera.vlct = glm::vec3(0.0f);
    }

    camera.pos = camera.pos + hvec3(camera.vlct * timeDelta);
    camera.up = glm::normalize(camera.pos);
    float height = glm::length(camera.pos) - earthRadius;
    camera.proj = glm::perspective(glm::radians(camera.fov), camera.aspect, glm::max(0.000001f, height / 100), 2.0f);
    camera.proj[1][1] *= -1;

    glfwSetCursorPos(window, windowW/2, windowH/2);
}

double getDirLenFromPos(double height, double cos_down_dir) {
    if (cos_down_dir < 0) return 999999.9f;
    double sin_down_dir = glm::sqrt(1 - glm::pow(cos_down_dir, 2));
    double d = earthRadius / sin_down_dir;
    double sin_b = (earthRadius + height) / d;
    if (sin_b > 1) return 999999.9f;
    double rad_b = glm::pi<double>() - glm::asin(sin_b);
    double rad_dd = glm::acos(cos_down_dir);
    double rad_a = glm::pi<double>() - rad_b - rad_dd;
    return glm::sin(rad_a) * d;
}

static const int RES_H = 100;
static const float H_MIN = 0.000001f;
static const float H_RANGE = 1.0f;
static const int RES_RAD = 250;
static const int RES_FOV = 20;
struct InvParam {
    float rad_fov_inv;
    float rad_down_di;

    InvParam operator + (const InvParam &obj) {
        InvParam tmp = {
            rad_fov_inv + obj.rad_fov_inv,
            rad_down_di + obj.rad_down_di,
        };
        return tmp;
    }

    InvParam operator - (const InvParam &obj) {
        InvParam tmp = {
            rad_fov_inv - obj.rad_fov_inv,
            rad_down_di - obj.rad_down_di,
        };
        return tmp;
    }

    InvParam operator / (const int div) {
        InvParam tmp = {
            rad_fov_inv / div,
            rad_down_di / div,
        };
        return tmp;
    }

    InvParam operator * (const float fac) {
        InvParam tmp = {
            rad_fov_inv * fac,
            rad_down_di * fac,
        };
        return tmp;
    }
};
static InvParam inv_map[RES_H][RES_RAD][RES_FOV];

float getInvDeltaH(float h) {
    return glm::max(0.00001f, h);
}

InvParam getInvParam(float height, float rad_down_dir, float fov) {
    glm::vec3 axi = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 down = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 dir = glm::rotate(down, rad_down_dir, axi);
    glm::vec3 dir_btm = glm::rotate(dir, -glm::radians(fov/2), axi);
    glm::vec3 dir_top = glm::rotate(dir, glm::radians(fov/2), axi);
    double cos_down_db = glm::dot(down, dir_btm);
    double cos_down_dt = glm::dot(down, dir_top);
    double len_dir_btm = getDirLenFromPos(height, cos_down_db);
    double len_dir_top = getDirLenFromPos(height, cos_down_dt);
    glm::vec3 pos_cam = glm::vec3(0.0f, height, 0.0f);
    glm::vec3 lookat = pos_cam + dir_btm * (float)len_dir_btm;
    float d_h = getInvDeltaH(height);
    glm::vec3 pos_cam_inv = pos_cam + up * (float)d_h;
    float h_inv = height + d_h;
    glm::vec3 dir_inv_btm = glm::normalize(lookat - pos_cam_inv);

    glm::vec3 dir_inv_top = glm::rotate(dir_inv_btm, glm::radians(fov), axi);
    double cos_down_dit = glm::dot(down, dir_inv_top);
    double len_dit = getDirLenFromPos(h_inv, cos_down_dit);
    double hlen_dt = glm::sqrt(glm::pow(len_dir_top, 2) - glm::pow(cos_down_dt * len_dir_top, 2));
    double hlen_dit = glm::sqrt(glm::pow(len_dit, 2) - glm::pow(cos_down_dit * len_dit, 2));
    if (hlen_dt > hlen_dit && len_dit < earthRadius) {
        double cos_down_dit_shear = glm::sqrt(glm::pow(height+earthRadius, 2) - glm::pow(earthRadius, 2)) / (height+earthRadius);
        if (len_dir_top < earthRadius) {
            glm::vec3 pos_dt = pos_cam + dir_top * (float)len_dir_top;
            dir_inv_top = glm::normalize(pos_dt - pos_cam_inv);
        } else {
            double rad_shear = glm::acos(cos_down_dit_shear);
            dir_inv_top = glm::rotate(down, (float)rad_shear, axi);
        }
    }
	float rad_fov_inv = glm::acos(glm::dot(dir_inv_top, dir_inv_btm));
    float rad_down_di = glm::acos(glm::dot(down, dir_inv_top)) - rad_fov_inv/2;
    InvParam inv_param = {rad_fov_inv, rad_down_di};
    return inv_param;
}

InvParam getInvParamCache(float height, float rad_down_dir, float fov) {
    float key_h = glm::pow(glm::max(0.0f, height - H_MIN), 0.2) / H_RANGE * RES_H;
    int btm_key_h = (int)key_h;
    float del_key_h = key_h - btm_key_h;
    float key_rad = rad_down_dir / glm::pi<float>() * RES_RAD;
    int btm_key_rad = (int)key_rad;
    float del_key_rad = key_rad - btm_key_rad;
    float key_fov = (fov - FOV_MIN) / FOV_RANGE * RES_FOV;
    int btm_key_fov = (int)key_fov;
    float del_key_fov = key_fov - btm_key_fov;
    InvParam del_h = inv_map[btm_key_h+1][btm_key_rad][btm_key_fov] - inv_map[btm_key_h][btm_key_rad][btm_key_fov];
    InvParam del_rad = inv_map[btm_key_h][btm_key_rad+1][btm_key_fov] - inv_map[btm_key_h][btm_key_rad][btm_key_fov];
    InvParam del_fov = inv_map[btm_key_h][btm_key_rad][btm_key_fov+1] - inv_map[btm_key_h][btm_key_rad][btm_key_fov];
    return inv_map[btm_key_h][btm_key_rad][btm_key_fov] + del_h * del_key_h + del_rad * del_key_rad + del_fov * del_key_fov;
}

void initInvParam() {
    for (int i = 0; i < RES_H; i++) {
        float h = glm::pow(i * H_RANGE / RES_H, 5) + H_MIN;
        for (int j = 0; j < RES_RAD; j++) {
            float rad = j * glm::pi<float>() / RES_RAD;
            for (int k = 0; k < RES_FOV; k++) {
                float fov = FOV_MIN + k * (FOV_RANGE / RES_FOV);
                inv_map[i][j][k] = getInvParam(h, rad, fov);
            }
        }
    }
}

void Camera::getPVInv(float &h_inv, glm::mat4 &p_inv, glm::mat4 &v_inv, glm::dvec3 offset) {
    float rad_down_dir = glm::acos(glm::dot(-up, dir));
    double height = glm::length(pos) - earthRadius;
    InvParam inv_param = getInvParamCache(height, rad_down_dir, fov);
    h_inv = height + getInvDeltaH(height);
    glm::vec3 pos_cam = lvec3(pos - offset);
    glm::vec3 pos_cam_inv = pos_cam + up * getInvDeltaH(height);
    glm::vec axi = glm::cross(up, dir);
    glm::vec3 dir_inv = glm::rotate(-up, -inv_param.rad_down_di, axi);
    p_inv = glm::inverse(glm::perspective((float)inv_param.rad_fov_inv, aspect * 1.07f, glm::max(0.000001f, h_inv / 100), 2.0f));
    v_inv = glm::inverse(glm::lookAt(pos_cam_inv, pos_cam_inv + dir_inv, up));
}

void updateCamera(Camera &camera, GLFWwindow *window) {
    freeCamera(camera, window);
}

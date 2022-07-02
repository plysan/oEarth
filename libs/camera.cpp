#include <iostream>
#include <chrono>
#include <vector>
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
    static float aceleration = 0.0001f;
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

void Camera::getPVInv(float &h_inv, glm::mat4 &p_inv, glm::mat4 &v_inv, glm::dvec3 offset) {
    glm::vec3 axi = glm::cross(up, dir);
    glm::vec3 dir_btm = glm::rotate(dir, glm::radians(fov/2), axi);
    glm::vec3 dir_top = glm::rotate(dir, -glm::radians(fov/2), axi);
    double cos_down_db = glm::dot(-up, dir_btm);
    double cos_down_dt = glm::dot(-up, dir_top);
    double height = glm::length(pos) - earthRadius;
    double len_dir_btm = getDirLenFromPos(height, cos_down_db);
    double len_dir_top = getDirLenFromPos(height, cos_down_dt);
    glm::vec3 pos_cam = lvec3(pos - offset);
    glm::vec3 lookat = pos_cam + dir_btm * (float)len_dir_btm;
    double d_h = height * 2;
    glm::vec3 pos_cam_inv = pos_cam + up * (float)d_h;
    h_inv = height + d_h;
    glm::vec3 dir_inv_btm = glm::normalize(lookat - pos_cam_inv);

    glm::vec3 dir_inv_top = glm::rotate(dir_inv_btm, -glm::radians(fov), axi);
    double cos_down_dit = glm::dot(-up, dir_inv_top);
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
            dir_inv_top = glm::rotate(-up, -(float)rad_shear, axi);
        }
    }
	double cos_fov_inv = glm::dot(dir_inv_top, dir_inv_btm);

    double rad_fov_inv = glm::acos(cos_fov_inv);
    glm::vec3 dir_inv = glm::rotate(dir_inv_top, (float)rad_fov_inv / 2, axi);
    p_inv = glm::inverse(glm::perspective((float)rad_fov_inv, aspect * 1.07f, glm::max(0.000001f, h_inv / 100), 2.0f));
    v_inv = glm::inverse(glm::lookAt(pos_cam_inv, pos_cam_inv + dir_inv, up));
}

void updateCamera(Camera &camera, GLFWwindow *window) {
    freeCamera(camera, window);
}

#include <iostream>
#include <chrono>
#include <vector>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <GLFW/glfw3.h>
#include "utils.h"
#include "common.h"


void rotateCamera(Camera &camera, GLFWwindow *window) {
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float timeDelta = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
    lastTime = currentTime;

    camera.pos = glm::rotate(camera.pos, -timeDelta * glm::radians(1.0), glm::dvec3(0.0, 0.0, 1.0));
    camera.dir = glm::vec3(0.0f, 0.0f, 0.0f) - lvec3(camera.pos);
    camera.up = glm::vec3(0.0f, 0.0f, 1.0f);
    camera.proj = glm::perspective(glm::radians(45.0f), camera.aspect, 0.1f, 10.0f);
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
    static float aceleration = 0.1f;
    float dx = (windowW/2 - xpos) * rotateFactor, dy = (windowH/2 - ypos) * rotateFactor;
    glm::vec3 up = glm::normalize(lvec3(camera.pos));
    camera.dir = glm::rotate(camera.dir, dx, up);
    glm::vec3 tmpAxis = glm::cross(camera.dir, up);
    camera.dir = glm::rotate(camera.dir, dy, tmpAxis);
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
    camera.proj = glm::perspective(45.0f, camera.aspect, 0.000001f, 100.0f);
    camera.proj[1][1] *= -1;

    glfwSetCursorPos(window, windowW/2, windowH/2);
}

void updateCamera(Camera &camera, GLFWwindow *window) {
    freeCamera(camera, window);
}

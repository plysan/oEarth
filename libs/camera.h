#ifndef CAMERA_H
#define CAMERA_H

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "../vars.h"

struct Camera {
    glm::dvec3 pos;
    glm::vec3 dir;
    glm::vec3 up;
    glm::mat4 proj;
    glm::vec3 vlct;
    float aspect;
    float fov;
    void getPVInv(float &height_inv, glm::mat4 &p_inv, glm::mat4 &v_inv, glm::dvec3 offset);
};

void updateCamera(Camera &camera, GLFWwindow *window);
void initInvParam();

#endif //CAMERA_H

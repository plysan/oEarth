#include <iostream>
#include <GLFW/glfw3.h>

#include "control.h"

Control control {
    .dt = 0.01,
};

void updateControl(int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;
    switch (key) {
        case GLFW_KEY_LEFT_BRACKET:
            if (control.dt == 0.0) control.dt = 0.0005;
            else control.dt /= 0.8;
            break;
        case GLFW_KEY_O:
            control.dt *= 0.8; break;
        case GLFW_KEY_P:
            if (control.dt == 0.0) {
                control.dt = control.dtPause;
            } else {
                control.dtPause = control.dt;
                control.dt = 0.0;
            }
            break;
    }
}

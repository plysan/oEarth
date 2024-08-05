#ifndef CONTROL_H
#define CONTROL_H

struct Control {
    float dt;
    float dtPause;
};

extern Control control;

void updateControl(int key, int scancode, int action, int mods);

#endif //CONTROL_H

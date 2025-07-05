#include <stdlib.h>
#include <time.h>
#include "turret.h"

int main() {
    srand(time(NULL));
    Turret env = {0};
    env.observations = (float *)calloc(2, sizeof(float));
    env.actions = (int *)calloc(3, sizeof(int));
    env.rewards = (float *)calloc(1, sizeof(float));
    env.terminals = (unsigned char *)calloc(1, sizeof(unsigned char));

    c_reset(&env);
    c_render(&env);

    while (!WindowShouldClose()) {
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            env.actions[0] = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
            env.actions[1] = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
            env.actions[2] = IsKeyDown(KEY_SPACE);
        } else {
            env.actions[0] = rand() % 2;
            env.actions[1] = rand() % 2;
            env.actions[2] = rand() % 2;
        }

        c_step(&env);
        c_render(&env);

        if (env.terminals[0]) {
            c_reset(&env);
        }
    }

    free(env.observations);
    free(env.actions);
    free(env.rewards);
    free(env.terminals);
    c_close(&env);
    return 0;
} 
#include "raylib.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 640
#define HEIGHT 480
#define GROUND_Y (HEIGHT - 40)
#define MAX_BULLETS 10
#define GRAVITY 0.2f
#define MAX_CHARGE 100.0f
#define CHARGE_RATE 4.0f
#define CANNON_ROTATE_SPEED 4.0f
#define TARGET_RADIUS 12
#define OBS_SIZE 12

const Color PUFF_RED = (Color){187, 0, 0, 255};
const Color PUFF_CYAN = (Color){0, 187, 187, 255};
const Color PUFF_WHITE = (Color){241, 241, 241, 241};
const Color PUFF_BACKGROUND = (Color){6, 24, 24, 255};

#pragma once

typedef struct {
    float score;
    float episode_return;
    float episode_length;
    float hit_rate;
    float waste_rate;
    float n;
} Log;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    int active;
} Bullet;

typedef struct {
    Vector2 position;
    float vx;
    int active;
} Target;

typedef struct {
    Log log;
    float *observations;
    int *actions;
    float *rewards;
    unsigned char *terminals;
    float angle;
    float charge;
    int is_charging;
    int tick;
    int shots_fired;
    int hits;
    int wasted_shots;
    Bullet bullets[MAX_BULLETS];
    Target targets[MAX_BULLETS];
} Turret;

static inline float get_distance(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

static inline float clamp(float v, float mi, float ma) { 
    return fmaxf(mi, fminf(ma, v)); 
}

int count_active_targets(Turret *env) {
    int count = 0;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (env->targets[i].active) count++;
    }
    return count;
}

void reset(Turret *env) {
    env->angle = -90;
    env->charge = 0;
    env->is_charging = 0;
    env->tick = 0;
    env->shots_fired = 0;
    env->hits = 0;
    env->wasted_shots = 0;

    for (int i = 0; i < MAX_BULLETS; i++) {
        env->bullets[i].active = 0;
        env->targets[i].active = 0;
    }
}

void spawn_bullet(Turret *env) {
    if (env->charge < 10)
        return;

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!env->bullets[i].active) {
            Bullet *bullet = &env->bullets[i];
            bullet->active = 1;

            Vector2 base = {WIDTH / 2, GROUND_Y};
            float rad = env->angle * PI / 180;
            Vector2 tip = {base.x + cosf(rad) * 30, base.y + sinf(rad) * 30};
            bullet->position = tip;

            float power = env->charge * 0.4f;
            bullet->velocity.x = cosf(rad) * power;
            bullet->velocity.y = sinf(rad) * power;
            env->charge = 0;
            env->shots_fired++;
            break;
        }
    }
}

void step_bullets(Turret *env) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!env->bullets[i].active)
            continue;

        Bullet *bullet = &env->bullets[i];
        bullet->position.x += bullet->velocity.x;
        bullet->position.y += bullet->velocity.y;
        bullet->velocity.y += GRAVITY;

        if (bullet->position.y >= GROUND_Y || 
            bullet->position.x < 0 || bullet->position.x > WIDTH ||
            bullet->position.y < 0) {
            bullet->active = 0;
            env->wasted_shots++;
            env->rewards[0] -= 0.05f;
        }
    }
}

void spawn_target_random(Turret *env) {
    if (rand() % 60 != 0)
        return;

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!env->targets[i].active) {
            Target *target = &env->targets[i];
            int from_left = rand() % 2;

            target->position.y = rand() % (HEIGHT - 200) + 60;
            target->position.x = from_left ? -TARGET_RADIUS : WIDTH + TARGET_RADIUS;
            target->vx = from_left ? (1.5f + (rand() % 100) / 100.0f) : -(1.5f + (rand() % 100) / 100.0f);
            target->active = 1;
            break;
        }
    }
}

void step_targets(Turret *env) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!env->targets[i].active)
            continue;

        Target *target = &env->targets[i];
        target->position.x += target->vx;

        if (target->position.x < -TARGET_RADIUS || 
            target->position.x > WIDTH + TARGET_RADIUS) {
            target->active = 0;
        }
    }
}

void check_collisions(Turret *env) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!env->bullets[i].active)
            continue;

        Bullet *bullet = &env->bullets[i];
        for (int j = 0; j < MAX_BULLETS; j++) {
            if (!env->targets[j].active)
                continue;

            Target *target = &env->targets[j];
            if (get_distance(bullet->position, target->position) < TARGET_RADIUS) {
                bullet->active = 0;
                target->active = 0;
                env->rewards[0] += 1.0f;
                env->hits++;
                env->log.score += 1;
                break;
            }
        }
    }
}

void compute_observations(Turret *env) {
    int obs_idx = 0;
    
    // Basic cannon state (3 dimensions)
    env->observations[obs_idx++] = env->angle / 90.0f;
    env->observations[obs_idx++] = env->charge / MAX_CHARGE;
    env->observations[obs_idx++] = env->is_charging ? 1.0f : 0.0f;
    
    // Target count (1 dimension)
    env->observations[obs_idx++] = count_active_targets(env) / 5.0f;
    
    // Find nearest target (5 dimensions)
    int nearest_target_idx = -1;
    float nearest_distance = 999999.0f;
    Vector2 cannon_pos = {WIDTH / 2, GROUND_Y};
    
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (env->targets[i].active) {
            float dist = get_distance(cannon_pos, env->targets[i].position);
            if (dist < nearest_distance) {
                nearest_distance = dist;
                nearest_target_idx = i;
            }
        }
    }
    
    if (nearest_target_idx >= 0) {
        Target *target = &env->targets[nearest_target_idx];
        env->observations[obs_idx++] = (target->position.x - WIDTH/2) / (WIDTH/2);
        env->observations[obs_idx++] = (target->position.y - HEIGHT/2) / (HEIGHT/2);
        env->observations[obs_idx++] = target->vx / 3.0f;
        env->observations[obs_idx++] = clamp(nearest_distance / 400.0f, 0.0f, 1.0f);
        
        // Angle to target
        float target_angle = atan2f(target->position.y - GROUND_Y, target->position.x - WIDTH/2) * 180.0f / PI;
        float angle_diff = target_angle - env->angle;
        while (angle_diff > 180) angle_diff -= 360;
        while (angle_diff < -180) angle_diff += 360;
        env->observations[obs_idx++] = angle_diff / 180.0f;
    } else {
        for (int i = 0; i < 5; i++) {
            env->observations[obs_idx++] = 0.0f;
        }
    }
    
    // Bullet count (1 dimension)
    int bullet_count = 0;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (env->bullets[i].active) bullet_count++;
    }
    env->observations[obs_idx++] = bullet_count / 5.0f;
    
    // Performance metrics (2 dimensions)
    env->observations[obs_idx++] = env->shots_fired > 0 ? (float)env->hits / env->shots_fired : 0.0f;
    env->observations[obs_idx++] = env->shots_fired > 0 ? (float)env->wasted_shots / env->shots_fired : 0.0f;
}

void c_reset(Turret *env) {
    reset(env);
    compute_observations(env);
}

void c_step(Turret *env) {
    env->rewards[0] = 0;
    env->terminals[0] = 0;
    env->tick++;

    // Handle actions
    if (env->actions[0]) {
        env->angle -= CANNON_ROTATE_SPEED;
        if (env->angle < -180)
            env->angle = -180;
    }
    if (env->actions[1]) {
        env->angle += CANNON_ROTATE_SPEED;
        if (env->angle > 0)
            env->angle = 0;
    }
    if (env->actions[2]) {
        env->is_charging = 1;
        if (env->charge < MAX_CHARGE)
            env->charge += CHARGE_RATE;
    } else {
        if (env->is_charging) {
            spawn_bullet(env);
            env->is_charging = 0;
        }
    }

    // Update game state
    step_bullets(env);
    step_targets(env);
    spawn_target_random(env);
    check_collisions(env);
    

    if (count_active_targets(env) > 0 && !env->is_charging && env->charge < 50) {
        env->rewards[0] -= 0.005f;  // Small penalty for being passive with targets
    }
    
    compute_observations(env);

    // Episode termination
    if (env->tick >= 2400) {
        env->terminals[0] = 1;
        env->log.episode_return += env->rewards[0];
        env->log.episode_length += env->tick;
        env->log.hit_rate = env->shots_fired > 0 ? (float)env->hits / env->shots_fired : 0.0f;
        env->log.waste_rate = env->shots_fired > 0 ? (float)env->wasted_shots / env->shots_fired : 0.0f;
        env->log.n += 1;
        c_reset(env);
    }
}

void c_render(Turret *env) {
    if (!IsWindowReady()) {
        InitWindow(WIDTH, HEIGHT, "Turret");
        SetTargetFPS(60);
    }

    if (IsKeyDown(KEY_ESCAPE)) {
        exit(0);
    }

    BeginDrawing();
    ClearBackground(PUFF_BACKGROUND);

    // Draw ground
    DrawLine(0, GROUND_Y, WIDTH, GROUND_Y, PUFF_CYAN);
    
    // Draw HUD
    DrawText(TextFormat("Score: %d", env->hits), 10, 10, 20, PUFF_WHITE);
    DrawText(TextFormat("Shots: %d", env->shots_fired), 10, 35, 20, PUFF_WHITE);
    DrawText(TextFormat("Wasted: %d", env->wasted_shots), 10, 60, 20, PUFF_WHITE);
    DrawText(TextFormat("Targets: %d", count_active_targets(env)), 10, 85, 20, PUFF_WHITE);
    DrawText(TextFormat("Charge: %.0f", env->charge), 10, 110, 20, PUFF_WHITE);

    // Draw cannon
    Vector2 base = {WIDTH / 2, GROUND_Y};
    float rad = env->angle * PI / 180;
    Vector2 tip = {base.x + cosf(rad) * 30, base.y + sinf(rad) * 30};
    DrawLineEx(base, tip, 6, PUFF_CYAN);

    // Draw targets
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (env->targets[i].active) {
            DrawCircle((int)env->targets[i].position.x,
                       (int)env->targets[i].position.y, TARGET_RADIUS, PUFF_RED);
        }
    }

    // Draw bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (env->bullets[i].active) {
            DrawCircle((int)env->bullets[i].position.x, (int)env->bullets[i].position.y, 3,
                       PUFF_WHITE);
        }
    }

    EndDrawing();
}

void c_close(Turret *env) {
    if (IsWindowReady()) {
        CloseWindow();
    }
}
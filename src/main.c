#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#define SCREEN_W 960.0f
#define SCREEN_H 544.0f

typedef struct {
    float x, y, w, h;
} Rect;

static int intersects(Rect a, Rect b) {
    return a.x < b.x + b.w &&
           a.x + a.w > b.x &&
           a.y < b.y + b.h &&
           a.y + a.h > b.y;
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float axis_from_stick(unsigned char value) {
    int centered = (int)value - 128;
    if (abs(centered) < 18) return 0.0f;
    return centered / 127.0f;
}

int main(void) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    vita2d_init();
    vita2d_set_clear_color(RGBA8(20, 22, 30, 255));

    vita2d_pgf *font = vita2d_load_default_pgf();

    Rect player = {120, 240, 42, 42};
    Rect pickup = {720, 250, 30, 30};
    Rect enemy = {500, 160, 52, 52};

    float enemy_vx = 3.0f;
    float enemy_vy = 2.2f;

    int score = 0;
    int dead = 0;
    int running = 1;

    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));

    while (running) {
        sceCtrlPeekBufferPositive(0, &pad, 1);

        if (pad.buttons & SCE_CTRL_START)
            running = 0;

        if (!dead) {
            float mx = axis_from_stick(pad.lx);
            float my = axis_from_stick(pad.ly);

            player.x += mx * 5.5f;
            player.y += my * 5.5f;

            player.x = clampf(player.x, 0, SCREEN_W - player.w);
            player.y = clampf(player.y, 0, SCREEN_H - player.h);

            enemy.x += enemy_vx;
            enemy.y += enemy_vy;

            if (enemy.x <= 0 || enemy.x + enemy.w >= SCREEN_W)
                enemy_vx *= -1;
            if (enemy.y <= 0 || enemy.y + enemy.h >= SCREEN_H)
                enemy_vy *= -1;

            if (intersects(player, pickup)) {
                score++;
                pickup.x = 50 + rand() % 830;
                pickup.y = 70 + rand() % 410;

                if (score % 3 == 0) {
                    enemy_vx *= 1.12f;
                    enemy_vy *= 1.12f;
                }
            }

            if (intersects(player, enemy))
                dead = 1;
        } else if (pad.buttons & SCE_CTRL_CROSS) {
            player.x = 120;
            player.y = 240;
            pickup.x = 720;
            pickup.y = 250;
            enemy.x = 500;
            enemy.y = 160;
            enemy_vx = 3.0f;
            enemy_vy = 2.2f;
            score = 0;
            dead = 0;
        }

        vita2d_start_drawing();
        vita2d_clear_screen();

        vita2d_draw_rectangle(player.x, player.y, player.w, player.h, RGBA8(90, 180, 255, 255));
        vita2d_draw_rectangle(pickup.x, pickup.y, pickup.w, pickup.h, RGBA8(255, 220, 60, 255));
        vita2d_draw_rectangle(enemy.x, enemy.y, enemy.w, enemy.h, RGBA8(240, 70, 80, 255));

        char score_text[64];
        snprintf(score_text, sizeof(score_text), "Score: %d", score);
        vita2d_pgf_draw_text(font, 24, 38, RGBA8(255, 255, 255, 255), 1.0f, score_text);

        vita2d_pgf_draw_text(font, 24, 520, RGBA8(170, 175, 190, 255), 0.8f,
                            "Left stick: move   START: quit");

        if (dead) {
            vita2d_draw_rectangle(230, 185, 500, 170, RGBA8(0, 0, 0, 210));
            vita2d_pgf_draw_text(font, 365, 245, RGBA8(255, 255, 255, 255), 1.4f, "YOU DIED");
            vita2d_pgf_draw_text(font, 310, 300, RGBA8(255, 255, 255, 255), 1.0f, "Press X to restart");
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    vita2d_free_pgf(font);
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}

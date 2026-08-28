#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#include "assets_b64.h"

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

static int b64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static unsigned char *decode_base64(const char *src, size_t *out_len) {
    size_t len = strlen(src);
    size_t cap = (len * 3) / 4 + 4;
    unsigned char *out = malloc(cap);
    if (!out) return NULL;

    size_t oi = 0;
    int val = 0;
    int bits = -8;

    for (size_t i = 0; i < len; ++i) {
        if (src[i] == '=') break;
        int v = b64_value(src[i]);
        if (v < 0) continue;
        val = (val << 6) | v;
        bits += 6;
        if (bits >= 0) {
            out[oi++] = (unsigned char)((val >> bits) & 0xFF);
            bits -= 8;
        }
    }

    *out_len = oi;
    return out;
}

static void draw_texture_in_rect(vita2d_texture *tex, Rect r) {
    if (!tex) return;
    float tw = (float)vita2d_texture_get_width(tex);
    float th = (float)vita2d_texture_get_height(tex);
    vita2d_draw_texture_scale(tex, r.x, r.y, r.w / tw, r.h / th);
}

int main(void) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    vita2d_init();
    vita2d_set_clear_color(RGBA8(20, 22, 30, 255));

    vita2d_pgf *font = vita2d_load_default_pgf();

    size_t player_png_len = 0;
    size_t enemy_png_len = 0;
    unsigned char *player_png = decode_base64(player_png_b64, &player_png_len);
    unsigned char *enemy_png = decode_base64(enemy_png_b64, &enemy_png_len);

    vita2d_texture *player_tex = player_png ? vita2d_load_PNG_buffer(player_png) : NULL;
    vita2d_texture *enemy_tex = enemy_png ? vita2d_load_PNG_buffer(enemy_png) : NULL;

    free(player_png);
    free(enemy_png);

    Rect player = {120, 240, 52, 52};
    Rect pickup = {720, 250, 30, 30};
    Rect enemy = {500, 160, 64, 64};

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

        if (player_tex)
            draw_texture_in_rect(player_tex, player);
        else
            vita2d_draw_rectangle(player.x, player.y, player.w, player.h, RGBA8(90, 180, 255, 255));

        vita2d_draw_rectangle(pickup.x, pickup.y, pickup.w, pickup.h, RGBA8(255, 220, 60, 255));

        if (enemy_tex)
            draw_texture_in_rect(enemy_tex, enemy);
        else
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

    if (player_tex) vita2d_free_texture(player_tex);
    if (enemy_tex) vita2d_free_texture(enemy_tex);
    vita2d_free_pgf(font);
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}

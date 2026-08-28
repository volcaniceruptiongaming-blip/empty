#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#define SCREEN_W 960.0f
#define SCREEN_H 544.0f
#define WORLD_W 3600.0f
#define GRAVITY 0.72f
#define MOVE_SPEED 4.8f
#define JUMP_SPEED 13.5f

typedef struct {
    float x, y, w, h;
} Rect;

typedef struct {
    Rect r;
    int collected;
} Coin;

typedef struct {
    Rect r;
    float vx;
    float min_x;
    float max_x;
    int alive;
} Enemy;

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

static float stick_x(unsigned char value) {
    int centered = (int)value - 128;
    if (abs(centered) < 18) return 0.0f;
    return centered / 127.0f;
}

static void reset_game(Rect *player, float *vx, float *vy, int *on_ground,
                       int *dead, int *won, int *coins_collected,
                       Coin *coins, int coin_count, Enemy *enemies, int enemy_count) {
    player->x = 120;
    player->y = 360;
    *vx = 0;
    *vy = 0;
    *on_ground = 0;
    *dead = 0;
    *won = 0;
    *coins_collected = 0;

    for (int i = 0; i < coin_count; ++i)
        coins[i].collected = 0;

    for (int i = 0; i < enemy_count; ++i)
        enemies[i].alive = 1;
}

int main(void) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    vita2d_init();
    vita2d_set_clear_color(RGBA8(120, 190, 255, 255));
    vita2d_pgf *font = vita2d_load_default_pgf();

    Rect player = {120, 360, 52, 66};
    float player_vx = 0.0f;
    float player_vy = 0.0f;
    int on_ground = 0;

    Rect platforms[] = {
        {0, 470, 900, 74},
        {980, 470, 520, 74},
        {1600, 470, 720, 74},
        {2420, 470, 1180, 74},
        {420, 385, 180, 28},
        {760, 320, 180, 28},
        {1120, 365, 210, 28},
        {1450, 300, 180, 28},
        {1800, 365, 210, 28},
        {2130, 290, 180, 28},
        {2590, 385, 190, 28},
        {2920, 320, 180, 28},
        {3260, 255, 170, 28}
    };
    const int platform_count = sizeof(platforms) / sizeof(platforms[0]);

    Coin coins[] = {
        {{470, 345, 24, 24}, 0},
        {{820, 280, 24, 24}, 0},
        {{1180, 325, 24, 24}, 0},
        {{1500, 260, 24, 24}, 0},
        {{1880, 325, 24, 24}, 0},
        {{2180, 250, 24, 24}, 0},
        {{2660, 345, 24, 24}, 0},
        {{2980, 280, 24, 24}, 0},
        {{3320, 215, 24, 24}, 0}
    };
    const int coin_count = sizeof(coins) / sizeof(coins[0]);

    Enemy enemies[] = {
        {{680, 428, 48, 42}, 1.6f, 620, 850, 1},
        {{1700, 428, 48, 42}, 1.8f, 1650, 2150, 1},
        {{2760, 428, 48, 42}, 2.0f, 2600, 3200, 1}
    };
    const int enemy_count = sizeof(enemies) / sizeof(enemies[0]);

    Rect goal = {3480, 350, 56, 120};

    int coins_collected = 0;
    int dead = 0;
    int won = 0;
    int intro = 1;
    int running = 1;
    float camera_x = 0.0f;

    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    unsigned int old_buttons = 0;

    while (running) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        unsigned int pressed = pad.buttons & ~old_buttons;

        if (pressed & SCE_CTRL_START)
            running = 0;

        if (intro) {
            if (pressed & SCE_CTRL_CROSS)
                intro = 0;
        } else if (dead || won) {
            if (pressed & SCE_CTRL_CROSS) {
                reset_game(&player, &player_vx, &player_vy, &on_ground,
                           &dead, &won, &coins_collected,
                           coins, coin_count, enemies, enemy_count);
                camera_x = 0;
            }
        } else {
            float input_x = stick_x(pad.lx);
            player_vx = input_x * MOVE_SPEED;

            if ((pressed & SCE_CTRL_CROSS) && on_ground) {
                player_vy = -JUMP_SPEED;
                on_ground = 0;
            }

            player.x += player_vx;
            player.x = clampf(player.x, 0, WORLD_W - player.w);

            for (int i = 0; i < platform_count; ++i) {
                if (intersects(player, platforms[i])) {
                    if (player_vx > 0)
                        player.x = platforms[i].x - player.w;
                    else if (player_vx < 0)
                        player.x = platforms[i].x + platforms[i].w;
                }
            }

            float old_y = player.y;
            player_vy += GRAVITY;
            if (player_vy > 18.0f) player_vy = 18.0f;
            player.y += player_vy;
            on_ground = 0;

            for (int i = 0; i < platform_count; ++i) {
                if (!intersects(player, platforms[i])) continue;

                if (player_vy > 0 && old_y + player.h <= platforms[i].y + 8) {
                    player.y = platforms[i].y - player.h;
                    player_vy = 0;
                    on_ground = 1;
                } else if (player_vy < 0 && old_y >= platforms[i].y + platforms[i].h - 8) {
                    player.y = platforms[i].y + platforms[i].h;
                    player_vy = 0;
                }
            }

            if (player.y > SCREEN_H + 140)
                dead = 1;

            for (int i = 0; i < coin_count; ++i) {
                if (!coins[i].collected && intersects(player, coins[i].r)) {
                    coins[i].collected = 1;
                    coins_collected++;
                }
            }

            for (int i = 0; i < enemy_count; ++i) {
                if (!enemies[i].alive) continue;

                enemies[i].r.x += enemies[i].vx;
                if (enemies[i].r.x <= enemies[i].min_x ||
                    enemies[i].r.x + enemies[i].r.w >= enemies[i].max_x) {
                    enemies[i].vx *= -1;
                }

                if (intersects(player, enemies[i].r)) {
                    float player_bottom_before = old_y + player.h;
                    if (player_vy > 0 && player_bottom_before <= enemies[i].r.y + 12) {
                        enemies[i].alive = 0;
                        player_vy = -9.0f;
                    } else {
                        dead = 1;
                    }
                }
            }

            if (intersects(player, goal) && coins_collected == coin_count)
                won = 1;

            float target_camera = player.x - 300.0f;
            camera_x += (target_camera - camera_x) * 0.12f;
            camera_x = clampf(camera_x, 0, WORLD_W - SCREEN_W);
        }

        vita2d_start_drawing();
        vita2d_clear_screen();

        if (intro) {
            vita2d_draw_rectangle(100, 80, 760, 360, RGBA8(0, 0, 0, 190));
            vita2d_pgf_draw_text(font, 250, 145, RGBA8(255, 255, 255, 255), 1.7f,
                                "PLATFORMER TEST");
            vita2d_pgf_draw_text(font, 220, 220, RGBA8(235, 235, 235, 255), 1.0f,
                                "LEFT STICK: MOVE");
            vita2d_pgf_draw_text(font, 220, 265, RGBA8(235, 235, 235, 255), 1.0f,
                                "X: JUMP / STOMP ENEMIES");
            vita2d_pgf_draw_text(font, 220, 310, RGBA8(255, 225, 80, 255), 1.0f,
                                "COLLECT ALL GOLD AND REACH THE FLAG");
            vita2d_pgf_draw_text(font, 345, 390, RGBA8(200, 215, 235, 255), 1.0f,
                                "PRESS X TO START");
        } else {
            for (int i = 0; i < platform_count; ++i) {
                float sx = platforms[i].x - camera_x;
                if (sx + platforms[i].w < 0 || sx > SCREEN_W) continue;
                vita2d_draw_rectangle(sx, platforms[i].y, platforms[i].w, platforms[i].h,
                                      RGBA8(90, 175, 75, 255));
                vita2d_draw_rectangle(sx, platforms[i].y, platforms[i].w, 8,
                                      RGBA8(65, 125, 55, 255));
            }

            for (int i = 0; i < coin_count; ++i) {
                if (coins[i].collected) continue;
                float sx = coins[i].r.x - camera_x;
                vita2d_draw_rectangle(sx, coins[i].r.y, coins[i].r.w, coins[i].r.h,
                                      RGBA8(255, 220, 45, 255));
            }

            for (int i = 0; i < enemy_count; ++i) {
                if (!enemies[i].alive) continue;
                float sx = enemies[i].r.x - camera_x;
                vita2d_draw_rectangle(sx, enemies[i].r.y, enemies[i].r.w, enemies[i].r.h,
                                      RGBA8(220, 70, 70, 255));
            }

            float gx = goal.x - camera_x;
            vita2d_draw_rectangle(gx, goal.y, 8, goal.h, RGBA8(230, 230, 230, 255));
            vita2d_draw_rectangle(gx + 8, goal.y, 48, 34,
                                  coins_collected == coin_count ? RGBA8(80, 220, 110, 255)
                                                                : RGBA8(130, 130, 130, 255));

            float px = player.x - camera_x;
            vita2d_draw_rectangle(px, player.y, player.w, player.h, RGBA8(70, 105, 235, 255));

            char hud[96];
            snprintf(hud, sizeof(hud), "GOLD %d/%d", coins_collected, coin_count);
            vita2d_pgf_draw_text(font, 24, 38, RGBA8(255, 255, 255, 255), 1.0f, hud);
            vita2d_pgf_draw_text(font, 720, 38, RGBA8(255, 255, 255, 255), 0.8f,
                                "START: quit");

            if (dead) {
                vita2d_draw_rectangle(245, 185, 470, 170, RGBA8(0, 0, 0, 215));
                vita2d_pgf_draw_text(font, 370, 245, RGBA8(255, 100, 100, 255), 1.5f,
                                    "YOU DIED");
                vita2d_pgf_draw_text(font, 325, 305, RGBA8(255, 255, 255, 255), 1.0f,
                                    "PRESS X TO RESTART");
            }

            if (won) {
                vita2d_draw_rectangle(210, 170, 540, 190, RGBA8(0, 0, 0, 215));
                vita2d_pgf_draw_text(font, 315, 235, RGBA8(255, 225, 80, 255), 1.5f,
                                    "LEVEL COMPLETE!");
                vita2d_pgf_draw_text(font, 320, 305, RGBA8(255, 255, 255, 255), 1.0f,
                                    "PRESS X TO PLAY AGAIN");
            }
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
        old_buttons = pad.buttons;
    }

    vita2d_free_pgf(font);
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}

#pragma once
#include <SDL2/SDL.h>

// clang-format off
#define GGIT_MAP_ENUM            \
    X(master,  0x7E, 0xD3, 0x21) \
    X(hotfix,  0xE6, 0x00, 0x00) \
    X(release, 0x00, 0x68, 0xDE) \
    X(bugfix,  0xE6, 0x96, 0x17) \
    X(develop, 0xB8, 0x16, 0xD9) \
    X(feature, 0x34, 0xD3, 0xE5) \
    X(none,    0xEE, 0xEE, 0xEE) \
    /* */
// clang-format on

static SDL_Color const GGIT_COLORS[] = {
#define X(name, r, g, b, ...) { r, g, b, 0xFF },
    GGIT_MAP_ENUM
#undef X
};
static SDL_Color const GGIT_COLOR_VLINE = { 0x9B, 0x9B, 0x9B, 0xFF };
static SDL_Color const GGIT_COLOR_HLINE = { 0xDA, 0xDA, 0xDA, 0xFF };
static SDL_Color const GGIT_COLOR_BORDER = { 0xE3, 0xE3, 0xE3, 0xFF };

static int const ITEM_W = 38 * 0.8;
static int const ITEM_H = 26 * 0.8;
static int const BORDER = 2;
static int const MARGIN_X = 2;
static int const MARGIN_Y = 4;

static int const ITEM_OUTER_W = ITEM_W + BORDER * 2;
static int const ITEM_OUTER_H = ITEM_H + BORDER * 2;
static int const ITEM_BOX_W = ITEM_OUTER_W + MARGIN_X * 2;
static int const ITEM_BOX_H = ITEM_OUTER_H + MARGIN_Y * 2;

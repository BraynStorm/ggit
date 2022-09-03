#pragma once

#include "ggit-graph.h"
#include "ggit-vector.h"

#include <SDL2/SDL_render.h>
#include <SDL2/SDL_ttf.h>

#include <assert.h>

struct ggit_size
{
    int w;
    int h;
};

struct ggit_ui
{
    int screen_w;
    int screen_h;

    int graph_x;
    int graph_y;

    int item_w;
    int item_h;
    int border;
    int margin_x;
    int margin_y;

    SDL_Renderer* renderer;
    TTF_Font* font;

    enum action
    {
        nothing,
        dragging_selection,
        dragging_commit,
    } action;

    /* Selection */
    struct selection
    {
        int start_x;
        int start_y;
        int end_x;
        int end_y;
        int active_commit;
        struct ggit_vector selected_commits;
    } select;

    struct cache
    {
        int compressed_width;
        struct ggit_vector commit_x; /* int */
    } cache;

    /*
    =============
    Windows
    =============
    */
    int active;
    int hit;
};

struct ggit_input
{
    int mouse_x;
    int mouse_y;
    int wheel_y;
    int wheel_x;
    int delta_mouse_x;
    int delta_mouse_y;
    unsigned buttons[5];

    bool is_ctrl_down;
};
/*
===============
    DRAWING
===============
*/
void ggit_ui_draw_arc(
    SDL_Renderer* renderer,
    int x_center,
    int y_center,
    int r,
    float direction_lr,
    float direction_tb
);
void ggit_ui_draw_rect_cut(
    SDL_Renderer* renderer,
    int x0,
    int y0,
    int x1,
    int y1,
    int cut,
    SDL_Color color
);
void ggit_ui_draw_text(
    SDL_Renderer* renderer,
    TTF_Font* font,
    char const* text,
    int x,
    int y,
    struct ggit_size* out_opt_size
);

/*
===============
    WIDGETS
===============
*/
int ggit_ui_button(
    struct ggit_ui* ui,
    struct ggit_input* input,
    char const* text,
    int x,
    int y,
    unsigned color[4]
);


/*
===============
    UTILS
===============
*/
struct ggit_size ggit_ui_size_text(
    SDL_Renderer* renderer,
    TTF_Font* font,
    char const* text
);
static inline bool
point_in_rect(int x0, int y0, int x1, int y1, int mx, int my)
{
    if (x0 > x1) {
        int xt = x0;
        x0 = x1;
        x1 = xt;
    }
    if (y0 > y1) {
        int yt = y0;
        y0 = y1;
        y1 = yt;
    }

    return mx >= x0 && mx <= x1 && my >= y0 && my <= y1;
}
static inline void
set_color(SDL_Renderer* renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

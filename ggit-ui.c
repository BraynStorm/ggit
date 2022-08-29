#include "ggit-ui.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_ttf.h>

#define ARRAY_COUNT(array) sizeof(array) / sizeof((array)[0])

void
ggit_ui_draw_rect_cut(
    SDL_Renderer* renderer,
    int x0,
    int y0,
    int x1,
    int y1,
    int cut,
    SDL_Color color
)
{
    SDL_Vertex vertices[] = {
        // top-left corner
        (SDL_Vertex){ { x0 + cut, y0 }, color } /* top */,
        (SDL_Vertex){ { x0, y0 + cut }, color } /* bottom */,

        // bottom-left corner
        (SDL_Vertex){ { x0, y1 - cut }, color } /* top */,
        (SDL_Vertex){ { x0 + cut, y1 }, color } /* bottom */,

        // bottom-right corner
        (SDL_Vertex){ { x1 - cut + 1, y1 }, color } /* bottom */,
        (SDL_Vertex){ { x1, y1 - cut + 1 }, color } /* top */,

        // top-right corner
        (SDL_Vertex){ { x1, y0 + cut }, color } /* bottom */,
        (SDL_Vertex){ { x1 - cut + 1, y0 }, color } /* top */,
    };
    int const indices[] = {
        // clang-format off
        
        // left side
        1, 2, 3,
        0, 1, 3,
        
        // center
        0, 3, 4,
        0, 4, 7,

        // right side
        7, 4, 6,
        6, 4, 5,

        // clang-format on
    };
    SDL_RenderGeometry(
        renderer,
        NULL,
        vertices,
        ARRAY_COUNT(vertices),
        indices,
        ARRAY_COUNT(indices)
    );
}

void
ggit_ui_draw_arc(
    SDL_Renderer* renderer,
    int x_center,
    int y_center,
    int r,
    float direction_lr,
    float direction_tb
)
{
    float step = -(M_PI / 2.0f);
    SDL_Point points[] = {
        (SDL_Point){
            x_center + r * direction_lr * cosf(step * 0.0f * 0.125f),
            y_center - r * direction_tb * sinf(step * 0.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction_lr * cosf(step * 1.0f * 0.125f),
            y_center - r * direction_tb * sinf(step * 1.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction_lr * cosf(step * 2.0f * 0.125f),
            y_center - r * direction_tb * sinf(step * 2.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction_lr * cosf(step * 3.0f * 0.125f),
            y_center - r * direction_tb * sinf(step * 3.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction_lr * cosf(step * 4.0f * 0.125f),
            y_center - r * direction_tb * sinf(step * 4.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction_lr * cosf(step * 5.0f * 0.125f),
            y_center - r * direction_tb * sinf(step * 5.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction_lr * cosf(step * 6.0f * 0.125f),
            y_center - r * direction_tb * sinf(step * 6.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction_lr * cosf(step * 7.0f * 0.125f),
            y_center - r * direction_tb * sinf(step * 7.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction_lr * cosf(step * 8.0f * 0.125f),
            y_center - r * direction_tb * sinf(step * 8.0f * 0.125f),
        },
    };

    SDL_RenderDrawLines(renderer, points, ARRAY_COUNT(points));
}
void
ggit_ui_draw_text(
    SDL_Renderer* renderer,
    TTF_Font* font,
    char const* text,
    int x,
    int y,
    struct ggit_size* out_opt_size
)
{
    struct ggit_size size;
    if (!out_opt_size)
        out_opt_size = &size;

    SDL_Surface* text_surface = TTF_RenderUTF8_LCD(
        font,
        text,
        (SDL_Color){ 0x05, 0x05, 0x05, 0xFF },
        (SDL_Color){ 220, 220, 220, 0xFF }
    );
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, text_surface);
    TTF_SizeUTF8(font, text, &out_opt_size->w, &out_opt_size->h);
    SDL_RenderCopy(
        renderer,
        texture,
        NULL,
        &(SDL_Rect){
            .x = x,
            .y = y,
            .w = out_opt_size->w,
            .h = out_opt_size->h,
        }
    );
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(text_surface);
}

int
ggit_ui_button(
    struct ggit_ui* ui,
    struct ggit_input* input,
    char const* text,
    int x,
    int y,
    unsigned color[4]
)
{
    struct ggit_size size;
    ggit_ui_draw_text(ui->renderer, ui->font, text, x, y, &size);

    bool hovered = point_in_rect(
        x,
        y,
        x + size.w,
        y + size.h,
        input->mouse_x,
        input->mouse_y
    );

    unsigned lmb = input->buttons[0];
    return hovered && lmb && !(lmb & 1);
}

struct ggit_size
ggit_ui_size_text(SDL_Renderer* renderer, TTF_Font* font, char const* text)
{
    struct ggit_size size;
    TTF_SizeUTF8(font, text, &size.w, &size.h);
    return size;
}

#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_syswm.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "ggit-vector.h"
#include "ggit-graph.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NONEARFAR
#include <Windows.h>

#define ARRAY_COUNT(array) ARRAYSIZE(array)

#include "ggit-ui-settings.h"

struct ggit_ui
{
    int graph_x;
    int graph_y;

    // Sizes.
    int item_w;
    int item_h;
    int border;
    int margin_x;
    int margin_y;
};

struct ggit_input
{
    int mouse_x;
    int mouse_y;
    int wheel_y;
    int wheel_x;
    int delta_mouse_x;
    int delta_mouse_y;
    bool buttons[5];

    bool is_ctrl_down;
};

struct compressed_x
{
    short new_x;
    bool taken;
};
GGIT_GENERATE_VECTOR_GETTERS(struct compressed_x, compressed_x)

/* TODO:
    Use this instead of _popen in ggit_graph_load();
*/
static int
call_git(char const* command_line, struct ggit_vector* stdout_buffer)
{
#define zero \
    {        \
        0    \
    }

    int result = 1;

    HANDLE stdin_pipe_read;
    HANDLE stdin_pipe_write;
    HANDLE stdout_pipe_read;
    HANDLE stdout_pipe_write;
    HANDLE stderr_pipe_read;
    HANDLE stderr_pipe_write;

    /* Create pipes */
    {
        SECURITY_ATTRIBUTES security = {
            .nLength = sizeof(security),
            .bInheritHandle = 1,
        };

        if (!CreatePipe(&stdin_pipe_read, &stdin_pipe_write, &security, 0))
            goto error_stdin;

        if (!CreatePipe(&stdout_pipe_read, &stdout_pipe_write, &security, 0))
            goto error_stdout;

        if (!CreatePipe(&stderr_pipe_read, &stderr_pipe_write, &security, 0))
            goto error_stderr;

        if (!SetHandleInformation(stdin_pipe_write, HANDLE_FLAG_INHERIT, 0)
            || !SetHandleInformation(stdout_pipe_read, HANDLE_FLAG_INHERIT, 0)
            || !SetHandleInformation(stderr_pipe_read, HANDLE_FLAG_INHERIT, 0))
            goto error_set_handle_info;
    }

    char* cmd = calloc(3 + 1 + 1 + strlen(command_line), 1);
    sprintf(cmd, "git %s", command_line);

    PROCESS_INFORMATION proc_info = zero;
    {
        STARTUPINFO startup_info = {
            .cb = sizeof(startup_info),
            .hStdInput = stdin_pipe_read,
            .hStdOutput = stdout_pipe_write,
            .hStdError = stderr_pipe_write,
            .dwFlags = STARTF_USESTDHANDLES,
        };

        if (!CreateProcessA(
                NULL,
                cmd,
                NULL,
                NULL,
                true,
                0,
                NULL,
                NULL,
                &startup_info,
                &proc_info
            ))
            goto error_create_process;

        CloseHandle(proc_info.hThread);
        CloseHandle(stdin_pipe_read);
        CloseHandle(stdout_pipe_write);
        stdin_pipe_read = INVALID_HANDLE_VALUE;
        stdout_pipe_write = INVALID_HANDLE_VALUE;
    }

    /* Consume STDOUT */
    {
        ggit_vector_reserve_more(stdout_buffer, 4096);

        /* TODO:
            Improve this code:
                Wait for the pipe's buffer to fill up or the process to die.
                And then read it.

                This should help:
                https://docs.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-getnamedpipeinfo
        */
        DWORD read;
        while (ReadFile(
            stdout_pipe_read,
            stdout_buffer->data + stdout_buffer->size,
            stdout_buffer->capacity - stdout_buffer->size,
            &read,
            0
        )) {
            if (!read)
                break;

            stdout_buffer->size += read;
            if (stdout_buffer->size == stdout_buffer->capacity)
                ggit_vector_reserve_more(stdout_buffer, 4096);
        }

        /* NOTE: This might be unnecessary. */
        // Wait for it to die.
        WaitForSingleObject(proc_info.hProcess, INFINITE);

        // Get the exit code.
        DWORD exit_code;
        GetExitCodeProcess(proc_info.hProcess, &exit_code);
        result = exit_code;
    }

    // Let it flow down to the cleanup area.

    CloseHandle(proc_info.hProcess);
error_create_process:;
    free(cmd);
error_set_handle_info:;
    CloseHandle(stderr_pipe_write);
    CloseHandle(stderr_pipe_read);
error_stderr:;
    CloseHandle(stdout_pipe_write);
    CloseHandle(stdout_pipe_read);
error_stdout:;
    CloseHandle(stdin_pipe_write);
    CloseHandle(stdin_pipe_read);
error_stdin:;
    return result;
}

static void
draw_rect_cut(
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
        0,
        vertices,
        ARRAY_COUNT(vertices),
        indices,
        ARRAY_COUNT(indices)
    );
}

static void
draw_arc(
    SDL_Renderer* renderer,
    int x_center,
    int y_center,
    int r,
    float direction,
    SDL_Color color
)
{
    float step = -(M_PI / 2.0f);
    SDL_Point points[] = {
        (SDL_Point){
            x_center + r * direction * cosf(step * 0.0f * 0.125f),
            y_center - r * sinf(step * 0.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction * cosf(step * 1.0f * 0.125f),
            y_center - r * sinf(step * 1.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction * cosf(step * 2.0f * 0.125f),
            y_center - r * sinf(step * 2.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction * cosf(step * 3.0f * 0.125f),
            y_center - r * sinf(step * 3.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction * cosf(step * 4.0f * 0.125f),
            y_center - r * sinf(step * 4.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction * cosf(step * 5.0f * 0.125f),
            y_center - r * sinf(step * 5.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction * cosf(step * 6.0f * 0.125f),
            y_center - r * sinf(step * 6.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction * cosf(step * 7.0f * 0.125f),
            y_center - r * sinf(step * 7.0f * 0.125f),
        },
        (SDL_Point){
            x_center + r * direction * cosf(step * 8.0f * 0.125f),
            y_center - r * sinf(step * 8.0f * 0.125f),
        },
    };

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLines(renderer, points, ARRAY_COUNT(points));
}
static void
util_draw_text(
    SDL_Renderer* renderer,
    TTF_Font* font,
    char const* text,
    int x,
    int y,
    int* text_w,
    int* text_h
)
{
    int text_w_dummy;
    if (!text_w)
        text_w = &text_w_dummy;
    int text_h_dummy;
    if (!text_h)
        text_h = &text_h_dummy;

    SDL_Surface* text_surface = TTF_RenderUTF8_LCD(
        font,
        text,
        (SDL_Color){ 0x05, 0x05, 0x05, 0xFF },
        (SDL_Color){ 220, 220, 220, 0xFF }
    );
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, text_surface);
    TTF_SizeUTF8(font, text, text_w, text_h);
    SDL_RenderCopy(
        renderer,
        texture,
        NULL,
        &(SDL_Rect){
            .x = x,
            .y = y,
            .w = *text_w,
            .h = *text_h,
        }
    );
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(text_surface);
}

static void
set_color(SDL_Renderer* renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static bool
ggit_graph_spans_collide(
    struct ggit_column_span const* restrict a,
    struct ggit_column_span const* restrict b
)
{
    int am_min = a->merge_min;
    int am_max = a->merge_max;
    int bm_min = b->merge_min;
    int bm_max = b->merge_max;
    return (am_max >= bm_min && am_max <= bm_max)
           || (am_min >= bm_min && am_min <= bm_max);
}

static int
ggit_graph_commit_screen_column(
    struct ggit_graph* graph,
    struct ggit_vector* compressed_x,
    int commit_index
)
{
    struct ggit_commit_tag const tag = graph->tags[commit_index];
    int const branch = tag.tag[0];
    int index = tag.tag[1];
    struct ggit_special_branch* commit_branch = ggit_vector_ref_special_branch(
        &graph->special_branches,
        branch
    );
    struct ggit_column_span* commit_branch_span = ggit_vector_ref_column_span(
        &commit_branch->spans,
        index
    );

    int column = 0;
    bool stop = false;

    if (commit_branch->growth_direction >= 0)
        for (int i = 0; i < graph->special_branches.size; ++i) {
            struct ggit_special_branch* i_branch = ggit_vector_ref_special_branch(
                &graph->special_branches,
                i
            );
            if (!stop) {
                if (i == branch)
                    stop = true;
                else
                    column += i_branch->instances.size;
            } else if (i_branch->growth_direction < 0)
                column += i_branch->instances.size;
        }
    else
        for (int i = 0; i < graph->special_branches.size; ++i) {
            if (i == branch)
                break;

            struct ggit_special_branch* i_branch = ggit_vector_ref_special_branch(
                &graph->special_branches,
                i
            );
            if (i_branch->growth_direction < 0)
                column += i_branch->instances.size;
        }
    if (commit_branch->growth_direction >= 0) {
        for (int j = 0; j < index; ++j) {
            struct ggit_column_span const* j_span = ggit_vector_ref_column_span(
                &commit_branch->spans,
                j
            );
            if (!ggit_graph_spans_collide(j_span, commit_branch_span)) {
                // Try compress the column.

                bool collides_with_other = false;
                for (int k = j + 1; k < index; ++k) {
                    struct ggit_column_span const* k_span = ggit_vector_ref_column_span(
                        &commit_branch->spans,
                        k
                    );
                    if (ggit_graph_spans_collide(j_span, k_span)) {
                        collides_with_other = true;
                        break;
                    }
                }
                if (!collides_with_other) {
                    index = j;
                    break;
                }
            }
        }
    } else {
    }
    column += index;

    if (compressed_x && compressed_x->size > column) {
        column = ggit_vector_ref_compressed_x(compressed_x, column)->new_x;
    }

    return column;
}
static int
ggit_graph_commit_screen_x_left(struct ggit_ui* ui, int column)
{
    int const item_w = ui->item_w;
    int const item_outer_w = item_w + ui->border * 2;
    int const item_box_w = item_outer_w + ui->margin_x * 2;
    return column * item_box_w;
}
static int
ggit_graph_commit_screen_x_center(struct ggit_ui* ui, int column)
{
    int const item_w = ui->item_w;
    int const item_outer_w = item_w + ui->border * 2;
    int const item_box_w = item_outer_w + ui->margin_x * 2;
    int const commit_x_center = (item_box_w / 2) + column * item_box_w;
    return commit_x_center;
}
static int
ggit_graph_commit_screen_y_top(struct ggit_ui* ui, int commit_index, int graph_height)
{
    int const item_h = ui->item_h;
    int const item_outer_h = item_h + ui->border * 2;
    int const item_box_h = item_outer_h + ui->margin_y * 2;
    int const commit_y_top = item_box_h * (graph_height - 1 - commit_index);
    return commit_y_top;
}
static int
ggit_graph_commit_screen_y_center(
    struct ggit_ui* ui,
    int commit_index,
    int graph_height
)
{
    int const item_h = ui->item_h;
    int const item_outer_h = item_h + ui->border * 2;
    int const item_box_h = item_outer_h + ui->margin_y * 2;
    int const commit_y_top = ggit_graph_commit_screen_y_top(
        ui,
        commit_index,
        graph_height
    );
    int const commit_y_center = (item_box_h / 2) + commit_y_top;
    return commit_y_center;
}
static bool
point_in_rect(int x0, int y0, int x1, int y1, int mx, int my)
{
    assert(x0 >= x1);
    assert(y0 >= y1);

    return mx >= x0 && mx <= x1 && my >= y0 && my <= y1;
}

static void
ggit_ui_draw_graph__connections(
    struct ggit_ui* ui,
    struct ggit_graph* graph,
    int i_from,
    int i_to,
    SDL_Renderer* renderer,
    struct ggit_vector* compressed_x
)
{
    int const graph_x = ui->graph_x;
    int const graph_y = ui->graph_y;
    int const G_HEIGHT = graph->height;
    int const G_WIDTH = graph->width;

    int const ITEM_H = ui->item_h;
    int const ITEM_W = ui->item_w;
    int const BORDER = ui->border;
    int const MARGIN_X = ui->margin_x;
    int const MARGIN_Y = ui->margin_y;

    int const ITEM_OUTER_H = ITEM_W + BORDER * 2;
    int const ITEM_OUTER_W = ITEM_H + BORDER * 2;
    int const ITEM_BOX_H = ITEM_OUTER_W + MARGIN_X * 2;
    int const ITEM_BOX_W = ITEM_OUTER_H + MARGIN_Y * 2;

    set_color(renderer, (SDL_Color){ 0xAA, 0xAA, 0xAA, 0xFF });
    for (int i = i_from; i < i_to; ++i) {
        int const commit_i = G_HEIGHT - 1 - i;
        int const column = ggit_graph_commit_screen_column(
            graph,
            compressed_x,
            commit_i
        );

        int const commit_x_source = ggit_graph_commit_screen_x_left(ui, column);
        int const commit_y_source = ggit_graph_commit_screen_y_center(
            ui,
            commit_i,
            G_HEIGHT
        );
        int const commit_center_source = ggit_graph_commit_screen_x_center(ui, column);

        int const commit_x = graph_x + commit_x_source;
        int const commit_y = graph_y + commit_y_source;
        int const commit_x_center = graph_x + commit_center_source;
        int const commit_y_bottom = commit_y + ITEM_H / 2 + BORDER + MARGIN_Y;

        bool is_merge = graph->parents[commit_i].parent[1] != -1;
        for (int j = 0; j < ARRAY_COUNT(graph->parents->parent); ++j) {
            int parent = graph->parents[commit_i].parent[j];
            if (parent == -1)
                break;
            int const parent_column = ggit_graph_commit_screen_column(
                graph,
                compressed_x,
                parent
            );
            int const parent_x_center = graph_x
                                        + ggit_graph_commit_screen_x_center(
                                            ui,
                                            parent_column
                                        );
            int const parent_y_top = graph_y
                                     + ggit_graph_commit_screen_y_top(
                                         ui,
                                         parent,
                                         G_HEIGHT
                                     );


            if (parent_x_center != commit_x_center) {
                int arc_radius = 5;
                int direction = 1 + (-2) * (parent_x_center > commit_x_center);
                draw_arc(
                    renderer,
                    commit_x_center - arc_radius * direction,
                    commit_y_bottom - arc_radius,
                    arc_radius,
                    direction,
                    (SDL_Color){ 0xAA, 0xAA, 0xAA, 0xFF }
                );
                // Middle - left/right to match parent column
                SDL_RenderDrawLine(
                    renderer,
                    commit_x_center - arc_radius * direction,
                    commit_y_bottom,
                    parent_x_center,
                    commit_y_bottom
                );
            } else {
                // Commit - center to bottom
                int const offset_merge = is_merge * -2;
                SDL_RenderDrawLine(
                    renderer,
                    commit_x_center,
                    commit_y_bottom + offset_merge,
                    commit_x_center,
                    commit_y
                );
            }
            // Middle - down to match parent Top
            SDL_RenderDrawLine(
                renderer,
                parent_x_center,
                commit_y_bottom,
                parent_x_center,
                parent_y_top
            );
            // Parent - center to top.
            SDL_RenderDrawLine(
                renderer,
                parent_x_center,
                parent_y_top,
                parent_x_center,
                parent_y_top + MARGIN_Y
            );
        }
    }
}

static void
ggit_ui_draw_graph__spans(
    struct ggit_ui* ui,
    struct ggit_graph* graph,
    int i_from,
    int i_to,
    SDL_Renderer* renderer,
    struct ggit_vector* compressed_x
)
{
    int const graph_x = ui->graph_x;
    int const graph_y = ui->graph_y;
    int const G_WIDTH = graph->width;
    int const G_HEIGHT = graph->height;

    int const ITEM_W = ui->item_w;
    int const ITEM_H = ui->item_h;
    int const BORDER = ui->border;
    int const MARGIN_X = ui->margin_x;
    int const MARGIN_Y = ui->margin_y;

    int const ITEM_OUTER_W = ITEM_W + BORDER * 2;
    int const ITEM_OUTER_H = ITEM_H + BORDER * 2;
    int const ITEM_BOX_W = ITEM_OUTER_W + MARGIN_X * 2;
    int const ITEM_BOX_H = ITEM_OUTER_H + MARGIN_Y * 2;

    for (int i = i_from; i < i_to; ++i) {
        int const commit_i = G_HEIGHT - 1 - i;
        struct ggit_commit_tag const tags = graph->tags[commit_i];
        int const color_index = min(ARRAY_COUNT(GGIT_COLORS) - 1, tags.tag[0]);
        int const branch = tags.tag[0];
        int const index = tags.tag[1];
        if (branch == -1)
            continue;

        SDL_Color color = GGIT_COLORS[color_index];
        struct ggit_special_branch* commit_branch = ggit_vector_ref_special_branch(
            &graph->special_branches,
            branch
        );
        struct ggit_column_span* commit_branch_span = ggit_vector_ref_column_span(
            &commit_branch->spans,
            index
        );

        int const column = ggit_graph_commit_screen_column(
            graph,
            compressed_x,
            commit_i
        );
        int const commit_x = MARGIN_X + graph_x
                             + ggit_graph_commit_screen_x_left(ui, column);
        int span_y_top = ggit_graph_commit_screen_y_top(
            ui,
            commit_branch_span->merge_max,
            G_HEIGHT
        );
        int span_y_bottom = ggit_graph_commit_screen_y_top(
            ui,
            commit_branch_span->merge_min,
            G_HEIGHT
        );

        /* Use the base color but increase the brightness. */
        color.r = min(255, color.r + 120);
        color.g = min(255, color.g + 120);
        color.b = min(255, color.b + 120);

        draw_rect_cut(
            renderer,
            commit_x,
            span_y_top + BORDER + MARGIN_Y + graph_y,
            commit_x + ITEM_OUTER_W,
            span_y_bottom + ITEM_OUTER_H + graph_y,
            ITEM_OUTER_W / 2,
            color
        );
    }
}

static void
ggit_ui_draw_graph__commit_messages(
    struct ggit_ui* ui,
    struct ggit_graph* graph,
    int i_from,
    int i_to,
    SDL_Renderer* renderer,
    TTF_Font* font,
    int compressed_width,
    struct ggit_vector* compressed_x
)
{
    int const graph_x = ui->graph_x;
    int const graph_y = ui->graph_y;
    int const G_WIDTH = graph->width;
    int const G_HEIGHT = graph->height;

    int const ITEM_W = ui->item_w;
    int const ITEM_H = ui->item_h;
    int const BORDER = ui->border;
    int const MARGIN_X = ui->margin_x;
    int const MARGIN_Y = ui->margin_y;

    int const ITEM_OUTER_W = ITEM_W + BORDER * 2;
    int const ITEM_OUTER_H = ITEM_H + BORDER * 2;
    int const ITEM_BOX_W = ITEM_OUTER_W + MARGIN_X * 2;
    int const ITEM_BOX_H = ITEM_OUTER_H + MARGIN_Y * 2;

    int const text_x = graph_x + compressed_width * ITEM_BOX_W + ITEM_BOX_W / 2;
    for (int i = i_from; i < i_to; ++i) {
        int const commit_i = G_HEIGHT - 1 - i;
        int const commit_y = graph_y
                             + ggit_graph_commit_screen_y_top(ui, commit_i, G_HEIGHT);
        char const* message = graph->messages[commit_i];
        util_draw_text(renderer, font, message, text_x, commit_y, 0, 0);
    }
}
static void
ggit_ui_draw_graph__boxes(
    struct ggit_ui* ui,
    struct ggit_graph* graph,
    int i_from,
    int i_to,
    SDL_Renderer* renderer,
    struct ggit_vector* compressed_x
)
{
    int const graph_x = ui->graph_x;
    int const graph_y = ui->graph_y;
    int const G_WIDTH = graph->width;
    int const G_HEIGHT = graph->height;

    int const ITEM_W = ui->item_w;
    int const ITEM_H = ui->item_h;
    int const BORDER = ui->border;
    int const MARGIN_X = ui->margin_x;
    int const MARGIN_Y = ui->margin_y;

    int const ITEM_OUTER_W = ITEM_W + BORDER * 2;
    int const ITEM_OUTER_H = ITEM_H + BORDER * 2;
    int const ITEM_BOX_W = ITEM_OUTER_W + MARGIN_X * 2;
    int const ITEM_BOX_H = ITEM_OUTER_H + MARGIN_Y * 2;

    for (int i = i_from; i < i_to; ++i) {
        int const commit_i = G_HEIGHT - 1 - i;
        int const tag = graph->tags[commit_i].tag[0];
        int const column = ggit_graph_commit_screen_column(
            graph,
            compressed_x,
            commit_i
        );
        int const commit_x = MARGIN_X + graph_x
                             + ggit_graph_commit_screen_x_left(ui, column);
        int const commit_y = MARGIN_Y + graph_y
                             + ggit_graph_commit_screen_y_top(ui, commit_i, G_HEIGHT);
        bool is_merge = graph->parents[commit_i].parent[1] != -1;

        int const cut = 2 + 2 * is_merge;

        int color_index = min(ARRAY_COUNT(GGIT_COLORS) - 1, tag);
        SDL_Color color = GGIT_COLORS[color_index];
        draw_rect_cut(
            renderer,
            commit_x + BORDER,
            commit_y + BORDER,
            commit_x + BORDER + ITEM_W,
            commit_y + BORDER + ITEM_H,
            cut,
            color
        );
    }
}
static int
ggit_ui_graph__generate_compressed_x(
    struct ggit_graph* graph,
    int i_from,
    int i_to,
    struct ggit_vector* compressed_x
)
{
    int const g_width = graph->width;
    int const g_height = graph->height;

    if (compressed_x->data)
        ggit_vector_destroy(compressed_x);

    ggit_vector_init(compressed_x, sizeof(struct compressed_x));

    ggit_vector_reserve(compressed_x, g_width);
    memset(compressed_x->data, 0, compressed_x->capacity * compressed_x->value_size);
    compressed_x->size = g_width;
    for (int i = i_from; i < i_to; ++i) {
        int const commit_i = g_height - 1 - i;
        int x = ggit_graph_commit_screen_column(graph, 0, commit_i);

        ggit_vector_ref_compressed_x(compressed_x, x)->taken = true;
    }

    int counter = 0;
    for (int i = 0; i < g_width; ++i) {
        struct compressed_x* cx = ggit_vector_ref_compressed_x(compressed_x, i);
        if (cx->taken) {
            cx->new_x = counter++;
        }
    }
    return counter;
}

static void
ggit_ui_draw_graph(
    struct ggit_ui* ui,
    struct ggit_input* input,
    struct ggit_graph* graph,
    SDL_Renderer* renderer,
    TTF_Font* font_monospaced
)
{
    int const g_width = graph->width;
    int const g_height = graph->height;

    int const graph_x = ui->graph_x;
    int const graph_y = ui->graph_y;

    int const i_max = min(g_height, 150);
    static int compressed_width = 0;
    // Compress X
    static struct ggit_vector compressed_x = { 0 };
    if (compressed_x.size != g_width) {
        compressed_width = ggit_ui_graph__generate_compressed_x(
            graph,
            0,
            i_max,
            &compressed_x
        );
    }

    // Draw spans (debug)
    ggit_ui_draw_graph__spans(ui, graph, 0, i_max, renderer, &compressed_x);
    // Draw connections
    ggit_ui_draw_graph__connections(ui, graph, 0, i_max, renderer, &compressed_x);

    // Draw commit messages
    ggit_ui_draw_graph__commit_messages(
        ui,
        graph,
        0,
        i_max,
        renderer,
        font_monospaced,
        compressed_width,
        &compressed_x
    );

    // Draw the crosshair
    set_color(renderer, (SDL_Color){ 0x22, 0x22, 0x22, 0xFF });
    SDL_RenderDrawLines(
        renderer,
        (SDL_Point[]){
            { 0, input->mouse_y },
            { 1920, input->mouse_y },
        },
        2
    );
    SDL_RenderDrawLines(
        renderer,
        (SDL_Point[]){
            { input->mouse_x, 0 },
            { input->mouse_x, 1080 },
        },
        2
    );

    // Draw the blocks.
    ggit_ui_draw_graph__boxes(ui, graph, 0, i_max, renderer, &compressed_x);

    // Draw the branch names.
    int const n_refs = graph->ref_names.size;
    for (int i = 0; i < n_refs; ++i) {
        int const commit_i = ggit_vector_get_int(&graph->ref_commits, i);

        int ci = g_height - 1 - commit_i;
        if (ci > i_max)
            continue;

        char const* name = ggit_vector_get_string(&graph->ref_names, i);

        int const commit_y = graph_y
                             + ggit_graph_commit_screen_y_top(ui, commit_i, g_height);
        util_draw_text(renderer, font_monospaced, name, 0, commit_y, 0, 0);
    }
}

static void
ggit_ui_input(struct ggit_ui* ui, struct ggit_input* input, struct ggit_graph* graph)
{
    if (input->buttons[0]) {
        ui->graph_x += input->delta_mouse_x;
        ui->graph_y += input->delta_mouse_y;
    }
}

int
main(int argc, char** argv)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    TTF_Init();
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_CreateWindowAndRenderer(
        1280,
        720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_MOUSE_FOCUS,
        &window,
        &renderer
    );
    SDL_SetWindowTitle(window, "GGit");
    int window_width;
    int window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);
    TTF_Font* font = TTF_OpenFont("res/segoeui.ttf", 13);

    struct ggit_input input = { 0 };
    struct ggit_ui ui = { 0 };
#define SMALL
#ifdef SMALL
    ui.item_w = 18;
    ui.item_h = 12;
#else
    ui.item_w = 38;
    ui.item_h = 26;
#endif
    ui.border = 1;
    ui.margin_x = 4;
    ui.margin_y = 4;
    struct ggit_ui original_ui = ui;

    float scale = 1.0f;


    struct ggit_graph graph;
    ggit_graph_init(&graph);

    // clang-format off
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ _strdup("master"),   regex_compile("^master$"),   0, { [0] = { 0x7E, 0xD3, 0x21 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ _strdup("hotfix/"),  regex_compile("^hotfix/"),  -1, { [0] = { 0xE6, 0x00, 0x00 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ _strdup("release/"), regex_compile("^release/"), -1, { [0] = { 0x00, 0x68, 0xDE }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ _strdup("bugfix/"),  regex_compile("^bugfix/"),  +1, { [0] = { 0xE6, 0x96, 0x17 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ _strdup("sprint/"),  regex_compile("^sprint/"),  +1, { [0] = { 0xB8, 0x16, 0xD9 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ _strdup("feature/"), regex_compile("^feature/"), +1, { [0] = { 0x34, 0xD3, 0xE5 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ _strdup(""),         regex_compile(".*"),        +1, { [0] = { 0xCC, 0xCC, 0xCC }, [1] = {} } });
    // clang-format on

    for (int i = 0; i < graph.special_branches.size; ++i) {
        ggit_vector_init(
            &ggit_vector_ref_special_branch(&graph.special_branches, i)->instances,
            sizeof(char*)
        );
        ggit_vector_init(
            &ggit_vector_ref_special_branch(&graph.special_branches, i)->spans,
            sizeof(struct ggit_column_span)
        );
    }

    // ggit_graph_load(&graph, "D:/public/ggit/tests/1");
    // ggit_graph_load(&graph, "D:/public/ggit/tests/2");
    ggit_graph_load(&graph, "D:/public/ggit/tests/3");
    // ggit_graph_load(&graph, "D:/public/ggit/tests/tag-with-multiple-matches");
    // ggit_graph_load(&graph, "C:/Projects/ColumboMonorepo");
    // ggit_graph_load(&graph, "D:/Stuff/work/Columbo");

    bool running = true;
    while (running) {
        input.delta_mouse_x = 0;
        input.delta_mouse_y = 0;
        int a;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_CLOSE: running = false; break;
                        case SDL_WINDOWEVENT_RESIZED:
                            window_width = event.window.data1;
                            window_height = event.window.data2;
                            break;
                    }
                case SDL_MOUSEMOTION:
                    if (input.buttons[0]) {
                        input.delta_mouse_x += event.motion.x - input.mouse_x;
                        input.delta_mouse_y += event.motion.y - input.mouse_y;
                    }
                    input.mouse_x = event.motion.x;
                    input.mouse_y = event.motion.y;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    input.buttons[event.button.button - 1] = true;
                    break;
                case SDL_MOUSEBUTTONUP:
                    input.buttons[event.button.button - 1] = false;
                    break;
                case SDL_MOUSEWHEEL:
                    if (input.is_ctrl_down) {
                        scale += 0.1f * (-1.0f + 2.0f * (event.wheel.y > 0));

                        ui.item_w = original_ui.item_w * scale;
                        ui.item_h = original_ui.item_h * scale;
                        ui.border = original_ui.border;
                        ui.margin_x = original_ui.margin_x;
                        ui.margin_y = original_ui.margin_y;
                    } else {
                        int delta = -1 + 2 * (event.wheel.y > 0);
                        ui.graph_y += delta * (ui.item_h + ui.border * 2 + ui.margin_y * 2);
                    }

                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.scancode == SDL_SCANCODE_LCTRL) {
                        input.is_ctrl_down = true;
                    }
                    if (event.key.keysym.scancode == SDL_SCANCODE_F5) {
                        ggit_graph_load(&graph, "D:/public/ggit/tests/3");
                    }
                    if (event.key.keysym.scancode == SDL_SCANCODE_F6) {
                        ggit_graph_load(&graph, "D:/Stuff/work/Columbo");
                    }
                    break;

                case SDL_KEYUP: break;
            }
        }

        ggit_ui_input(&ui, &input, &graph);
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        SDL_RenderClear(renderer);
        ggit_ui_draw_graph(&ui, &input, &graph, renderer, font);
        SDL_RenderPresent(renderer);
    }
end:;
    TTF_CloseFont(font);
    return 0;
}

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

#include "ggit-vector.h"
#include "ggit-graph.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NONEARFAR
#include <Windows.h>

#define ARRAY_COUNT(array) ARRAYSIZE(array)

#include "ggit-ui-settings.h"

struct ggit_input
{
    int mouse_x;
    int mouse_y;
    int wheel_y;
    int wheel_x;
    int delta_mouse_x;
    int delta_mouse_y;
    bool buttons[5];
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
static inline int
ggit_graph_commit_screen_column(
    struct ggit_graph* graph,
    struct ggit_vector* x_map,
    int commit_index
)
{
    struct ggit_commit_tag const tag = graph->tags[commit_index];
    int const branch = tag.tag[0];
    int const index = tag.tag[1];
    struct ggit_special_branch const* commit_branch = ggit_vector_ref_special_branch(
        &graph->special_branches,
        branch
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
    column += index;

    if (x_map && x_map->size > column) {
        column = ggit_vector_ref_compressed_x(x_map, column)->new_x;
    }

    return column;
}
static inline int
ggit_graph_commit_screen_x_left(
    struct ggit_graph* graph,
    struct ggit_vector* x_map,
    int commit_index
)
{
    int const column = ggit_graph_commit_screen_column(graph, x_map, commit_index);
    int const commit_x_left = column * ITEM_BOX_W;

    return commit_x_left;
}

static inline int
ggit_graph_commit_screen_x_center(
    struct ggit_graph* graph,
    struct ggit_vector* x_map,
    int commit_index
)
{
    int const commit_x_left = ggit_graph_commit_screen_x_left(
        graph,
        x_map,
        commit_index
    );
    int const commit_x_center = (ITEM_BOX_W / 2) + commit_x_left;
    return commit_x_center;
}

static inline int
ggit_graph_commit_screen_y_top(int graph_width, int graph_height, int commit_index)
{
    int const commit_y_top = ITEM_BOX_H * (graph_height - 1 - commit_index);
    return commit_y_top;
}
static inline int
ggit_graph_commit_screen_y_center(int graph_width, int graph_height, int commit_index)
{
    int const commit_y_top = ggit_graph_commit_screen_y_top(
        graph_width,
        graph_height,
        commit_index
    );
    int const commit_y_center = (ITEM_BOX_H / 2) + commit_y_top;
    return commit_y_center;
}

struct ggit_ui
{
    int graph_x;
    int graph_y;
};


static void
ggit_ui_draw_graph(
    struct ggit_ui* ui,
    struct ggit_graph* graph,
    SDL_Renderer* renderer,
    TTF_Font* font_monospaced
)
{
    int const i_max = 150;
    int const g_width = graph->width;
    int const g_height = graph->height;

    int const graph_x = ui->graph_x;
    int const graph_y = ui->graph_y;

    static int compressed_width = 0;
    // Compress X
    static struct ggit_vector compressed_x = { 0 };
    if (compressed_x.size != g_width) {
        if (compressed_x.data)
            ggit_vector_destroy(&compressed_x);

        ggit_vector_init(&compressed_x, sizeof(struct compressed_x));

        ggit_vector_reserve(&compressed_x, g_width);
        memset(compressed_x.data, 0, compressed_x.capacity * compressed_x.value_size);
        compressed_x.size = g_width;
        for (int i = 0; i < g_height; ++i) {
            if (i > i_max)
                break;

            int const commit_i = g_height - 1 - i;
            int x = ggit_graph_commit_screen_column(graph, 0, commit_i);

            ggit_vector_ref_compressed_x(&compressed_x, x)->taken = true;
        }

        int counter = 0;
        for (int i = 0; i < g_width; ++i) {
            struct compressed_x* cx = ggit_vector_ref_compressed_x(&compressed_x, i);
            if (cx->taken) {
                cx->new_x = counter++;
            }
        }
        compressed_width = counter;
    }

    int const text_x = graph_x + compressed_width * ITEM_BOX_W + 16;

    // Draw the text to the right.
    for (int i = 0; i < g_height; ++i) {
        if (i > i_max)
            break;
        int const commit_i = g_height - 1 - i;
        int const commit_y = graph_y
                             + ggit_graph_commit_screen_y_top(
                                 graph->width,
                                 graph->height,
                                 commit_i
                             );
        char const* message = graph->messages[commit_i];
        util_draw_text(renderer, font_monospaced, message, text_x, commit_y, 0, 0);
    }

    // Draw lines between the commits.
    set_color(renderer, (SDL_Color){ 0xAA, 0xAA, 0xAA, 0xFF });
    for (int i = 0; i < g_height; ++i) {
        if (i > i_max)
            break;
        int const commit_i = g_height - 1 - i;

        int const commit_x = graph_x
                             + ggit_graph_commit_screen_x_left(
                                 graph,
                                 &compressed_x,
                                 commit_i
                             );
        int const commit_y = graph_y
                             + ggit_graph_commit_screen_y_center(
                                 g_width,
                                 g_height,
                                 commit_i
                             );
        int const commit_center_x = graph_x
                                    + ggit_graph_commit_screen_x_center(
                                        graph,
                                        &compressed_x,
                                        commit_i
                                    );
        int const commit_y_bottom = commit_y + ITEM_H / 2 + BORDER + MARGIN_Y;

        bool is_merge = graph->parents[commit_i].parent[1] != -1;
        for (int j = 0; j < ARRAY_COUNT(graph->parents->parent); ++j) {
            int parent = graph->parents[commit_i].parent[j];
            if (parent == -1)
                break;

            int const parent_center_x = graph_x
                                        + ggit_graph_commit_screen_x_center(
                                            graph,
                                            &compressed_x,
                                            parent
                                        );
            int const parent_y_top = graph_y
                                     + ggit_graph_commit_screen_y_top(
                                         g_width,
                                         g_height,
                                         parent
                                     );

            int offset_merge = is_merge * -2;

            // Commit - center to bottom
            SDL_RenderDrawLine(
                renderer,
                commit_center_x,
                commit_y_bottom + offset_merge,
                commit_center_x,
                commit_y
            );
            // Middle - left/right to match parent column
            SDL_RenderDrawLine(
                renderer,
                commit_center_x,
                commit_y_bottom + offset_merge,
                parent_center_x,
                commit_y_bottom
            );
            // Middle - down to match parent Top
            SDL_RenderDrawLine(
                renderer,
                parent_center_x,
                commit_y_bottom,
                parent_center_x,
                parent_y_top
            );
            // Parent - center to top.
            SDL_RenderDrawLine(
                renderer,
                parent_center_x,
                parent_y_top,
                parent_center_x,
                parent_y_top + MARGIN_Y
            );
        }
    }

    // Draw the blocks.
    for (int i = 0; i < g_height; ++i) {
        if (i > i_max)
            break;
        int const commit_i = g_height - 1 - i;
        int const tag = graph->tags[commit_i].tag[0];
        int const column = tag;

        int const commit_x = MARGIN_X + graph_x
                             + ggit_graph_commit_screen_x_left(
                                 graph,
                                 &compressed_x,
                                 commit_i
                             );
        int const commit_y = MARGIN_Y + graph_y
                             + ggit_graph_commit_screen_y_top(
                                 g_width,
                                 g_height,
                                 commit_i
                             );
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
    // Draw the branch names.
    int const n_refs = graph->ref_names.size;
    for (int i = 0; i < n_refs; ++i) {
        int const commit_i = ggit_vector_get_int(&graph->ref_commits, i);

        int ci = g_height - 1 - commit_i;
        if (ci > i_max)
            continue;

        char const* name = ggit_vector_get_string(&graph->ref_names, i);

        int const commit_y = graph_y
                             + ggit_graph_commit_screen_y_top(
                                 g_width,
                                 g_height,
                                 commit_i
                             );
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
    TTF_Font* font = TTF_OpenFont("res/segoeui.ttf", 14);

    struct ggit_input input = { 0 };
    struct ggit_ui ui = { 0 };
    struct ggit_graph graph;
    ggit_graph_init(&graph);
    ggit_vector_init(&graph.special_branches, sizeof(struct ggit_special_branch));

    // clang-format off
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "master",           regex_compile("^master$"),           0, { [0] = { 0x7E, 0xD3, 0x21 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "hotfix/",          regex_compile("^hotfix/"),          -1, { [0] = { 0xE6, 0x00, 0x00 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "release/",         regex_compile("^release/"),         -1, { [0] = { 0x00, 0x68, 0xDE }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "bugfix/",          regex_compile("^bugfix/"),          +1, { [0] = { 0xE6, 0x96, 0x17 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "develop/|sprint/", regex_compile("^(develop|sprint)/"),+1, { [0] = { 0xB8, 0x16, 0xD9 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "feature/",         regex_compile("^feature/"),         +1, { [0] = { 0x34, 0xD3, 0xE5 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "",                 regex_compile(".*"),                +1, { [0] = { 0xEE, 0xEE, 0xEE }, [1] = {} } });
    // clang-format on

    for (int i = 0; i < graph.special_branches.size; ++i) {
        ggit_vector_init(
            &ggit_vector_ref_special_branch(&graph.special_branches, i)->instances,
            sizeof(char*)
        );
    }

    // ggit_graph_load(&graph, "D:/public/ggit/tests/1");
    // ggit_graph_load(&graph, "D:/public/ggit/tests/2");
    ggit_graph_load(&graph, "D:/public/ggit/tests/3");
    // ggit_graph_load(&graph, "D:/public/ggit/tests/tag-with-multiple-matches");
    // ggit_graph_load(&graph, "C:/Projects/ColumboMonorepo");

    bool running = true;
    while (running) {
        input.delta_mouse_x = 0;
        input.delta_mouse_y = 0;

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
                case SDL_KEYDOWN:
                    if (event.key.keysym.scancode == SDL_SCANCODE_F5) {
                        ggit_graph_load(&graph, "D:/public/ggit/tests/3");
                    }
                    break;

                case SDL_KEYUP: break;
            }
        }

        ggit_ui_input(&ui, &input, &graph);
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        SDL_RenderClear(renderer);
        ggit_ui_draw_graph(&ui, &graph, renderer, font);
        SDL_RenderPresent(renderer);
    }
end:;
    TTF_CloseFont(font);
    return 0;
}
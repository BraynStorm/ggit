/*
    Tasks
    ========

    - Replace SDL with GDI.
    - Split platform layer.
*/

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

struct Input
{
    int mouse_x;
    int mouse_y;
    int wheel_y;
    int wheel_x;
    int delta_mouse_x;
    int delta_mouse_y;
    bool buttons[5];
};

struct Hash
{
    unsigned char sha[40];
};
typedef struct Hash Parents[2];
typedef struct Hash Ref;

GGIT_GENERATE_VECTOR_GETTERS(struct Hash, hash);
GGIT_GENERATE_VECTOR_GETTERS(Ref, ref);
GGIT_GENERATE_VECTOR_REF_GETTER(Parents, parents);

void
random_hash(struct Hash* out)
{
    for (int i = 0; i < 40; ++i)
        out->sha[i] = rand();
}


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

    SDL_Surface* text_surface = TTF_RenderUTF8_Solid(
        font,
        text,
        (SDL_Color){ 0, 0, 0, 255 } // , (SDL_Color){ 255, 255, 255, 255 }
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
ggit_graph_commit_screen_x_left(struct ggit_graph* graph, int commit_index)
{
    short* tag = graph->tags[commit_index].tag;
    int column = 0;
    for (int i = 0; i < graph->special_branches.size; ++i) {
        struct ggit_special_branch* branch = ggit_vector_ref_special_branch(
            &graph->special_branches,
            i
        );
        if (i >= tag[0])
            break;

        column += branch->instances.size;
    }
    int index = tag[1];
    column += index;

    int const commit_x_left = column * ITEM_BOX_W;

    return commit_x_left;
}

static inline int
ggit_graph_commit_screen_x_center(struct ggit_graph* graph, int commit_index)
{
    int const commit_x_left = ggit_graph_commit_screen_x_left(graph, commit_index);
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

static void
ggit_graph_draw(
    struct ggit_graph* graph,
    SDL_Renderer* renderer,
    TTF_Font* font_monospaced
)
{
    int const g_width = graph->width;
    int const g_height = graph->height;

    int const text_x = (graph->width + 2) * ITEM_OUTER_W + 16;

    // Draw the text to the right.
    set_color(renderer, (SDL_Color){ 0x05, 0x05, 0x05, 0xFF });
    for (int i = 0; i < graph->height; ++i) {
        int const commit_i = graph->height - 1 - i;
        char const* message = graph->messages[commit_i];
        util_draw_text(
            renderer,
            font_monospaced,
            message,
            text_x,
            ggit_graph_commit_screen_y_top(graph->width, graph->height, commit_i),
            0,
            0
        );
    }

    // Draw lines between the commits.
    set_color(renderer, (SDL_Color){ 0xAA, 0xAA, 0xAA, 0xFF });
    for (int i = 0; i < graph->height; ++i) {
        int const commit_i = graph->height - 1 - i;

        int const commit_x = ggit_graph_commit_screen_x_left(graph, commit_i);
        int const commit_y = ggit_graph_commit_screen_y_center(
            g_width,
            g_height,
            commit_i
        );
        int const commit_center_x = ggit_graph_commit_screen_x_center(graph, commit_i);
        int const commit_y_bottom = commit_y + ITEM_H / 2 + BORDER + MARGIN_Y;

        for (int j = 0; j < ARRAY_COUNT(graph->parents->parent); ++j) {
            int parent = graph->parents[commit_i].parent[j];
            if (parent == -1)
                break;

            int const parent_center_x = ggit_graph_commit_screen_x_center(
                graph,
                parent
            );
            int const parent_y_top = ggit_graph_commit_screen_y_top(
                g_width,
                g_height,
                parent
            );

            // Connective (for current commit)
            SDL_RenderDrawLine(
                renderer,
                commit_center_x,
                commit_y_bottom,
                commit_center_x,
                commit_y
            );
            SDL_RenderDrawLine(
                renderer,
                commit_center_x,
                commit_y_bottom,
                parent_center_x,
                parent_y_top
            );
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
        int const commit_i = g_height - 1 - i;
        int const tag = graph->tags[commit_i].tag[0];
        int const column = tag;

        int const commit_x = MARGIN_X
                             + ggit_graph_commit_screen_x_left(graph, commit_i);
        int const commit_y = MARGIN_Y
                             + ggit_graph_commit_screen_y_top(
                                 g_width,
                                 g_height,
                                 commit_i
                             );

        int const cut = 3;

        draw_rect_cut(
            renderer,
            commit_x,
            commit_y,
            commit_x + ITEM_OUTER_W,
            commit_y + ITEM_OUTER_H,
            cut,
            GGIT_COLOR_BORDER
        );

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

int
main(int argc, char** argv)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    TTF_Init();
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_CreateWindowAndRenderer(1280, 720, SDL_WINDOW_RESIZABLE, &window, &renderer);
    SDL_SetWindowTitle(window, "GGit");
    int window_width;
    int window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);
    TTF_Font* font = TTF_OpenFont("res/segoeui.ttf", 14);

    struct Input input = { 0 };


    struct ggit_graph graph;
    ggit_graph_init(&graph);
    ggit_vector_init(&graph.special_branches, sizeof(struct ggit_special_branch));
    // clang-format off
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "master", +1, { [0] = { 0x7E, 0xD3, 0x21 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "hotfix/", +1, { [0] = { 0xE6, 0x00, 0x00 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "release/", -1, { [0] = { 0x00, 0x68, 0xDE }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "bugfix/", -1, { [0] = { 0xE6, 0x96, 0x17 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "develop/", -1, { [0] = { 0xB8, 0x16, 0xD9 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "feature/", -1, { [0] = { 0x34, 0xD3, 0xE5 }, [1] = {} } });
    ggit_vector_push(&graph.special_branches, &(struct ggit_special_branch){ "", -1, { [0] = { 0xEE, 0xEE, 0xEE }, [1] = {} } });
    // clang-format on
    for (int i = 0; i < graph.special_branches.size; ++i) {
        ggit_vector_init(
            &ggit_vector_ref_special_branch(&graph.special_branches, i)->instances,
            sizeof(char*)
        );
    }

    ggit_graph_load(&graph, "D:/public/ggit/tests/3");
    // ggit_graph_load(&graph, "D:/public/ggit/tests/tag-with-multiple-matches");
    // ggit_graph_load(&graph, "C:/Projects/ColumboMonorepo");

    bool running = true;
    while (running) {
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
                    input.mouse_x = event.motion.x;
                    input.mouse_y = event.motion.y;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    input.buttons[event.button.button] = true;
                    break;
                case SDL_MOUSEBUTTONUP:
                    input.buttons[event.button.button] = false;
                    break;
                case SDL_KEYDOWN: break;
                case SDL_KEYUP: break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        SDL_RenderClear(renderer);
        ggit_graph_draw(&graph, renderer, font);
        SDL_RenderPresent(renderer);
    }
end:;
    TTF_CloseFont(font);
    return 0;
}
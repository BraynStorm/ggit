#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_ttf.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ggit-vector.h"
#include "ggit-graph.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NONEARFAR
#include <Windows.h>
#undef DrawText

#define ARRAY_COUNT(array) ARRAYSIZE(array)

/* TODO: add more. */
SDL_Color const colors[] = { { 0xE6, 0x00, 0x00, 0xFF },
                             { 0x7E, 0xD3, 0x21, 0xFF },
                             { 0x00, 0x68, 0xDE, 0xFF },
                             { 0xE6, 0x96, 0x17, 0xFF },
                             { 0xB8, 0x16, 0xD9, 0xFF } };
SDL_Color const color_vline = { 0x9B, 0x9B, 0x9B, 0xFF };
SDL_Color const color_hline = { 0xDA, 0xDA, 0xDA, 0xFF };


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


struct Graph
{
    struct ggit_vector commit_messages;
    struct ggit_vector commit_hashes;
    struct ggit_vector commit_parents;

    struct ggit_vector branch_names;
    struct ggit_vector branch_refs;

    int head;
};
static void
graph_commit(
    struct Graph* g,
    char* message,
    struct Hash* parent_a,
    struct Hash* parent_b
)
{
    ggit_vector_push(&g->commit_messages, &message);

    Parents parents = { 0 };
    if (parent_a) {
        parents[0] = *parent_a;

        if (parent_b)
            parents[1] = *parent_b;
    } else {
        parents[0] = ggit_vector_get_ref(&g->branch_refs, g->head);
    }
    ggit_vector_push(&g->commit_parents, &parents);

    Ref ref = { 0 };
    random_hash(&ref);
    *ggit_vector_ref_ref(&g->branch_refs, g->head) = ref;
    ggit_vector_push(&g->commit_hashes, &ref);
}
static void
graph_reset(struct Graph* g, Ref* ref)
{
    if (!g->branch_refs.size)
        return;

    *ggit_vector_ref_ref(&g->branch_refs, g->head) = *ref;
}
static void
graph_create_branch(struct Graph* g, char const* name)
{
    char* name_copy = _strdup(name);
    if (g->branch_refs.size) {
        int old_branch = g->head;
        Ref* ref = ggit_vector_ref_ref(&g->branch_refs, old_branch);

        // Create the branch name and the ref.
        g->head = g->branch_names.size;

        ggit_vector_push(&g->branch_names, &name_copy);
        ggit_vector_push(&g->branch_refs, ref);
    } else {
        ggit_vector_push(&g->branch_names, &name_copy);
        ggit_vector_push(&g->branch_refs, &(Ref){ 0 });
    }
}
static void
graph_checkout(struct Graph* g, char const* thing)
{
    for (int i = 0; i < g->branch_names.size; ++i) {
        char const* branch_name = ggit_vector_get_string(&g->branch_names, i);
        if (strcmp(branch_name, thing) == 0) {
            g->head = i;
            goto end_find;
        }
    }

    /* TODO:implement */
    abort();

end_find:;
}
static void
graph_merge(struct Graph* g, char const* thing)
{
    int const current_branch = g->head;
    char const* current_branch_name = *(char**)ggit_vector_get(
        &g->branch_names,
        current_branch
    );

    bool is_branch = false;
    bool is_commit = false;
    struct Hash* ref = 0;
    char const* other_branch_name = 0;

    for (int i = 0; i < g->commit_hashes.size; ++i) {
        Ref* commit = ggit_vector_ref_ref(&g->commit_hashes, i);
        if (strcmp((char const*)commit->sha, thing) == 0) {
            // They are the same!
            ref = commit;
            is_commit = true;
            goto end_find;
        }
    }

    for (int i = 0; i < g->branch_names.size; ++i) {
        char const* branch_name = ggit_vector_get_string(&g->branch_names, i);
        if (strcmp(branch_name, thing) == 0) {
            // They are the same!
            is_branch = true;
            other_branch_name = ggit_vector_get_string(&g->branch_names, i);
            ref = ggit_vector_ref_ref(&g->branch_refs, i);
            goto end_find;
        }
    }

end_find:;

    char* message = calloc(256, 1);
    if (is_branch) {
        sprintf_s(
            message,
            256,
            "Merge branch '%s' into '%s'.",
            other_branch_name,
            current_branch_name
        );
    } else if (is_commit) {
        sprintf_s(
            message,
            256,
            "Merge '%.40s' into '%s'.",
            ref->sha,
            current_branch_name
        );
    }

    graph_commit(g, message, ggit_vector_ref_ref(&g->branch_refs, g->head), ref);
}
static void
graph_init(struct Graph* g)
{
    memset(g, 0, sizeof(*g));
    ggit_vector_init(&g->commit_messages, sizeof(char*));
    ggit_vector_init(&g->commit_hashes, sizeof(struct Hash));
    ggit_vector_init(&g->commit_parents, sizeof(Parents));
    ggit_vector_init(&g->branch_names, sizeof(char*));
    ggit_vector_init(&g->branch_refs, sizeof(struct Hash));

    char* text = _strdup("Initial commit");
    ggit_vector_push(&g->commit_messages, &text);
    ggit_vector_push(&g->commit_hashes, &(struct Hash){ 0 });
    ggit_vector_push(&g->commit_parents, &(Parents){ 0 });

    graph_create_branch(g, "master");
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

        if (!SetHandleInformation(stdin_pipe_write, HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(stdout_pipe_read, HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(stderr_pipe_read, HANDLE_FLAG_INHERIT, 0))
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
        (SDL_Vertex){ { x1 - cut, y1 }, color } /* bottom */,
        (SDL_Vertex){ { x1, y1 - cut }, color } /* top */,

        // top-right corner
        (SDL_Vertex){ { x1, y0 + cut }, color } /* bottom */,
        (SDL_Vertex){ { x1 - cut, y0 }, color } /* top */,
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
graph_init_from_git(struct Graph* g)
{
    struct ggit_vector git_out;
    ggit_vector_init(&git_out, sizeof(char));

    char* command_line = _strdup("git log --oneline");
    call_git(command_line, &git_out);
    free(command_line);
}
static void
DrawText(
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
}

static void
set_color(SDL_Renderer* renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void
ggit_graph_draw(
    struct ggit_graph* graph,
    SDL_Renderer* renderer,
    TTF_Font* font_monospaced
)
{
    int const item_w = 38;
    int const item_h = 26;
    int const border = 2;
    int const margin = 3;

    int const item_outer_w = item_w + border * 2;
    int const item_outer_h = item_h + border * 2;

    int const text_x = (graph->width + 1) * item_outer_w + 16;

    int last_commit_center_x = 0;
    int last_commit_center_y = 0;

    // Draw the text to the right.
    set_color(renderer, (SDL_Color){ 0x05, 0x05, 0x05, 0xFF });
    for (int i = 0; i < graph->height; ++i) {
        int const commit_i = graph->height - 1 - i;
        char const* message = graph->messages[commit_i];
        DrawText(renderer, font_monospaced, message, text_x, i * item_outer_h, 0, 0);
    }

    // Draw lines between the commits.
    for (int i = 0; i < graph->height; ++i) {
        int const commit_i = graph->height - 1 - i;
        int const tag = graph->tags[commit_i].tag[0];
        int const column = tag;

        int const commit_x = column * item_outer_w + border;
        int const commit_y = i * item_outer_h + border;
        int const commit_center_x = commit_x + item_outer_w / 2;
        int const commit_center_y = commit_y + item_outer_h / 2;

        if (i) {
            set_color(renderer, (SDL_Color){ 0xAA, 0xAA, 0xAA, 0xFF });
            SDL_RenderDrawLine(
                renderer,
                commit_center_x,
                commit_y,
                last_commit_center_x,
                last_commit_center_y
            );
        }

        last_commit_center_x = commit_center_x;
        last_commit_center_y = commit_center_y;
    }

    // Draw the rectangles of the commits.
    for (int i = 0; i < graph->height; ++i) {
        int const commit_i = graph->height - 1 - i;
        int const tag = graph->tags[commit_i].tag[0];
        int const column = tag;

        int const commit_x = column * item_outer_w + border;
        int const commit_y = i * item_outer_h + border;

        int const cut = 4;
        set_color(renderer, (SDL_Color){ 0xE3, 0xE3, 0xE3, 0xFF });
        draw_rect_cut(
            renderer,
            commit_x,
            commit_y,
            commit_x + item_outer_w,
            commit_y + item_outer_h,
            cut,
            (SDL_Color){ 0xE3, 0xE3, 0xE3, 0xFF }
        );

        SDL_Color color;
        if (tag < ARRAY_COUNT(colors)) {
            color = colors[tag];
        } else {
            color = (SDL_Color){ 0xEE, 0xEE, 0xEE, 0xFF };
        }
        // set_color(renderer, c);
        draw_rect_cut(
            renderer,
            commit_x + border,
            commit_y + border,
            commit_x + border + item_w,
            commit_y + border + item_h,
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
    int window_width;
    int window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);
    TTF_Font* font = TTF_OpenFont("res/segoeui.ttf", 14);

    struct Input input = { 0 };

    struct ggit_graph graph;
    ggit_graph_init(&graph);
    ggit_graph_load(&graph, "../../tests/simple-1");

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

#if 0
        int x = 167;
        for (int i = graph.commit_hashes.size - 1; i >= 0; i--) {
            int y = (26 + 5) * i;

            int c = 0;

            Ref const* commit_hash = ggit_vector_ref_ref(&graph.commit_hashes, i);
            for (int b = 0; b < graph.branch_refs.size; ++b) {
                Ref const* branch_hash = ggit_vector_ref_ref(&graph.branch_refs, b);

                if (memcmp(branch_hash, commit_hash, sizeof(*branch_hash)) == 0) {
                    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
                    char const* branch_name = ggit_vector_get_string(
                        &graph.branch_names,
                        b
                    );
                    DrawText(renderer, font, branch_name, x - 100, y + 5, 0, 0);
                }
            }
            set_color(renderer, colors[c]);
            SDL_RenderFillRect(renderer, &(struct SDL_Rect){ x, y, 38, 26 });

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            DrawText(
                renderer,
                font,
                ggit_vector_get_string(&graph.commit_messages, i),
                x + 100,
                y + 5,
                0,
                0
            );
            if (y >= window_height)
                break;
        }
#endif
        ggit_graph_draw(&graph, renderer, font);
        SDL_RenderPresent(renderer);
    }
end:;
    TTF_CloseFont(font);
    return 0;
}
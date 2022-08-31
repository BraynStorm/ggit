#include "ggit-git.h"
#include "ggit-vector.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>


int
ggit_git_run(
    char const* restrict command,
    char** restrict out_stdout,
    int* restrict out_stdout_length
)
{
    struct ggit_vector buf_stdout = { 0 };
    ggit_vector_init(&buf_stdout, sizeof(char));
    ggit_vector_reserve(&buf_stdout, 4096);

    FILE* pipe = _popen(command, "r");
    if (!pipe)
        return false;

    while (true) {
        char* begin = (char*)buf_stdout.data;
        size_t read = fread(
            begin + buf_stdout.size,
            1,
            buf_stdout.capacity - buf_stdout.size,
            pipe
        );
        if (read == 0) {
            /* NOTE(boz): Put the null terminator */
            ggit_vector_push(&buf_stdout, &read);
            break;
        }
        buf_stdout.size += (int)read;
        if (buf_stdout.size == buf_stdout.capacity)
            ggit_vector_reserve_more(&buf_stdout, buf_stdout.capacity);
    }
    int exit_code = _pclose(pipe);
    fclose(pipe);

    if (out_stdout)
        *out_stdout = (char*)buf_stdout.data;
    else
        free(buf_stdout.data);
    if (out_stdout_length)
        *out_stdout_length = buf_stdout.size;
    return exit_code;
}

int
ggit_git_checkout_branch(
    char const* restrict repository_path,
    char const* restrict branch_name
)
{
    struct ggit_vector cmd;
    ggit_vector_init(&cmd, sizeof(char));
    ggit_vector_reserve(&cmd, 256);
    ggit_vector_push_sprintf_terminated(
        &cmd,
        "git -C \"%s\" branch %s",
        repository_path,
        branch_name
    );

    char* output;
    int n_output;
    int status = ggit_git_run(cmd.data, &output, &n_output);
    ggit_vector_destroy(&cmd);

    free(output);

    return status;
}


int
ggit_git_cherry_pick(
    char const* restrict repository_path,
    int n_commits,
    char const* restrict* commit_hashes
)
{
    char const space = ' ';
    char const null = '\0';

    struct ggit_vector cmd;
    ggit_vector_init(&cmd, sizeof(char));
    ggit_vector_reserve(&cmd, 256 + 40 * n_commits);
    ggit_vector_push_sprintf(&cmd, "git -C \"%s\" cherry-pick ", repository_path);
    for (int i = 0; i < n_commits; ++i) {
        ggit_vector_push_string(&cmd, commit_hashes[i]);
        ggit_vector_push(&cmd, &space);
    }
    ggit_vector_push(&cmd, &null);

    char* output;
    int n_output;
    bool status = ggit_git_run(cmd.data, &output, &n_output);
    ggit_vector_destroy(&cmd);

    free(output);
    return status;
}

int
ggit_git_status(char const* restrict repository_path, struct ggit_vector* out_vec)
{
    struct ggit_vector cmd;
    ggit_vector_init(&cmd, sizeof(char));
    ggit_vector_reserve(&cmd, 256);
    ggit_vector_push_sprintf_terminated(
        &cmd,
        "git -C \"%s\" status -s -b",
        repository_path
    );

    char* output;
    int n_output;
    int status = ggit_git_run(cmd.data, &output, &n_output);
    ggit_vector_destroy(&cmd);

    /* TODO: parse the output: git status -b -s:
## feature/ui/cherry-pick
MM ggit-git.c
 M ggit-git.h
 M ggit-graph.h
 M ggit-vector.c
 M ggit-vector.h
 M ggit.c
    */

    free(output);

    return status;
}

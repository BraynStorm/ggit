#include "ggit-git.h"
#include "ggit-vector.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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


void
ggit_vector_push_string(struct ggit_vector* vector, char const* string)
{
    char c;
    int length = strlen(string);
    int vector_size = vector->size;

    ggit_vector_reserve_more(vector, length + 1);
    memcpy((char*)vector->data + vector_size, string, length);
    vector->size = vector_size + length;
}

int
ggit_git_checkout(char const* restrict repository_path, char const* restrict branch)
{
    return 0;
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
    ggit_vector_push_string(&cmd, "git -C \"");
    ggit_vector_push_string(&cmd, repository_path);
    ggit_vector_push_string(&cmd, "\" cherry-pick ");
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
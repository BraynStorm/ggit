#pragma once

#include <stdbool.h>

#include "ggit-vector.h"

int ggit_git_run(
    char const* restrict command,
    char** restrict out_stdout,
    int* restrict out_stdout_length
);

int ggit_git_checkout_branch(
    char const* restrict repository_path,
    char const* restrict branch
);
int ggit_git_cherry_pick(
    char const* restrict repository_path,
    int n_commits,
    char const* restrict* commit_hashes
);
int ggit_git_status(
    char const* restrict repository_path,
    struct ggit_vector* out_modified_files
);
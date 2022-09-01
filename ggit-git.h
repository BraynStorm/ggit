#pragma once

#include <stdbool.h>

#include "ggit-vector.h"

struct ggit_git
{
    struct ggit_vector command;
    struct ggit_vector out;
    struct ggit_vector err;
};

void ggit_git_init(struct ggit_git*);
void ggit_git_clear(struct ggit_git*);
void ggit_git_destroy(struct ggit_git*);
int ggit_git_run(struct ggit_git*);
int ggit_git_run_vec(
    struct ggit_vector* cmd,
    struct ggit_vector* out,
    struct ggit_vector* err
);
int ggit_git_reset(
    struct ggit_git*,
    char const* restrict repository_path,
    char const* restrict to,
    char const* restrict mode
);
int ggit_git_checkout_branch(
    struct ggit_git*,
    char const* restrict repository_path,
    char const* restrict branch
);
int ggit_git_cherry_pick(
    struct ggit_git*,
    char const* restrict repository_path,
    int n_commits,
    char const* restrict* commit_hashes
);
int ggit_git_status(
    struct ggit_git*,
    char const* restrict repository_path,
    struct ggit_vector* out_modified_files
);
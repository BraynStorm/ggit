#include "ggit-git.h"
#include "ggit-vector.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

void
ggit_git_init(struct ggit_git* ggg)
{
    ggit_vector_init(&ggg->command, sizeof(char));
    ggit_vector_init(&ggg->out, sizeof(char));
    ggit_vector_init(&ggg->err, sizeof(char));
    ggit_vector_reserve(&ggg->command, 4096);
    ggit_vector_reserve(&ggg->out, 4096);
    ggit_vector_reserve(&ggg->err, 4096);
}
void
ggit_git_clear(struct ggit_git* ggg)
{
    ggit_vector_clear(&ggg->err);
    ggit_vector_clear(&ggg->out);
    ggit_vector_clear(&ggg->command);
}
void
ggit_git_destroy(struct ggit_git* ggg)
{
    ggit_vector_destroy(&ggg->err);
    ggit_vector_destroy(&ggg->out);
    ggit_vector_destroy(&ggg->command);
}
int
ggit_git_run(struct ggit_git* ggg)
{
    return ggit_git_run_vec(&ggg->command, &ggg->out, &ggg->err);
}

int
ggit_git_reset(
    struct ggit_git* ggg,
    char const* restrict repository_path,
    char const* restrict to,
    char const* restrict mode
)
{
    ggit_git_clear(ggg);
    ggit_vector_push_sprintf_terminated(
        &ggg->command,
        "git -C \"%s\" reset --%s %s",
        repository_path,
        mode,
        to
    );

    int status = ggit_git_run(ggg);

    /* TODO: Implement. */

    return status;
}
int
ggit_git_checkout_branch(
    struct ggit_git* ggg,
    char const* restrict repository_path,
    char const* restrict branch_name
)
{
    ggit_git_clear(ggg);
    ggit_vector_push_sprintf_terminated(
        &ggg->command,
        "git -C \"%s\" branch %s",
        repository_path,
        branch_name
    );

    int status = ggit_git_run(ggg);

    /* TODO: Implement? */

    return status;
}


int
ggit_git_cherry_pick(
    struct ggit_git* ggg,
    char const* restrict repository_path,
    int n_commits,
    char const* restrict* commit_hashes
)
{
    char const space = ' ';
    char const null = '\0';

    ggit_git_clear(ggg);
    ggit_vector_push_sprintf(
        &ggg->command,
        "git -C \"%s\" cherry-pick ",
        repository_path
    );
    for (int i = 0; i < n_commits; ++i) {
        ggit_vector_push_string(&ggg->command, commit_hashes[i]);
        ggit_vector_push(&ggg->command, &space);
    }
    ggit_vector_push(&ggg->command, &null);

    int status = ggit_git_run(ggg);
    return status;
}

int
ggit_git_status(
    struct ggit_git* ggg,
    char const* restrict repository_path,
    struct ggit_vector* out_vec
)
{
    ggit_vector_clear(&ggg->command);
    ggit_vector_clear(&ggg->out);
    ggit_vector_clear(&ggg->err);

    ggit_vector_push_sprintf_terminated(
        &ggg->command,
        "git -C \"%s\" status -s -b",
        repository_path
    );

    int status = ggit_git_run(ggg);

    /* TODO: parse the output: git status -b -s:
## feature/ui/cherry-pick
MM ggit-git.c
 M ggit-git.h
 M ggit-graph.h
 M ggit-vector.c
 M ggit-vector.h
 M ggit.c
    */

    return status;
}

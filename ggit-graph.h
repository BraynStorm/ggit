#pragma once

#include "ggit-vector.h"

#include <libsmallregex.h>

struct ggit_commit_parents
{
    int parent[2];
};

struct ggit_commit_tag
{
    /* NOTE:
        First short is the actual tag - master, hotfix, none, w/e.
        Second short is the "position" inside this tag.

        Example, some branches don't match our regular expressions,
        they are tagged as "none", but the indices inside allow
        us to separate them:

            If the branches were named exp1 and exp2,
            a commit inside them would have the following tags:

            commit in exp1
                tag[0] = none
                tag[1] = 0

            commit in exp2
                tag[0] = none
                tag[1] = 1
    */
    short tag[2];
    bool strong;
};

struct ggit_tag_entry
{
    char* tag_name;
};

enum ggit_growth_direction
{
    ggit_growth_direction_left = -1,
    ggit_growth_direction_right = 1,
};

struct ggit_column_span
{
    int merge_min;
    int commit_min;
    int commit_max;
    int merge_max;
};

struct ggit_special_branch
{
    /* Like: "release/" */
    char* name;
    struct small_regex* regex;

    /* Valid values:
        -1 = left
        +1 = right
    */
    int8_t growth_direction;

    /* Two colors, three bytes each (RGB). */
    uint8_t colors_base[2][3];

    /* [char*]                   */ struct ggit_vector instances;
    /* [struct ggit_column_span] */ struct ggit_vector spans;
};

struct ggit_graph
{
    int width;
    int height;

    char* path;

    int* message_lengths;
    char** messages;
    char** hashes;
    struct ggit_commit_parents* parents;
    struct ggit_commit_tag* tags;

    struct ggit_vector special_branches;

    struct ggit_vector ref_names;
    struct ggit_vector ref_hashes;
    struct ggit_vector ref_commits;
};

// clang-format align
int ggit_graph_init(struct ggit_graph*);
void ggit_graph_destroy(struct ggit_graph*);
void ggit_graph_clear(struct ggit_graph*);
int ggit_graph_load(struct ggit_graph*, char const* path_repository);
int ggit_graph_reload(struct ggit_graph*);

void ggit_special_branch_clear(struct ggit_special_branch*);
void ggit_special_branch_destroy(struct ggit_special_branch*);

GGIT_GENERATE_VECTOR_REF_GETTER(struct ggit_special_branch, special_branch)
GGIT_GENERATE_VECTOR_REF_GETTER(struct ggit_column_span, column_span)

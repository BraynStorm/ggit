#pragma once

#include "ggit-ui-settings.h"

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
};

enum ggit_primary_tag
{
#define X(name, ...) ggit_primary_tag_##name,
    GGIT_MAP_ENUM
#undef X
    /*  */
    ggit_primary_tag_COUNT,
};

struct ggit_tag_entry
{
    char* tag_name;
};

struct ggit_graph
{
    int width;
    int height;

    int* message_lengths;
    char** messages;
    char** hashes;
    struct ggit_commit_parents* parents;
    struct ggit_commit_tag* tags;
};

// clang-format align
int ggit_graph_init(struct ggit_graph*);
void ggit_graph_destroy(struct ggit_graph*);
void ggit_graph_clear(struct ggit_graph*);
int ggit_graph_load(struct ggit_graph*, char const* path_repository);
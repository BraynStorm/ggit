#pragma once

struct ggit_commit_parents
{
    int parent[2];
};

struct ggit_commit_tag
{
    short tag[4];
};

enum ggit_primary_tag
{
    ggit_primary_tag_master,
    ggit_primary_tag_hotfix,
    ggit_primary_tag_release,
    ggit_primary_tag_bugfix,
    ggit_primary_tag_develop /* AKA sprint */,
    ggit_primary_tag_feature,
    ggit_primary_tag_none,
    ggit_primary_tag_COUNT,
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
    int* commit_xs;
};

// clang-format align
int  ggit_graph_init   (struct ggit_graph*);
void ggit_graph_destroy(struct ggit_graph*);
void ggit_graph_clear  (struct ggit_graph*);
int  ggit_graph_load   (struct ggit_graph*, char const* path_repository);
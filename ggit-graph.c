#include "ggit-graph.h"
#include "ggit-vector.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>


int ggit_graph_init(struct ggit_graph* graph);
void ggit_graph_destroy(struct ggit_graph* graph);
void ggit_graph_clear(struct ggit_graph* graph);
int ggit_graph_load(struct ggit_graph* graph, char const* path_repository);


GGIT_GENERATE_VECTOR_REF_GETTER(struct ggit_commit_parents, commit_parents)
GGIT_GENERATE_VECTOR_REF_GETTER(struct ggit_commit_tag, commit_tags)


static bool
starts_with(char const* restrict str, char const* restrict prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

static int
index_of(int str_len, char const* str, char c)
{
    for (int i = 0; i < str_len; ++i) {
        if (str[i] == c)
            return i;
    }
    return -1;
}

static char*
strndup(int length, char const* text)
{
    char* dst = (char*)calloc(length + 1, 1);
    memcpy(dst, text, length);
    return dst;
}


static bool
ggit_run(
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
    fclose(pipe);

    if (out_stdout)
        *out_stdout = (char*)buf_stdout.data;
    else
        free(buf_stdout.data);
    if (out_stdout_length)
        *out_stdout_length = buf_stdout.size;
    return true;
}
static int
ggit_load_refs(
    int refs_len,
    char* restrict refs,
    struct ggit_vector* restrict ref_names,
    struct ggit_vector* restrict ref_hashes
)
{
    int start_hash = 0;
    int start_name = 0;
    int n = 0;
    for (int i = 0; i < refs_len; ++i) {
        switch (refs[i]) {
            case ' ':
                ggit_vector_push(ref_hashes, refs + start_hash);
                start_name = i + 1;
                break;
            case '\0':
                break;
            case '\n':
                char* name = strndup(i - start_name, refs + start_name);
                ggit_vector_push(ref_names, &name);
                start_hash = i + 1;
                ++n;
                break;
        }
    }
    return 0;
}

/* TODO:
    clean up the internals
*/
static bool
ggit_parse_merge_commit(
    int message_length,
    char const* merge_commit_message,
    char** out_name_main,
    char** out_name_kink
)
{
    /* NOTE(boz):
        Original regex:
        "Merge branch '(.+?)'(?: into (.+))?"
    */
    static char const start_0[] = "Merge branch '";
    int start_0_strlen = sizeof(start_0) - 1;
    if (!starts_with(merge_commit_message, start_0))
        return false;

    // NOTE(boz): GIT's default merge message when merging into master.
    int closing_quote = index_of(
        message_length - start_0_strlen,
        merge_commit_message + start_0_strlen,
        '\''
    );
    if (closing_quote) {
        int kink_len = closing_quote;
        char* kink_name = strndup(kink_len, merge_commit_message + start_0_strlen);
        if (closing_quote + start_0_strlen + 1 != message_length) {
            // into 'branch_main'
            char const* leftover_message = merge_commit_message + start_0_strlen +
                                           closing_quote + 1;
            if (starts_with(leftover_message, " into ")) {
                char const* main_name = leftover_message + 6;
                int main_len = (int)strlen(main_name);
                /* TODO(boz): Change to strdup. */
                *out_name_main = strndup(main_len, main_name);
            } else {
                *out_name_main = _strdup("master");
            }
        } else {
            *out_name_main = _strdup("master");
        }
        *out_name_kink = kink_name;
    } else {
        return false;
    }
    return true;
}

static int
ggit_match_refs_to_commits(
    struct ggit_vector* restrict ref_hashes,
    struct ggit_vector* restrict commit_hashes,
    struct ggit_vector* restrict out_ref_y
)
{
    int count_refs = ref_hashes->size;
    int count_commits = commit_hashes->size;
    for (int r = 0; r < count_refs; ++r) {
        char* ref_hash = ggit_vector_get(ref_hashes, r);

        for (int c = 0; c < count_commits; ++c) {
            char* commit_hash = ggit_vector_get_string(commit_hashes, c);
            int commit_hash_len = (int)strlen(commit_hash);

            if (0 == strncmp(commit_hash, ref_hash, commit_hash_len)) {
                ggit_vector_push(out_ref_y, &c);
                break;
            }
        }
    }

    return 0;
}

static enum ggit_primary_tag
ggit_local_branch_to_tag(char const* ref_name)
{
    /* PERF(boz):
        Convert ot hashmap find operation for performance
    */
    if (starts_with(ref_name, "master"))
        return ggit_primary_tag_master;
    if (starts_with(ref_name, "feature"))
        return ggit_primary_tag_feature;
    if (starts_with(ref_name, "bugfix"))
        return ggit_primary_tag_bugfix;
    if (starts_with(ref_name, "hotfix"))
        return ggit_primary_tag_hotfix;
    if (starts_with(ref_name, "release"))
        return ggit_primary_tag_release;
    if (starts_with(ref_name, "develop") || starts_with(ref_name, "sprint"))

        return ggit_primary_tag_develop;
    return ggit_primary_tag_none;
}

static enum ggit_primary_tag
ggit_refname_to_tag(char const* ref_name)
{
    if (starts_with(ref_name, "refs/heads/")) {
        return ggit_local_branch_to_tag(ref_name + 11);
    } else {
        return ggit_primary_tag_none;
    }
}

static int
ggit_label_merge_commits(
    struct ggit_vector* restrict commit_message_lengths,
    struct ggit_vector* restrict commit_messages,
    struct ggit_vector* restrict commit_parents,
    struct ggit_vector* restrict commit_tags
)
{
    int count = commit_message_lengths->size;
    int width = 0;

    for (int c = 0; c < count; ++c) {
        int const* my_parents = ggit_vector_ref_commit_parents(commit_parents, c)
                                    ->parent;
        if (my_parents[1] == -1)
            // One parent -> ignore the commit.
            continue;

        /* NOTE(boz): has 2 parents -> merge commit. */
        /* PERF(boz):
            Unoptimized. A 'cache-miss' just to get the length of a message?
            Measure if strlen() is faster on average in this case.
        */

        int msg_len = ggit_vector_get_int(commit_message_lengths, c);
        char const* msg = ggit_vector_get_string(commit_messages, c);

        char* name_main = 0;
        char* name_kink = 0;
        ggit_parse_merge_commit(msg_len, msg, &name_main, &name_kink);

        enum ggit_primary_tag tag_main = name_main ? ggit_local_branch_to_tag(name_main)
                                                   : ggit_primary_tag_none;
        enum ggit_primary_tag tag_kink = name_kink ? ggit_local_branch_to_tag(name_kink)
                                                   : ggit_primary_tag_none;

        short* my_tags = ggit_vector_ref_commit_tags(commit_tags, c)->tag;
        short* p0_tags = ggit_vector_ref_commit_tags(commit_tags, my_parents[0])->tag;
        short* p1_tags = ggit_vector_ref_commit_tags(commit_tags, my_parents[1])->tag;

        my_tags[0] = p0_tags[0] = tag_main;
        p1_tags[0] = tag_kink;

        width = max(tag_main, width);
        width = max(tag_kink, width);
    }
    return width;
}
static void
ggit_propagate_tags(
    struct ggit_vector* restrict commit_parents,
    struct ggit_vector* restrict commit_tags
)
{
    int count = commit_parents->size;
    assert(commit_tags->size == count);
    for (int c = count - 1; c >= 0; --c) {
        int const* my_parents = ggit_vector_ref_commit_parents(commit_parents, c)
                                    ->parent;
        if (my_parents[0] != -1) {
            short const* my_tags = ggit_vector_ref_commit_tags(commit_tags, c)->tag;
            short* p0_tags = ggit_vector_ref_commit_tags(commit_tags, my_parents[0])
                                 ->tag;
            if (p0_tags[0] == ggit_primary_tag_none) {
                p0_tags[0] = my_tags[0];
            }
            // p0_tags[0] = NK_MIN(my_tags[0], p0_tags[0]);
            /* TODO(boz): Propagate the other tags as well. */
        }
    }
}

#define GGIT_VECTOR_DEFINE(name, type, initial_size) \
    struct ggit_vector name;                         \
    ggit_vector_init(&name, sizeof(type));           \
    ggit_vector_reserve(&name, (initial_size));

static int
ggit_graph_load_repository(
    int gitlog_len,
    char* gitlog,
    int refs_len,
    char* refs,
    struct ggit_graph* out_graph
)
{
    int const heuristic_refs = 512;
    int const heuristic_commits = 4096;

    out_graph->width = 1;

    GGIT_VECTOR_DEFINE(ref_names, char*, heuristic_refs);
    GGIT_VECTOR_DEFINE(ref_hashes, char[40], heuristic_refs);
    /* TODO: can use the size of the ref_names. */
    GGIT_VECTOR_DEFINE(ref_y, int, heuristic_refs);

    GGIT_VECTOR_DEFINE(commit_messages, char*, heuristic_commits);
    GGIT_VECTOR_DEFINE(commit_message_lengths, int, heuristic_commits);
    /* TODO: change to char[40] */
    GGIT_VECTOR_DEFINE(commit_hashes, char*, heuristic_commits);
    GGIT_VECTOR_DEFINE(commit_parents, struct ggit_commit_parents, heuristic_commits);
    GGIT_VECTOR_DEFINE(commit_tags, struct ggit_commit_tag, heuristic_commits);

    ggit_load_refs(refs_len, refs, &ref_names, &ref_hashes);

    int last_pipe = -1;
    int n = 0;
    int parts[8];
    /* NOTE(boz):
        Structure of logs:
            COMMIT_HASH|PARENT_0_HASH PARENT_1_HASH|COMMIT_MESSAGE_SUBJECT

        Example:
            4b9a43a|bae4937 7862c77|code2            <- 2 parents commit
            bae4937|7862c77|code                     <- 1 parent  commit
            7862c77|45a8e25|progress: Git graph.     <- 1 parent  commit
            45a8e25||z: Initial commit               <- 0 parents commit
    */
    for (int i = 0; i < gitlog_len; ++i) {
        switch (gitlog[i]) {
            case '|':
                parts[2 * n] = last_pipe + 1;
                parts[2 * n + 1] = i;
                last_pipe = i;
                if (n == 1) {
                    n += 1;
                    /* NOTE(boz):
                        Single-parent commit, fill the missing parent.
                    */
                    parts[2 * n] = i;
                    parts[2 * n + 1] = i;
                }
                n += 1;
                break;
            case ' ':
                if (n == 1) {
                    parts[2 * n] = last_pipe + 1;
                    parts[2 * n + 1] = i;
                    last_pipe = i;
                    ++n;
                }
                break;
            case '\0':
            case '\n': {
                parts[2 * n] = last_pipe + 1;
                parts[2 * n + 1] = i;
                last_pipe = i;
                n = 0;

                int p0_len = parts[3] - parts[2];
                int p1_len = parts[5] - parts[4];
                char const* p0_hash = gitlog + parts[2];
                char const* p1_hash = gitlog + parts[4];

                struct ggit_commit_parents parents = { -1, -1 };
                /* PERF(boz):
                    THIS IS VERY SLOW!!!
                    Implement a hashmap to MASSIVELY speed this up.
                */
                for (int p = commit_hashes.size - 1; p >= 0; --p) {
                    char* commit_hash = ggit_vector_get_string(&commit_hashes, p);
                    if (parents.parent[0] == -1 && p0_len &&
                        0 == strncmp(commit_hash, p0_hash, p0_len)) {
                        parents.parent[0] = p;
                        if (!p1_len || parents.parent[1] != -1)
                            break;
                    } else if (parents.parent[1] == -1 && p1_len && 0 == strncmp(commit_hash, p1_hash, p1_len)) {
                        parents.parent[1] = p;
                        if (!p0_len || parents.parent[0] != -1)
                            break;
                    }
                }
                ggit_vector_push(&commit_parents, &parents);

                int msg_len = parts[7] - parts[6];
                char* msg = strndup(msg_len, gitlog + parts[6]);
                char* hash = strndup(parts[1] - parts[0], gitlog + parts[0]);
                ggit_vector_push(&commit_message_lengths, &msg_len);
                ggit_vector_push(&commit_messages, &msg);
                ggit_vector_push(&commit_hashes, &hash);
                struct ggit_commit_tag tags = {
                    ggit_primary_tag_none,
                    ggit_primary_tag_none,
                };
                ggit_vector_push(&commit_tags, &tags);
                // debug_print_parsed_commit(gitlog, parts);
            }
        }
    }

    ggit_match_refs_to_commits(&ref_hashes, &commit_hashes, &ref_y);
    int w0 = 0;
    for (int i = 0; i < ref_y.size; ++i) {
        char* ref_name = ggit_vector_get_string(&ref_names, i);
        int index = ggit_vector_get_i32(&ref_y, i);
        short* tags = ggit_vector_ref_commit_tags(&commit_tags, index)->tag;

        short ref_tag = ggit_refname_to_tag(ref_name);
        if (ref_tag != ggit_primary_tag_none) {
            tags[0] = ref_tag;
        }
        w0 = max(ref_tag, w0);
    }
    int w1 = ggit_label_merge_commits(
        &commit_message_lengths,
        &commit_messages,
        &commit_parents,
        &commit_tags
    );
    ggit_propagate_tags(&commit_parents, &commit_tags);
    out_graph->width = max(w0, w1);

    out_graph->hashes = (char**)commit_hashes.data;
    out_graph->message_lengths = (int*)commit_message_lengths.data;
    out_graph->parents = (struct ggit_commit_parents*)commit_parents.data;
    out_graph->messages = (char**)commit_messages.data;
    out_graph->tags = (struct ggit_commit_tag*)commit_tags.data;
    out_graph->height = commit_messages.size;
    return 0;
}

int
ggit_graph_init(struct ggit_graph* graph)
{
    memset(graph, 0, sizeof(*graph));
    return 0;
}
void
ggit_graph_clear(struct ggit_graph* graph)
{
    graph->width = 0;
    graph->height = 0;
}
void
ggit_graph_destroy(struct ggit_graph* graph)
{
    for (int i = 0; i < graph->height; ++i)
        free(graph->messages[i]);

    graph->width = 0;
    graph->height = 0;
    free(graph->message_lengths);
    free(graph->messages);
    free(graph->hashes);
    free(graph->parents);
    free(graph->tags);
}
int
ggit_graph_load(struct ggit_graph* graph, char const* path_repository)
{
    time_t start = time(0);
    time_t took;

    char cmd_load_commits[512];
    char cmd_load_refs[512];
    sprintf_s(
        cmd_load_commits,
        sizeof(cmd_load_commits),
        "git -C \"%s\" log --reverse --all --pretty=format:\"%%h|%%p|%%s\"",
        path_repository
    );
    sprintf_s(
        cmd_load_refs,
        sizeof(cmd_load_refs),
        "git -C \"%s\" show-ref",
        path_repository
    );

    char* gitlog;
    int gitlog_len;
    ggit_run(cmd_load_commits, &gitlog, &gitlog_len);
    took = time(0) - start;
    printf("Loading commits took %llds\n", took);

    start += took;
    char* refs;
    int refs_len;
    ggit_run(cmd_load_refs, &refs, &refs_len);
    took = time(0) - start;
    printf("Loading branches took %llds\n", took);

    start += took;
    ggit_graph_load_repository(gitlog_len, gitlog, refs_len, refs, graph);
    took = time(0) - start;
    printf("Parsing took %llds\n", took);

    free(refs);
    free(gitlog);

    return 0;
}

#include "ggit-graph.h"
#include "ggit-vector.h"
#include "ggit-git.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include <libsmallregex.h>


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
            case '\0': break;
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


static bool
ggit_parse_merge_commit__bitbucket_0(
    int message_length,
    char const* merge_commit_message,
    char** out_name_main,
    char** out_name_kink
)
{
    /* Like: Merged in build/update-installer-output-paths (pull request #287) */

    char const start_0[] = "Merged in ";
    int start_0_strlen = sizeof(start_0) - 1;
    if (!starts_with(merge_commit_message, start_0))
        /* Not a bitbucket merge */
        return false;

    int next_space = index_of(
        message_length - start_0_strlen,
        merge_commit_message + start_0_strlen,
        ' '
    );
    int open_paren = index_of(
        message_length - start_0_strlen,
        merge_commit_message + start_0_strlen,
        '('
    );

    int end = (min(open_paren, next_space)) - 1;

    if (next_space <= 0 && open_paren <= 0)
        return false;

    *out_name_main = 0;
    *out_name_kink = strndup(next_space, merge_commit_message + start_0_strlen);
    return true;
}
static bool
ggit_parse_merge_commit__bitbucket_1(
    int message_length,
    char const* merge_commit_message,
    char** out_name_main,
    char** out_name_kink
)
{
    /* Like: Merged A into B (pull request #287) */

    char const start_0[] = "Merged ";
    int start_0_strlen = sizeof(start_0) - 1;
    if (!starts_with(merge_commit_message, start_0))
        /* Not a bitbucket merge */
        return false;

    int next_space = index_of(
        message_length - start_0_strlen,
        merge_commit_message + start_0_strlen,
        ' '
    );

    if (next_space <= 0)
        return false;

    char const* kink_name = merge_commit_message + start_0_strlen;
    int kink_strlen = next_space;

    if (0 != memcmp(kink_name + kink_strlen, " into ", 6))
        return false;

    char const* main_name = kink_name + kink_strlen + 6;
    int main_strlen = message_length - start_0_strlen - 6 - kink_strlen;
    next_space = index_of(main_strlen, main_name, ' ');
    if (next_space > 0) {
        main_strlen = next_space;
    } else {
        int open_paren = index_of(main_strlen, main_name, '(');
        if (open_paren > 0) {
            main_strlen = open_paren;
        }
    }

    *out_name_main = strndup(main_strlen, main_name);
    *out_name_kink = strndup(kink_strlen, kink_name);
    return true;
}

/* TODO:
    clean up the internals
*/
static bool
ggit_parse_merge_commit__git_local(
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
    if (closing_quote > 0) {
        int kink_len = closing_quote;
        char* kink_name = strndup(kink_len, merge_commit_message + start_0_strlen);
        if (closing_quote + start_0_strlen + 1 != message_length) {
            // into 'branch_main'
            char const* leftover_message = merge_commit_message + start_0_strlen
                                           + closing_quote + 1;
            char const* main_name;
            if (starts_with(leftover_message, " into ")) {
                main_name = leftover_message + 6;
            } else {
                main_name = "master";
            }
            *out_name_main = _strdup(main_name);
        } else {
            *out_name_main = _strdup("master");
        }
        *out_name_kink = kink_name;
    } else {
        return false;
    }
    return true;
}
static bool
ggit_parse_merge_commit__git_remote(
    int message_length,
    char const* merge_commit_message,
    char** out_name_main,
    char** out_name_kink
)
{
    /* NOTE(boz):
        Original regex:
        "Merge remote-tracking branch 'origin/(.+?)'(?: into (.+))?"
    */
    static char const start_0[] = "Merge remote-tracking branch '";
    int start_0_strlen = sizeof(start_0) - 1;
    if (!starts_with(merge_commit_message, start_0))
        return false;

    /* TODO: implement. */
    return false;
}
static bool
ggit_parse_merge_commit(
    int message_length,
    char const* merge_commit_message,
    char** out_name_main,
    char** out_name_kink
)
{
    return ggit_parse_merge_commit__git_local(
               message_length,
               merge_commit_message,
               out_name_main,
               out_name_kink
           )
           || ggit_parse_merge_commit__git_remote(
               message_length,
               merge_commit_message,
               out_name_main,
               out_name_kink
           )
           || ggit_parse_merge_commit__bitbucket_0(
               message_length,
               merge_commit_message,
               out_name_main,
               out_name_kink
           )
           || ggit_parse_merge_commit__bitbucket_1(
               message_length,
               merge_commit_message,
               out_name_main,
               out_name_kink
           )
           || 0;
}

static int
ggit_match_refs_to_commits(
    struct ggit_vector* restrict ref_hashes,
    struct ggit_vector* restrict commit_hashes,
    struct ggit_vector* restrict out_ref_commits
)
{
    int count_refs = ref_hashes->size;
    int count_commits = commit_hashes->size;
    for (int r = 0; r < count_refs; ++r) {
        bool found = false;
        char* ref_hash = ggit_vector_get(ref_hashes, r);

        for (int c = 0; c < count_commits; ++c) {
            char* commit_hash = ggit_vector_get_string(commit_hashes, c);
            int commit_hash_len = (int)strlen(commit_hash);

            if (0 == strncmp(commit_hash, ref_hash, commit_hash_len)) {
                ggit_vector_push(out_ref_commits, &c);
                found = true;
                break;
            }
        }
        if (!found) {
            int commit = -1;
            ggit_vector_push(out_ref_commits, &commit);
        }
    }

    return 0;
}
struct ggit_commit_tag
ggit_branch_to_tag(char const* ref_name, struct ggit_vector* special_branches)
{
    struct ggit_commit_tag tag = { { -1, -1 }, true };
    if (strcmp(ref_name, "HEAD") == 0)
        goto end;

    for (int i = 0; i < special_branches->size; ++i) {
        struct ggit_special_branch* sb = ggit_vector_ref_special_branch(
            special_branches,
            i
        );

        if (regex_matchp(sb->regex, ref_name) == 0) {
            int n_instances = sb->instances.size;
            tag.tag[0] = i;
            for (int j = 0; j < n_instances; ++j) {
                char* instance_name = ggit_vector_get_string(&sb->instances, j);

                if (0 == strcmp(ref_name, instance_name)) {
                    tag.tag[1] = j;
                    break;
                }
            }

            if (tag.tag[1] == -1) {
                tag.tag[1] = n_instances;
                char* sub_name = _strdup(ref_name);
                ggit_vector_push(&sb->instances, &sub_name);
            }
            break;
        }
    }
end:
    if (tag.tag[0] == -1)
        fprintf(stderr, "Ignoring ref %s - no matching special branches.\n", ref_name);
    return tag;
}

static struct ggit_commit_tag
ggit_refname_to_tag(char const* ref_name, struct ggit_vector* special_branches)
{
    char const prefix_local_branch[] = "refs/heads/";
    char const prefix_stash[] = "refs/stash";
    char const prefix_tags[] = "refs/tags/";
    char const prefix_remote_branch[] = "refs/remotes/origin/";
    /* TODO: support git tags and other things. */
    if (starts_with(ref_name, prefix_local_branch))
        return ggit_branch_to_tag(
            ref_name + sizeof(prefix_local_branch) - 1,
            special_branches
        );
    else if (starts_with(ref_name, prefix_remote_branch))
        return ggit_branch_to_tag(
            ref_name + sizeof(prefix_remote_branch) - 1,
            special_branches
        );
    else if (starts_with(ref_name, prefix_stash))
        return ggit_branch_to_tag(
            ref_name + sizeof(prefix_stash) - 1,
            special_branches
        );
    else if (starts_with(ref_name, prefix_tags))
        return ggit_branch_to_tag(ref_name + sizeof(prefix_tags) - 1, special_branches);
    return (struct ggit_commit_tag){ { -1, -1 }, false };
}

static int
ggit_label_merge_commits(
    struct ggit_vector* restrict commit_message_lengths,
    struct ggit_vector* restrict commit_messages,
    struct ggit_vector* restrict commit_parents,
    struct ggit_vector* restrict commit_tags,
    struct ggit_vector* restrict special_branches
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

        struct ggit_commit_tag tag_main = { { -1, -1 }, false };
        struct ggit_commit_tag tag_kink = { { -1, -1 }, false };
        if (name_main)
            tag_main = ggit_branch_to_tag(name_main, special_branches);
        if (name_kink)
            tag_kink = ggit_branch_to_tag(name_kink, special_branches);

        struct ggit_commit_tag* my_tags = ggit_vector_ref_commit_tags(commit_tags, c);
        struct ggit_commit_tag* p0_tags = ggit_vector_ref_commit_tags(
            commit_tags,
            my_parents[0]
        );
        struct ggit_commit_tag* p1_tags = ggit_vector_ref_commit_tags(
            commit_tags,
            my_parents[1]
        );

        if (tag_main.tag[0] == -1)
            tag_main = *my_tags;

        *my_tags = *p0_tags = tag_main;
        *p1_tags = tag_kink;

        free(name_main);
        free(name_kink);

        width = max(tag_main.tag[0], width);
        width = max(tag_kink.tag[0], width);
    }
    return width;
}
/** Take commits with tags and recursively tag the parents of the commits, according to
 * some rules.
 *
 * Initially (in ggit_graph_init), only branch-heads are tagged with their respective
 * tags. This function tags all the parents of those commits, recursively, applying the
 * left-right rule (or main-kink rule).
 */
static void
ggit_propagate_tags(
    struct ggit_vector* restrict commit_parents,
    struct ggit_vector* restrict commit_tags
)
{
    int count = commit_parents->size;
    assert(commit_tags->size == count);
    for (int c = count - 1; c >= 0; --c) {
        int const my_parent = ggit_vector_ref_commit_parents(commit_parents, c)
                                  ->parent[0];
        if (my_parent != -1) {
            struct ggit_commit_tag const* my_tags = ggit_vector_ref_commit_tags(
                commit_tags,
                c
            );
            struct ggit_commit_tag* p0_tags = ggit_vector_ref_commit_tags(
                commit_tags,
                my_parent
            );
            if (p0_tags->strong) {
                continue;
            } else {
                if (p0_tags->tag[0] == -1 || p0_tags->tag[0] > my_tags->tag[0]) {
                    *p0_tags = *my_tags;
                    p0_tags->strong = false;
                }
            }
        }
    }
}

static void
expand_span_merges(struct ggit_column_span* span, int i)
{
    span->merge_max = max(span->merge_max, i);
    span->merge_min = min(span->merge_min, i);
}
static void
expand_span_commits(struct ggit_column_span* span, int i)
{
    span->commit_max = max(span->commit_max, i);
    span->commit_min = min(span->commit_min, i);
    expand_span_merges(span, i);
}
void
ggit_compute_column_spans(struct ggit_graph* graph)
{
    for (int i = 0; i < graph->special_branches.size; ++i) {
        struct ggit_special_branch* sb = ggit_vector_ref_special_branch(
            &graph->special_branches,
            i
        );
        ggit_vector_reserve(&sb->spans, sb->instances.size);
        sb->spans.size = sb->instances.size;
        for (int j = 0; j < sb->instances.size; ++j) {
            struct ggit_column_span span = {
                .merge_min = INT32_MAX,
                .commit_min = INT32_MAX,
                .commit_max = INT32_MIN,
                .merge_max = INT32_MIN,
            };
            // "PUSH"
            *ggit_vector_ref_column_span(&sb->spans, j) = span;
        }
    }

    for (int i = 0; i < graph->height; ++i) {
        struct ggit_commit_tag tag = graph->tags[i];
        struct ggit_commit_parents parents = graph->parents[i];

        if (tag.tag[0] == -1 || tag.tag[1] == -1)
            continue;

        struct ggit_special_branch* sb = ggit_vector_ref_special_branch(
            &graph->special_branches,
            tag.tag[0]
        );
        struct ggit_column_span* span = ggit_vector_ref_column_span(
            &sb->spans,
            tag.tag[1]
        );
        expand_span_commits(span, i);

        for (int j = 0; j < 2; ++j) {
            int const parent = parents.parent[j];

            if (parent != -1) {
                expand_span_merges(span, parent);

                struct ggit_commit_tag p_tag = graph->tags[parent];

                if (p_tag.tag[0] != -1) {
                    struct ggit_special_branch* p_sb = ggit_vector_ref_special_branch(
                        &graph->special_branches,
                        p_tag.tag[0]
                    );
                    struct ggit_column_span* p_span = ggit_vector_ref_column_span(
                        &p_sb->spans,
                        p_tag.tag[1]
                    );
                    expand_span_merges(p_span, i);
                }
            }
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

    ggit_graph_clear(out_graph);

    GGIT_VECTOR_DEFINE(commit_messages, char*, heuristic_commits);
    GGIT_VECTOR_DEFINE(commit_message_lengths, int, heuristic_commits);
    /* TODO: change to char[40] */
    GGIT_VECTOR_DEFINE(commit_hashes, char*, heuristic_commits);
    GGIT_VECTOR_DEFINE(commit_parents, struct ggit_commit_parents, heuristic_commits);
    GGIT_VECTOR_DEFINE(commit_tags, struct ggit_commit_tag, heuristic_commits);

    ggit_load_refs(refs_len, refs, &out_graph->ref_names, &out_graph->ref_hashes);

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
                    if (parents.parent[0] == -1 && p0_len
                        && 0 == strncmp(commit_hash, p0_hash, p0_len)) {
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
                struct ggit_commit_tag tags = { { -1, -1 }, false };
                ggit_vector_push(&commit_tags, &tags);
            }
        }
    }

    ggit_match_refs_to_commits(
        &out_graph->ref_hashes,
        &commit_hashes,
        &out_graph->ref_commits
    );
    int w0 = 0;
    for (int i = 0; i < out_graph->ref_commits.size; ++i) {
        char* ref_name = ggit_vector_get_string(&out_graph->ref_names, i);
        int index = ggit_vector_get_i32(&out_graph->ref_commits, i);
        if (index == -1)
            continue;
        struct ggit_commit_tag* tags = ggit_vector_ref_commit_tags(&commit_tags, index);

        struct ggit_commit_tag ref_tag = ggit_refname_to_tag(
            ref_name,
            &out_graph->special_branches
        );
        if (ref_tag.tag[0] != -1) {
            *tags = ref_tag;
        }
        w0 = max(ref_tag.tag[0], w0);
    }
    int w1 = ggit_label_merge_commits(
        &commit_message_lengths,
        &commit_messages,
        &commit_parents,
        &commit_tags,
        &out_graph->special_branches
    );
    ggit_propagate_tags(&commit_parents, &commit_tags);
    out_graph->width = max(w0, w1);
    out_graph->width = 0;
    for (int i = 0; i < out_graph->special_branches.size; ++i) {
        struct ggit_special_branch* branch = ggit_vector_ref_special_branch(
            &out_graph->special_branches,
            i
        );
        out_graph->width += branch->instances.size;
    }

    out_graph->hashes = (char**)commit_hashes.data;
    out_graph->message_lengths = (int*)commit_message_lengths.data;
    out_graph->parents = (struct ggit_commit_parents*)commit_parents.data;
    out_graph->messages = (char**)commit_messages.data;
    out_graph->tags = (struct ggit_commit_tag*)commit_tags.data;
    out_graph->height = commit_messages.size;

    ggit_compute_column_spans(out_graph);
    return 0;
}

int
ggit_graph_init(struct ggit_graph* graph)
{
    memset(graph, 0, sizeof(*graph));

    ggit_git_init(&graph->git);

    ggit_vector_init(&graph->special_branches, sizeof(struct ggit_special_branch));

    ggit_vector_init(&graph->ref_names, sizeof(char*));
    ggit_vector_init(&graph->ref_hashes, sizeof(char[40]));
    ggit_vector_init(&graph->ref_commits, sizeof(int));
    return 0;
}
void
ggit_graph_clear(struct ggit_graph* graph)
{
    free(graph->message_lengths);

    for (int i = 0; i < graph->height; ++i)
        free(graph->messages[i]);
    free(graph->messages);

    for (int i = 0; i < graph->height; ++i)
        free(graph->hashes[i]);
    free(graph->hashes);

    free(graph->parents);
    free(graph->tags);

    graph->width = 0;
    graph->height = 0;

    for (int i = 0; i < graph->special_branches.size; ++i) {
        ggit_special_branch_clear(
            ggit_vector_ref_special_branch(&graph->special_branches, i)
        );
    }

    ggit_vector_clear_and_free(&graph->ref_names);
    ggit_vector_clear(&graph->ref_hashes);
    ggit_vector_clear(&graph->ref_commits);
}
void
ggit_graph_destroy(struct ggit_graph* graph)
{
    free(graph->path);
    ggit_git_destroy(&graph->git);
    ggit_graph_clear(graph);

    for (int i = 0; i < graph->special_branches.size; ++i) {
        ggit_special_branch_destroy(
            ggit_vector_ref_special_branch(&graph->special_branches, i)
        );
    }
    ggit_vector_destroy(&graph->special_branches);

    ggit_vector_destroy(&graph->ref_names);
    ggit_vector_destroy(&graph->ref_hashes);
    ggit_vector_destroy(&graph->ref_commits);
}
int
ggit_graph_load(struct ggit_graph* graph, char const* path_repository)
{
    if (graph->path)
        free(graph->path);
    graph->path = _strdup(path_repository);

    ggit_graph_reload(graph);
    return 0;
}

/* Moves all data from the old vector to the new vector, zeroing the input vector(old).
 */
struct ggit_vector
ggit_vector_copy(struct ggit_vector* in)
{
    int const value_size = in->value_size;
    int const size = in->size;
    struct ggit_vector new = { .value_size = value_size };
    ggit_vector_push_array(&new, size, in->data);
    return new;
}

int
ggit_graph_reload(struct ggit_graph* graph)
{
    struct ggit_git* ggg = &graph->git;

    time_t start = time(0);
    time_t end = 0;
    time_t took = 0;

    int status_commits;
    int status_refs;
    struct ggit_vector git_out_commits;
    struct ggit_vector git_out_refs;
    {
        ggit_git_clear(ggg);
        ggit_vector_push_sprintf_terminated(
            &ggg->command,
            "git -C \"%s\" log --reverse --all --pretty=format:\"%%h|%%p|%%s\"",
            graph->path
        );
        status_commits = ggit_git_run(ggg);
        git_out_commits = ggit_vector_copy(&ggg->out);

        end = time(0);
        took = end - start;
        start = end;
        printf("ggit_graph_reload: Loading commits took %llds\n", took);
    }
    {
        ggit_git_clear(ggg);
        ggit_vector_push_sprintf_terminated(
            &ggg->command,
            "git -C \"%s\" show-ref",
            graph->path
        );
        status_refs = ggit_git_run(ggg);
        git_out_refs = ggit_vector_copy(&ggg->out);

        end = time(0);
        took = end - start;
        start = end;
        printf("ggit_graph_reload: Loading branches took %llds\n", took);
    }
    ggit_graph_load_repository(
        git_out_commits.size,
        git_out_commits.data,
        git_out_refs.size,
        git_out_refs.data,
        graph
    );
    end = time(0);
    took = end - start;
    start = end;
    printf("ggit_graph_reload: Parsing took %llds\n", took);

    ggit_vector_destroy(&git_out_refs);
    ggit_vector_destroy(&git_out_commits);
    return 0;
}

void
ggit_special_branch_clear(struct ggit_special_branch* sb)
{
    ggit_vector_clear_and_free(&sb->instances);
    ggit_vector_clear(&sb->spans);
}
void
ggit_special_branch_destroy(struct ggit_special_branch* sb)
{
    free(sb->name);
    regex_free(sb->regex);
    ggit_vector_destroy(&sb->instances);
    ggit_vector_destroy(&sb->spans);
}

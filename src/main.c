#include "git2/commit.h"
#include "git2/oid.h"
#include "git2/remote.h"
#include "git2/repository.h"
#include "git2/revwalk.h"
#include "git2/tag.h"
#include <argp.h>
#include <bits/stdint-uintn.h>
#include <git2.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>

// ERRORS
typedef enum {
    ERR_PARSE_ARGS = 5,
    ERR_NO_REPOSITORY = 10,
    ERR_LATEST_TAG_NOT_FOUND = 30,
    ERR_TAG_NOT_CREATED = 40,
    ERR_NO_TAGS_NO_AUTO_INIT = 50,
    ERR_INVALID_INIT_TAG = 60,
    ERR_NO_COMMITS = 70,
} corel_error;

static corel_error corel_last_error = 0;
#define ERROR(err) corel_last_error = err;

#define BOAST(...)                                                                                                                                             \
    if (!args.quiet) {                                                                                                                                         \
        printf(__VA_ARGS__);                                                                                                                                   \
        printf("\n");                                                                                                                                          \
    }

#define BOAST_ERR(...)                                                                                                                                         \
    printf("ERROR: ");                                                                                                                                         \
    printf(__VA_ARGS__);                                                                                                                                       \
    printf("\n");

#ifdef DEBUG
#define BOAST_DBG(...)                                                                                                                                         \
    if (!args.quiet) {                                                                                                                                         \
        printf("DEBUG: ");                                                                                                                                     \
        BOAST(__VA_ARGS__);                                                                                                                                    \
    }
#else
#define BOAST_DBG(...)
#endif

#define GIT_COMMIT_HEAD NULL

#define ARG_DRY_RUN_SHORT 0x80
#define ARG_PRINT_VERSION_SHORT 0x81
#define ARG_PATH_SHORT 0x82
#define ARG_INIT_VERSION_SHORT 0x83
#define ARG_AUTO_INIT_VERSION_SHORT 0x84
#define ARG_NO_PUSH_SHORT 0x85

#define SEMVER_REGEX                                                                                                                                           \
    "^v?(0|[1-9][[:digit:]]*)\\.(0|[1-9][[:digit:]]*)\\.(0|[1-9][[:digit:]]*)?(-[[:alnum:]-]+(\\.[[:alnum:]-]+)*)?(\\+[[:alnum:]-]+(\\.[[:alnum:]-]+)*)?$"

/* Technically we don't need to keep this, but I will keep it here for documentation purposes */
#define PATCH_REGEX "^(build|chore|ci|docs|env|fix|perf|revert|style|test)\\s?(\\(.+\\))?\\s?:\\s*(.+)"
#define MINOR_REGEX "^(feat|refactor)\\s?(\\(.+\\))?\\s?:\\s*(.+)"
#define MAJOR_REGEX "^(BREAKING CHANGE)\\s?(\\(.+\\))?\\s?:\\s*(.+)"

typedef struct {
    bool quiet;
    bool print_version;
    bool dry_run;
    bool no_push;
    bool auto_init_tag;
    char *init_version;
    char *repo_path;
} cli_args;

static cli_args args;
static regex_t semver_regex = {0};
static regex_t major_regex = {0};
static regex_t minor_regex = {0};
static regex_t patch_regex = {0};

static struct argp_option options[] = {
    {"quiet", 'q', 0, 0, "Only show important output", 0},
    {"print-version", ARG_PRINT_VERSION_SHORT, 0, 0, "Only prints the current version of the git repository", 0},
    {"dry-run", ARG_DRY_RUN_SHORT, 0, 0, "Do not make any changes to the repository", 0},
    {"repository-path", ARG_PATH_SHORT, "path", 0, "Path to the git repository", 0},
    {"auto-init-tag", ARG_AUTO_INIT_VERSION_SHORT, NULL, 0, "Creates the initial tag by analyzing all current commits starting from --initial-version", 0},
    {"initial-version", ARG_INIT_VERSION_SHORT, "version", 0, "The version to start from. Defaults to v0.1.0", 0},
    {"no-push", ARG_NO_PUSH_SHORT, NULL, 0, "Tags will only be created locally and not pushed to the remote", 0},
    {0},
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    cli_args *arguments = state->input;
    switch (key) {
    case 'q':
        arguments->quiet = true;
        break;
    case ARG_DRY_RUN_SHORT:
        arguments->dry_run = true;
        break;
    case ARG_PRINT_VERSION_SHORT:
        arguments->print_version = true;
        break;
    case ARG_PATH_SHORT:
        arguments->repo_path = arg;
        break;
    case ARG_INIT_VERSION_SHORT:
        arguments->init_version = arg;
        break;
    case ARG_AUTO_INIT_VERSION_SHORT:
        arguments->auto_init_tag = true;
        break;
    case ARG_NO_PUSH_SHORT:
        arguments->no_push = true;
        break;
    case ARGP_KEY_ARG:
        return 0;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

typedef struct {
    uint64_t major;
    uint64_t minor;
    uint64_t patch;
} corel_ver;

typedef struct {
    char *name;
    corel_ver ver;
} corel_taginfo;

corel_taginfo *corel_taginfo_parse(char *tag_name) {
#define MAX_MATCHES 4
#define IDX_MAJOR 1
#define IDX_MINOR 2
#define IDX_PATCH 3

    regmatch_t matches[MAX_MATCHES];
    if (regexec(&semver_regex, tag_name, MAX_MATCHES, matches, 0) == REG_NOMATCH) {
        return NULL;
    }

    corel_taginfo *tag_info = malloc(sizeof(corel_taginfo));
    tag_info->name = tag_name;
    tag_info->ver = (corel_ver){
        .major = 0,
        .minor = 0,
        .patch = 0,
    };

    char str[strlen(tag_name)];

    for (size_t i = 0; i < MAX_MATCHES; ++i) {
        if (i == 0) {
            continue;
        }
        regmatch_t match = matches[i];
        size_t len = match.rm_eo - match.rm_so;
        if (len == 0) {
            continue;
        }
        for (size_t k = 0; k < len; k++) {
            str[k] = tag_name[match.rm_so + k];
        }
        str[len] = 0;

        int version = atoi(str);
        switch (i) {
        case IDX_MAJOR:
            tag_info->ver.major = version;
            break;
        case IDX_MINOR:
            tag_info->ver.minor = version;
            break;
        case IDX_PATCH:
            tag_info->ver.patch = version;
            break;
        default:
            break;
        }
    }
    return tag_info;
}

int corel_then_compare(int prev, int next) {
    if (prev == 0) {
        return next;
    } else {
        return prev;
    }
}

int corel_taginfo_cmp(corel_taginfo *t1, corel_taginfo *t2) {
#define CMP(target) t1->ver.target > t2->ver.target ? 1 : (t1->ver.target < t2->ver.target ? -1 : 0)
    int res = CMP(major);
    res = corel_then_compare(res, CMP(minor));
    res = corel_then_compare(res, CMP(patch));
    return res;
}

void corel_taginfo_free(corel_taginfo *tag_info) {
    free(tag_info);
}

void corel_taginfo_print(corel_taginfo *tag) {
    if (tag) {
        BOAST("Tag: %s (%lu %lu %lu)", tag->name, tag->ver.major, tag->ver.minor, tag->ver.patch)
    }
}

int corel_cli_parse_args(int argc, char *argv[], cli_args *args) {
    struct argp argp = {options, parse_opt, 0, 0, 0, 0, 0};

    args->print_version = false;
    args->dry_run = false;
    args->quiet = false;
    args->repo_path = ".";
    args->auto_init_tag = false;
    args->no_push = false;
    args->init_version = "v0.1.0";

    error_t err = argp_parse(&argp, argc, argv, 0, 0, args);

    return err;
}

#define DYNAMIC_ARRAY(array_name, target_struct)                                                                                                               \
    typedef struct {                                                                                                                                           \
        target_struct **entries;                                                                                                                               \
        u_int64_t len;                                                                                                                                         \
        u_int64_t capacity;                                                                                                                                    \
    } array_name;                                                                                                                                              \
                                                                                                                                                               \
    void array_name##_init(array_name **out, u_int64_t capacity) {                                                                                             \
        array_name *new = malloc(sizeof(corel_commit_array));                                                                                                  \
        new->entries = calloc(capacity, sizeof(git_commit *));                                                                                                 \
        new->len = 0;                                                                                                                                          \
        new->capacity = capacity;                                                                                                                              \
        *out = new;                                                                                                                                            \
    }                                                                                                                                                          \
                                                                                                                                                               \
    void array_name##_free(array_name *arr) {                                                                                                                  \
        for (size_t i = 0; i < arr->len; ++i) {                                                                                                                \
            target_struct##_free(arr->entries[i]);                                                                                                             \
        }                                                                                                                                                      \
        free(arr->entries);                                                                                                                                    \
        free(arr);                                                                                                                                             \
    }                                                                                                                                                          \
                                                                                                                                                               \
    int array_name##_push(array_name *arr, target_struct *target) {                                                                                            \
        if (arr->capacity <= arr->len) {                                                                                                                       \
            size_t new_capacity = arr->capacity * 2;                                                                                                           \
            void *tmp = realloc(arr->entries, new_capacity * sizeof(target_struct *));                                                                         \
            if (!tmp) {                                                                                                                                        \
                return 10;                                                                                                                                     \
            }                                                                                                                                                  \
            arr->entries = tmp;                                                                                                                                \
            arr->capacity = new_capacity;                                                                                                                      \
        }                                                                                                                                                      \
                                                                                                                                                               \
        arr->entries[arr->len] = target;                                                                                                                       \
        arr->len++;                                                                                                                                            \
        return 0;                                                                                                                                              \
    }

DYNAMIC_ARRAY(corel_commit_array, git_commit);
DYNAMIC_ARRAY(corel_tag_array, git_tag);

void corel_commit_array_collect(corel_commit_array **out, git_repository *repository, git_commit *since, u_int64_t max) {
    corel_commit_array_init(out, 1);

    git_revwalk *walk;
    git_revwalk_new(&walk, repository);
    git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME | GIT_SORT_REVERSE);

    if (since) {
        // git_revwalk_push(walk, git_commit_id(since));
        git_revwalk_push_head(walk);
        git_revwalk_hide(walk, git_commit_id(since));
    } else {
        git_revwalk_push_head(walk);
    }

    git_commit *curr_commit = NULL;
    git_oid oid;
    while (git_revwalk_next(&oid, walk) == 0) {
        if (git_commit_lookup(&curr_commit, repository, &oid) == 0) {
            corel_commit_array_push(*out, curr_commit);
        }

        if (max > 0 && (*out)->len > max) {
            break;
        }
    }

    u_int64_t count = (*out)->len;
    if (count > 0) {
        BOAST("Woaah, you have %lu commit(s)", count);
    }

    git_revwalk_free(walk);
}

typedef enum {
    MAJOR,
    MINOR,
    PATCH,
    NONE,
} COREL_RELEASE_BUMP;

COREL_RELEASE_BUMP corel_analyze_commit_message(char *commit_message) {
    if (regexec(&major_regex, commit_message, 0, NULL, 0) == 0) {
        return MAJOR;
    }
    if (regexec(&minor_regex, commit_message, 0, NULL, 0) == 0) {
        return MINOR;
    }
    return PATCH;
}

int corel_taginfo_commit(git_commit **out, corel_taginfo *tag, git_repository *repository) {
    git_object *obj;

    if (git_revparse_single(&obj, repository, tag->name) != 0) {
        return 1;
    }

    switch (git_object_type(obj)) {
    case GIT_OBJECT_COMMIT:
        *out = (git_commit *)obj;
        return 0;
    default:
        git_object_free(obj);
        return 1;
    }
}

char *corel_ver_tostr(corel_ver *version) {
#define VERSION_STR_MAX_ALLOC 64
    char *out = malloc(VERSION_STR_MAX_ALLOC);
    sprintf(out, "v%lu.%lu.%lu", version->major, version->minor, version->patch);
    return out;
}

void corel_ver_bump(corel_ver *version, COREL_RELEASE_BUMP type) {
    switch (type) {
    case MAJOR:
        if (version->major > 0) {
            version->major += 1;
            version->minor = 0;
            version->patch = 0;
            break;
        }
    case MINOR:
        version->minor += 1;
        version->patch = 0;
        break;
    case PATCH:
        version->patch += 1;
        break;
    case NONE:
        break;
    }
}

void corel_bump_version(corel_ver *version, corel_commit_array *commits, bool count_individually) {
    char *version_old = corel_ver_tostr(version);

    COREL_RELEASE_BUMP highest = NONE;
    for (size_t i = 0; i < commits->len; i++) {
        COREL_RELEASE_BUMP current = corel_analyze_commit_message(git_commit_message(commits->entries[i]));
        if (count_individually) {
            corel_ver_bump(version, current);
        } else if (current < highest) {
            highest = current;
        }
    }

    if (!count_individually) {
        corel_ver_bump(version, highest);
    }

    char *version_new = corel_ver_tostr(version);
    BOAST_DBG("Bumped Version from %s->%s in %lu commits", version_old, version_new, commits->len);
    free(version_old);
    free(version_new);
}

void corel_tag_now(char *tag_name, char *rev, git_repository *repository) {
#define GIT_REMOTE_CALLBACKS_VERSION 1
#define GIT_REMOTE_OPTIONS_VERSION 1
    git_object *target;
    git_oid created;
    git_revparse_single(&target, repository, rev);
    if (git_tag_create_lightweight(&created, repository, tag_name, target, false) == 0) {
        // if (!args.no_push) {
        //     git_remote *remote;
        //     if (git_remote_lookup(&remote, repository, "origin") != 0) {
        //         BOAST_ERR("Failed to lookup origin remote");
        //         goto cleanup;
        //     }
        //
        //     char *refspec = "refs/heads/master";
        //     const git_strarray refspecs = {&refspec, 1};
        //     git_push_options options;
        //     git_remote_callbacks callbacks;
        //
        //     git_remote_init_callbacks(&callbacks, GIT_REMOTE_CALLBACKS_VERSION);
        //     callbacks.credentials = cred_acquire_cb;
        //
        //     git_push_options_init(&options, GIT_REMOTE_OPTIONS_VERSION);
        //     options.callbacks = callbacks;
        //
        //     git_remote_push(remote, &refspecs, &options);
        //     BOAST("Pushed tag to origin");
        // }
    } else {
        ERROR(ERR_TAG_NOT_CREATED)
        BOAST_ERR("Failed to create tag %s", tag_name);
    }
    git_object_free(target);
}

void corel_try_auto_init(git_repository *repository) {
    if (!args.auto_init_tag) {
        ERROR(ERR_NO_TAGS_NO_AUTO_INIT)
        BOAST("No tags have been created yet and --auto-init-tag was not provided.");
        return;
    }

    BOAST("No tags have been created yet. Figuring out initial version, starting from %s", args.init_version);
    corel_taginfo *version = corel_taginfo_parse(args.init_version);
    if (!version) {
        ERROR(ERR_INVALID_INIT_TAG)
        BOAST_ERR("Could not parse initial version %s", args.init_version);
        return;
    }

    corel_commit_array *commits = {0};
    corel_commit_array_collect(&commits, repository, GIT_COMMIT_HEAD, -1);
    corel_bump_version(&version->ver, commits, true);

    if (!args.dry_run) {
        char *version_name = corel_ver_tostr(&version->ver);
        corel_tag_now(version_name, "HEAD", repository);
        free(version_name);
    }

    corel_commit_array_free(commits);
    corel_taginfo_free(version);
}

#define COMPILE_REGEX(target, regex)                                                                                                                           \
    if (regcomp(&target, regex, REG_EXTENDED | REG_ICASE) != 0) {                                                                                              \
        fprintf(stderr, "Error: could not compile %s regex.\n", #target);                                                                                      \
        return 1;                                                                                                                                              \
    };

int main(int argc, char *argv[]) {
    COMPILE_REGEX(semver_regex, SEMVER_REGEX);
    COMPILE_REGEX(major_regex, MAJOR_REGEX);
    COMPILE_REGEX(minor_regex, MINOR_REGEX);
    COMPILE_REGEX(patch_regex, PATCH_REGEX);

    if (corel_cli_parse_args(argc, argv, &args) != 0) {
        printf("Could not parse args\n");
        return ERR_PARSE_ARGS;
    }

    git_libgit2_init();
    BOAST("Corel v0.0.1"); // TODO: Replace with actual version

    git_repository *repository = NULL;
    corel_commit_array *commits = NULL;
    corel_taginfo *latest_tag = NULL;
    git_repository_open(&repository, args.repo_path);

    if (!repository) {
        ERROR(ERR_NO_REPOSITORY)
        BOAST_ERR("Provided path is not a git repository");
        return ERR_NO_REPOSITORY;
    }

    // Fast Lookup to see if we have any commits
    corel_commit_array_collect(&commits, repository, GIT_COMMIT_HEAD, 1);
    if (commits->len == 0) {
        ERROR(ERR_NO_COMMITS)
        BOAST_ERR("You have not made any commits yet. Why even run this?")
        goto cleanup;
    }
    corel_commit_array_free(commits);
    commits = NULL;

    BOAST("Grabbing tags...");
    git_strarray tag_names = {0};
    git_tag_list(&tag_names, repository);

    BOAST("Tags: %lu", tag_names.count);
    for (size_t i = 0; i < tag_names.count; i++) {
        corel_taginfo *tag = corel_taginfo_parse(tag_names.strings[i]);

        if (tag == NULL) {
            continue;
        }

        if (latest_tag == NULL || (corel_taginfo_cmp(tag, latest_tag)) > 0) {
            corel_taginfo_free(latest_tag);
            latest_tag = tag;
        } else {
            corel_taginfo_free(tag);
        }
    }

    if (latest_tag == NULL) {
        corel_try_auto_init(repository);
        git_strarray_free(&tag_names);
        goto cleanup;
    }

    BOAST("Found Latest Tag: ");
    corel_taginfo_print(latest_tag);

    git_commit *latest_tag_commit;
    if (corel_taginfo_commit(&latest_tag_commit, latest_tag, repository) != 0) {
        ERROR(ERR_LATEST_TAG_NOT_FOUND);
        BOAST_ERR("Failed to lookup commit for the latest tag. This should not happen!");
        git_strarray_free(&tag_names);
        goto cleanup;
    }

    BOAST_DBG("Latest Tag Refers to commit %s", git_commit_message(latest_tag_commit));

    corel_commit_array_collect(&commits, repository, latest_tag_commit, -1);
    corel_bump_version(&latest_tag->ver, commits, false);
    char *version_name = corel_ver_tostr(&latest_tag->ver);
    if (args.print_version) {
        printf("%s\n", version_name);
    } else if (!args.dry_run) {
        corel_tag_now(version_name, "HEAD", repository);
    }
    free(version_name);
    git_strarray_free(&tag_names);
    git_commit_free(latest_tag_commit);

cleanup:
    regfree(&semver_regex);
    if (commits) {
        corel_commit_array_free(commits);
    }
    if (repository) {
        git_repository_free(repository);
    }
    if (latest_tag) {
        corel_taginfo_free(latest_tag);
    }
    git_libgit2_shutdown();
    BOAST("Bye o/");
    return corel_last_error;
}

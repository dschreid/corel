#include "git2/repository.h"
#include "git2/revwalk.h"
#include "git2/tag.h"
#include <argp.h>
#include <git2.h>
#include <stdbool.h>
#include <stdio.h>

#define ARG_DRY_RUN_SHORT 0x80
#define ARG_PRINT_VERSION_SHORT 0x81
#define ARG_PATH_SHORT 0x82

typedef struct {
    bool quiet;
    bool print_version;
    bool dry_run;
    char *repo_path;
} cli_args;

static cli_args args;
#define BOAST(...)                                                                                                                                             \
    if (!args.quiet) {                                                                                                                                         \
        printf(__VA_ARGS__);                                                                                                                                   \
        printf("\n");                                                                                                                                          \
    }

#define BOAST_ERR(str) printf("ERROR: %s\n", str);

static struct argp_option options[] = {
    {"quiet", 'q', 0, 0, "Only show important output", 0},
    {"print-version", ARG_PRINT_VERSION_SHORT, 0, 0, "Print the next version", 0},
    {"dry-run", ARG_DRY_RUN_SHORT, 0, 0, "Print the next version", 0},
    {"repository-path", ARG_PATH_SHORT, "path", 0, "Print the next version", 0},
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
    case ARGP_KEY_ARG:
        return 0;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

int corel_cli_parse_args(int argc, char *argv[], cli_args *args) {
    struct argp argp = {options, parse_opt, 0, 0, 0, 0, 0};

    args->print_version = false;
    args->dry_run = false;
    args->quiet = false;
    args->repo_path = ".heehee";

    error_t err = argp_parse(&argp, argc, argv, 0, 0, args);

    return err;
}

int corel_count_commits(git_repository *repository) {
    BOAST("Counting commits...");
    git_revwalk *walk;
    git_revwalk_new(&walk, repository);
    git_revwalk_push_head(walk);

    git_oid oid;
    int count = 0;
    while (git_revwalk_next(&oid, walk) == 0) {
        count++;
    }

    if (count > 0) {
        BOAST("Woaah, you have %d commit(s)", count);
    }

    git_revwalk_free(walk);
    return count;
}

int main(int argc, char *argv[]) {
    if (corel_cli_parse_args(argc, argv, &args) != 0) {
        printf("Could not parse args\n");
        return 1;
    }

    git_libgit2_init();
    BOAST("Corel v0.0.1");
    git_repository *repository;
    git_repository_open(&repository, args.repo_path);

    if (!repository) {
        BOAST_ERR("Provided path is not a git repository");
        return 10;
    }

    if (corel_count_commits(repository) == 0) {
        BOAST_ERR("You have not made any commits yet. Why even run this?")
        goto cleanup;
    }

    BOAST("Grabbing tags...");
    git_strarray tag_names = {};
    git_tag_list(&tag_names, repository);

    BOAST("Tags: %lu", tag_names.count);
    size_t i = 0;
    for (; i < tag_names.count; i++) {
        BOAST("Tag: %s", tag_names.strings[i]);
    }

    git_strarray_free(&tag_names);

cleanup:
    git_repository_free(repository);
    git_libgit2_shutdown();
    BOAST("Bye o/");
    return 0;
}

#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARG_DRY_RUN_SHORT 0x80
#define ARG_PRINT_VERSION_SHORT 0x81
#define ARG_PATH_SHORT 0x82

typedef struct {
    bool quiet;
    bool print_version;
    bool dry_run;
    char *repo_path;
} cli_args;

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

int corel_cli_parse_args(int argc, char *argv[], cli_args* args) {
    struct argp argp = {options, parse_opt, 0, 0, 0, 0, 0};

    args->print_version = false;
    args->dry_run = false;
    args->quiet = false;
    args->repo_path = ".heehee";

    error_t err = argp_parse(&argp, argc, argv, 0, 0, args);

    return err;
}

int main(int argc, char *argv[]) {
    cli_args args = {};
    if (corel_cli_parse_args(argc, argv, &args) != 0) {
        printf("Could not parse args\n");
        return 1;
    }

    printf("HELLO\n");
    return 0;
}

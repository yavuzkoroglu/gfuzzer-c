#include <inttypes.h>
#include <string.h>
#include "ast.h"
#include "padkit/memalloc.h"
#include "padkit/verbose.h"

#define MAX_N       (4194304)
#define MAX_TIMEOUT (604800)

static void showCopyright(void) {
    fputs(
        "\n"
        "Copyright (C) 2025 Yavuz Koroglu\n"
        "\n",
        stderr
    );
}

static void showErrorBadNumber(char const* const arg1, char const* const arg2) {
    fprintf(
        stderr,
        "\n"
        "[ERROR] - Expected number @ '%.32s %.32s'\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        arg1,
        arg2
    );
}

static void showErrorBNFFilenameMissing(void) {
    fputs(
        "\n"
        "[ERROR] - BNF filename missing (-b or --bnf without a name)\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        stderr
    );
}

static void showErrorBNFFilenameTooLong(void) {
    fprintf(
        stderr,
        "\n"
        "[ERROR] - BNF filename cannot be longer than %d characters\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        FILENAME_MAX
    );
}

static void showErrorCannotOpenBNF(char const* const bnf_filename) {
    fprintf(
        stderr,
        "\n"
        "[ERROR] - Cannot open '%.*s'\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        FILENAME_MAX,
        bnf_filename
    );
}

static void showErrorNoBNFFilenameGiven(void) {
    fputs(
        "\n"
        "[ERROR] - Must specify a BNF file with -b or --bnf\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        stderr
    );
}

static void showErrorNumberMissing(void) {
    fputs(
        "\n"
        "[ERROR] - Must specify a NUMBER file with -n or --number\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        stderr
    );
}

static void showErrorNumberTooLarge(uint32_t const n) {
    fprintf(
        stderr,
        "\n"
        "[ERROR] - NUMBER must NOT exceed %d (n = %"PRIu32")\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        MAX_N, n
    );
}

static void showErrorTimeoutMissing(void) {
    fputs(
        "\n"
        "[ERROR] - Must specify a TIMEOUT file with -t or --timeout\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        stderr
    );
}

static void showErrorTimeoutZero(void) {
    fputs(
        "\n"
        "[ERROR] - TIMEOUT cannot be zero\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        stderr
    );
}

static void showErrorTimeoutTooLarge(uint32_t const t) {
    fprintf(
        stderr,
        "\n"
        "[ERROR] - TIMEOUT must NOT exceed %d (t = %"PRIu32")\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        MAX_TIMEOUT, t
    );
}

static void showUsage(void) {
    fputs(
        "\n"
        "gfuzzer-c: Grammar Fuzzer in C\n"
        "\n"
        "Usage: gfuzzer -b <bnf-file> [options]\n"
        "\n"
        "GENERAL OPTIONS:\n"
        "  -b,--bnf BNF-FILE            (Mandatory) An input grammar in Backus-Naur Form\n"
        "  -C,--copyright               Output the copyright message and exit\n"
        "  -h,--help                    Output this help message and exit\n"
        "  -n,--number NUMBER           The number of sentences (Default: 100)\n"
        "  -r,--root                    The root rule (Default: The first rule in the BNF-FILE)\n"
        "  -s,--same                    Allow the same sentence twice (Default: Do NOT allow / UNIQUE = true)\n"
        "  -t,--timeout TIMEOUT         Terminate generating sentences after some seconds (Default: 60)\n"
        "  -v,--verbose                 Timestamped status information (including token coverage) to stdout\n"
        "  -V,--version                 Output version number and exit\n"
        "\n"
        "EXAMPLE USES:\n"
        "\n",
        stderr
    );
}

static void showVersion(void) {
    fputs(
        "\n"
        "gfuzzer-c v1.0\n"
        "\n",
        stderr
    );
}

#ifdef LITEQ
    #undef LITEQ
#endif
#define LITEQ(str, lit) (memcmp(str, lit, sizeof(lit)) == 0)
int main(
    int argc,
    char* argv[]
) {
    FILE* bnf_file                  = NULL;
    char const* bnf_filename        = NULL;
    char const* root_str            = NULL;
    uint32_t const root_len         = 0;
    bool* const is_arg_processed    = mem_calloc((size_t)argc, sizeof(bool));
    bool unique                     = 1;
    uint32_t n                      = 100;
    uint32_t t                      = 60;

    if (argc <= 1) {
        showUsage();
        free(is_arg_processed);
        return EXIT_SUCCESS;
    }

    for (int i = argc - 1; i > 0; i--) {
        if (LITEQ(argv[i], "-V") || LITEQ(argv[i], "--version")) {
            showVersion();
            free(is_arg_processed);
            return EXIT_SUCCESS;
        }
    }

    for (int i = argc - 1; i > 0; i--) {
        if (LITEQ(argv[i], "-C") || LITEQ(argv[i], "--copyright")) {
            showCopyright();
            free(is_arg_processed);
            return EXIT_SUCCESS;
        }
    }

    for (int i = argc - 1; i > 0; i--) {
        if (LITEQ(argv[i], "-h") || LITEQ(argv[i], "--help")) {
            showUsage();
            free(is_arg_processed);
            return EXIT_SUCCESS;
        }
    }

    for (int i = argc - 1; i > 0; i--) {
        if (LITEQ(argv[i], "-v") || LITEQ(argv[i], "--verbose")) {
            verbose = 1;
            printf_verbose("Verbose enabled.");
            #ifndef NDEBUG
                printf_verbose("MODE = debug");
            #else
                printf_verbose("MODE = release");
            #endif
            is_arg_processed[i] = 1;
            break;
        }
    }

    for (int i = argc - 1; i > 0; i--) {
        if (is_arg_processed[i]) continue;

        if (LITEQ(argv[i], "-b") || LITEQ(argv[i], "--bnf")) {
            if (i == argc - 1) {
                showErrorBNFFilenameMissing();
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            bnf_filename = argv[i + 1];
            if (bnf_filename[0] == '-') {
                showErrorBNFFilenameMissing();
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            if (strlen(bnf_filename) > FILENAME_MAX) {
                showErrorBNFFilenameTooLong();
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            printf_verbose("BNF File = %.*s", FILENAME_MAX, bnf_filename);
            is_arg_processed[i]     = 1;
            is_arg_processed[i + 1] = 1;
            break;
        }
    }
    if (bnf_filename == NULL) {
        showErrorNoBNFFilenameGiven();
        free(is_arg_processed);
        return EXIT_FAILURE;
    }

    for (int i = argc - 1; i > 0; i--) {
        if (is_arg_processed[i]) continue;

        if (LITEQ(argv[i], "-r") || LITEQ(argv[i], "--root")) {
            if (i == argc - 1) {
                showErrorRootMissing();
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            bnf_filename = argv[i + 1];
            if (bnf_filename[0] == '-') {
                showErrorBNFFilenameMissing();
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            if (strlen(bnf_filename) > FILENAME_MAX) {
                showErrorBNFFilenameTooLong();
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            printf_verbose("BNF File = %.*s", FILENAME_MAX, bnf_filename);
            is_arg_processed[i]     = 1;
            is_arg_processed[i + 1] = 1;
            break;
        }
    }

    for (int i = argc - 1; i > 0; i--) {
        if (is_arg_processed[i]) continue;

        if (LITEQ(argv[i], "-s") || LITEQ(argv[i], "--same")) {
            unique = 0;
            break;
        }
    }
    if (unique)
        printf_verbose("UNIQUE = true");
    else
        printf_verbose("UNIQUE = false");

    for (int i = argc - 1; i > 0; i--) {
        if (is_arg_processed[i]) continue;

        if (LITEQ(argv[i], "-n") || LITEQ(argv[i], "--number")) {
            if (i == argc - 1) {
                showErrorNumberMissing();
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            if (sscanf(argv[i + 1], "%"SCNu32, &n) != 1) {
                showErrorBadNumber(argv[i], argv[i + 1]);
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            if (n == 0) {
                free(is_arg_processed);
                return EXIT_SUCCESS;
            }
            if (n > MAX_N) {
                showErrorNumberTooLarge(n);
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            is_arg_processed[i]     = 1;
            is_arg_processed[i + 1] = 1;
            break;
        }
    }
    printf_verbose("n = %"PRIu32" sentences", n);

    for (int i = argc - 1; i > 0; i--) {
        if (is_arg_processed[i]) continue;

        if (LITEQ(argv[i], "-t") || LITEQ(argv[i], "--timeout")) {
            if (i == argc - 1) {
                showErrorTimeoutMissing();
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            if (sscanf(argv[i + 1], "%"PRIu32, &t) != 1) {
                showErrorBadNumber(argv[i], argv[i + 1]);
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            if (t == 0) {
                showErrorTimeoutZero();
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            if (t > MAX_TIMEOUT) {
                showErrorTimeoutTooLarge(t);
                free(is_arg_processed);
                return EXIT_FAILURE;
            }
            is_arg_processed[i]     = 1;
            is_arg_processed[i + 1] = 1;
            break;
        }
    }
    printf_verbose("t = %"PRIu32" seconds", t);

    bnf_file = fopen(bnf_filename, "r");
    if (bnf_file == NULL) {
        showErrorCannotOpenBNF(bnf_filename);
        free(is_arg_processed);
        return EXIT_FAILURE;
    }
    switch (constructFromBNF_ast(ast, bnf_file, root_str, root_len)) {
        case AST_OK:
            break;
        case AST_SYNTAX_ERROR:
        default:
            showErrorSyntax();
            fclose(bnf_file);
            free(is_arg_processed);
            return EXIT_FAILURE;
    }
    fclose(bnf_file);

    generateAndPrintNSentencesWithinTSeconds(ast, n, t, unique);

    printf_verbose("# Tokens (Not-Covered) = %"PRIu32, ast->n_tokens_not_covered);
    printf_verbose("# Tokens (Covered-Once) = %"PRIu32, ast->n_tokens_covered_once);
    printf_verbose("# Tokens (Total) = %"PRIu32, ast->n_tokens_total);
    printf_verbose("Token Coverage = %"PRIu32"%%", getTokenCov_ast(ast));
    printf_verbose("Finished.");

    free(is_arg_processed);
    return EXIT_SUCCESS;
}
#undef LITEQ

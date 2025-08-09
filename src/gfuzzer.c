#include <string.h>
#include "padkit/memalloc.h"
#include "padkit/verbose.h"

static void showCopyright(void) {
    fputs(
        "\n"
        "Copyright (C) 2025 Yavuz Koroglu\n"
        "\n",
        stderr
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
        "  -n,--number                  The number of sentences (Default: 100)\n"
        "  -r,--root                    The root rule (Default: The first rule in the BNF-FILE)\n"
        "  -s,--same                    Allow the same sentence twice (Default: Do NOT allow / UNIQUE = true)\n"
        "  -v,--verbose                 Timestamped status information to stdout\n"
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
    char const* bnf_filename        = NULL;
    bool* const is_arg_processed    = mem_calloc((size_t)argc, sizeof(bool));
    bool unique                     = 1;
    uint32_t n                      = 100;

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
            break;
        }
    }

    printf_verbose("Finished.");

    generateAndPrintNSentences(bnf_filename, n)

    free(is_arg_processed);
    return EXIT_SUCCESS;
}
#undef LITEQ

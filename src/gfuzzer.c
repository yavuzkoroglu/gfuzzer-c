#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include "bnf.h"
#include "decisiontree.h"
#include "padkit/memalloc.h"
#include "padkit/verbose.h"

#define MAX_N               (4194304)
#define MAX_TIMEOUT         (604800)

#define DEFAULT_COV_GUIDED  (0)
#define DEFAULT_MIN_DEPTH   (0)
#define DEFAULT_UNIQUE      (1)
#define DEFAULT_SEED        (131077)
#define DEFAULT_N           (100)
#define DEFAULT_TIMEOUT     (60)

static void showCopyright(void) {
    fputs(
        "\n"
        "Copyright (C) 2025 Yavuz Koroglu\n"
        "\n",
        stderr
    );
}

static void showErrorBadNumber(
    char const* const arg1,
    char const* const arg2
) {
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

static void showErrorParameterTooLong(
    char const* const parameter_name,
    char const* const abbreviations,
    size_t const max_len,
    size_t const len
) {
    fprintf(
        stderr,
        "\n"
        "[ERROR] - %.1024s after %.1024s cannot be longer than %zu (given: %zu)\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        parameter_name,
        abbreviations,
        max_len,
        len
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
        "[ERROR] - Must specify BNF file (-b or --bnf-file)\n"
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

static void showErrorParameterMissing(
    char const* const parameter_name,
    char const* const abbreviations
) {
    fprintf(
        stderr,
        "\n"
        "[ERROR] - %.1024s missing after %.1024s\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        parameter_name, abbreviations
    );
}

static void showErrorRootStrBadFormat(char const* const root_str) {
    fprintf(
        stderr,
        "\n"
        "[ERROR] - Syntax Error @ \""BNF_STR_RULE_OPEN"RULE"BNF_STR_RULE_CLOSE"\" (%.*s)\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        BNF_MAX_LEN_TERM, root_str
    );
}

static void showErrorSyntax(void) {
    fputs(
        "\n"
        "[ERROR] - Bad Syntax @ BNF\n"
        "\n",
        stderr
    );
}

static void showErrorTimeoutZero(void) {
    fputs(
        "\n"
        "[ERROR] - Timeout NUMBER cannot be zero\n"
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
        "[ERROR] - Timeout NUMBER must NOT exceed %d (t = %"PRIu32")\n"
        "\n"
        "gfuzzer --help for more instructions\n"
        "\n",
        MAX_TIMEOUT, t
    );
}

static void showUsage(char const* const path) {
    fprintf(
        stderr,
        "\n"
        "gfuzzer-c: Grammar Fuzzer in C\n"
        "\n"
        "Usage: gfuzzer -b <bnf-file> [options]\n"
        "\n"
        "GENERAL OPTIONS:\n"
        "  -b,--bnf BNF-FILE            (Mandatory) An input grammar in Backus-Naur Form\n"
        "  -c,--cov-guided              Enable coverage guidance optimization (Default: Disabled)\n"
        "  -C,--copyright               Output the copyright message and exit\n"
        "  -h,--help                    Output this help message and exit\n"
        "  -m,--min-depth NUMBER        The minimum depth, increase it to get longer sentences (Default: %d)\n"
        "  -n,--number NUMBER           The number of sentences (Default: %d)\n"
        "  -r,--root \""
                      BNF_STR_RULE_OPEN
                      "RULE"
                          BNF_STR_RULE_CLOSE
                          "\"           The root rule (Default: The first rule in the BNF-FILE)\n"
        "  -s,--seed NUMBER             Change the default random seed (Default: %d)\n"
        "  -S,--same                    Allow the same sentence twice (Default: Do NOT allow / UNIQUE = true)\n"
        "  -t,--timeout NUMBER          Terminate generating sentences after some seconds (Default: %d)\n"
        "  -v,--verbose                 Timestamped status information (including term coverage) to stderr\n"
        "  -V,--version                 Output version number and exit\n"
        "\n"
        BNF_STR_RULE_OPEN"RULE"BNF_STR_RULE_CLOSE" FORMAT:\n"
        "  * Every rule name must begin with '"BNF_STR_RULE_OPEN"'.\n"
        "  * Every rule name must end with '"BNF_STR_RULE_CLOSE"'.\n"
        "  * Rule names cannot contain whitespace.\n"
        "\n"
        "EXAMPLE USES:\n"
        "  %.*s -b bnf/numbers.bnf -c -n 10 -r \""BNF_STR_RULE_OPEN"number"BNF_STR_RULE_CLOSE"\" -t 10\n"
        "\n",
        DEFAULT_MIN_DEPTH, DEFAULT_N, DEFAULT_SEED, DEFAULT_TIMEOUT, FILENAME_MAX, path
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
#define LITEQ(str, lit)     (strncmp(str, lit, sizeof(lit)) == 0)

#ifdef PROCESS_ARG
    #undef PROCESS_ARG
#endif
#define PROCESS_ARG(abbr,name)                                                      \
    for (int i = argc - 1; i > 0; i--)                                              \
        if (!is_arg_processed[i] && (LITEQ(argv[i], abbr) || LITEQ(argv[i], name)))

int main(
    int argc,
    char* argv[]
) {
    GrammarGraph graph[1]           = { NOT_A_GGRAPH };
    FILE* bnf_file                  = NULL;
    char const* bnf_filename        = NULL;
    size_t bnf_filename_len         = 0;
    char* root_str                  = NULL;
    size_t root_len                 = 0;
    bool* const is_arg_processed    = mem_calloc((size_t)argc, sizeof(bool));
    bool is_cov_guided              = DEFAULT_COV_GUIDED;
    bool unique                     = DEFAULT_UNIQUE;
    uint32_t min_depth              = DEFAULT_MIN_DEPTH;
    uint32_t n                      = DEFAULT_N;
    uint32_t seed                   = DEFAULT_SEED;
    uint32_t t                      = DEFAULT_TIMEOUT;

    if (argc <= 1) {
        showUsage(argv[0]);
        free(is_arg_processed);
        return EXIT_SUCCESS;
    }

    PROCESS_ARG("-C", "--copyright") {
        showCopyright();
        free(is_arg_processed);
        return EXIT_SUCCESS;
    }

    PROCESS_ARG("-h", "--help") {
        showUsage(argv[0]);
        free(is_arg_processed);
        return EXIT_SUCCESS;
    }

    PROCESS_ARG("-V", "--version") {
        showVersion();
        free(is_arg_processed);
        return EXIT_SUCCESS;
    }

    PROCESS_ARG("-v", "--verbose") {
        verbose = 1;
        fprintf_verbose(stderr, "Verbose enabled.");
        #ifndef NDEBUG
            fprintf_verbose(stderr, "MODE = debug");
        #else
            fprintf_verbose(stderr, "MODE = release");
        #endif
        is_arg_processed[i] = 1;
        break;
    }

    PROCESS_ARG("-b", "--bnf") {
        if (i == argc - 1 || (bnf_filename = argv[i + 1])[0] == '-') {
            showErrorParameterMissing("BNF-FILE", "-b or --bnf");
            free(is_arg_processed);
            return EXIT_FAILURE;
        }
        bnf_filename_len = strlen(bnf_filename);
        if (bnf_filename_len > FILENAME_MAX) {
            showErrorParameterTooLong("BNF-FILE", "-b or --bnf", FILENAME_MAX, bnf_filename_len);
            free(is_arg_processed);
            return EXIT_FAILURE;
        }
        fprintf_verbose(stderr, "BNF File = %.*s", FILENAME_MAX, bnf_filename);
        is_arg_processed[i]     = 1;
        is_arg_processed[i + 1] = 1;
        break;
    }
    if (bnf_filename == NULL) {
        showErrorNoBNFFilenameGiven();
        free(is_arg_processed);
        return EXIT_FAILURE;
    }

    PROCESS_ARG("-c", "--cov-guided") {
        is_cov_guided       = 1;
        is_arg_processed[i] = 1;
        break;
    }
    if (is_cov_guided)
        fprintf_verbose(stderr, "COV_GUIDANCE = Enabled");
    else
        fprintf_verbose(stderr, "COV_GUIDANCE = Disabled");

    PROCESS_ARG("-m", "--min-depth") {
        if (i == argc - 1) {
            showErrorParameterMissing("NUMBER", "-m or --min-depth");
            free(is_arg_processed);
            return EXIT_FAILURE;
        }
        if (sscanf(argv[i + 1], "%"SCNu32, &min_depth) != 1) {
            showErrorBadNumber(argv[i], argv[i + 1]);
            free(is_arg_processed);
            return EXIT_FAILURE;
        }
        is_arg_processed[i]     = 1;
        is_arg_processed[i + 1] = 1;
        break;
    }
    fprintf_verbose(stderr, "MIN_DEPTH = %"PRIu32" expansions", min_depth);

    PROCESS_ARG("-n", "--number") {
        if (i == argc - 1) {
            showErrorParameterMissing("NUMBER", "-n or --number");
            free(is_arg_processed);
            return EXIT_FAILURE;
        }
        if (sscanf(argv[i + 1], "%"SCNu32, &n) != 1) {
            showErrorBadNumber(argv[i], argv[i + 1]);
            free(is_arg_processed);
            return EXIT_FAILURE;
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
    fprintf_verbose(stderr, "n = %"PRIu32" sentences", n);

    PROCESS_ARG("-r", "--root") {
        root_str = argv[i + 1];
        if (root_str[0] == '-') {
            showErrorParameterMissing(BNF_STR_RULE_OPEN"RULE"BNF_STR_RULE_CLOSE, "-r or --root");
            free(is_arg_processed);
            return EXIT_FAILURE;
        }
        root_len = strlen(root_str);
        if (root_len > BNF_MAX_LEN_TERM) {
            showErrorParameterTooLong(
                BNF_STR_RULE_OPEN"RULE"BNF_STR_RULE_CLOSE,
                "-r or --root",
                BNF_MAX_LEN_TERM,
                root_len
            );
            free(is_arg_processed);
            return EXIT_FAILURE;
        }
        if (!isRuleNameWellFormed_bnf(root_str, root_len)) {
            showErrorRootStrBadFormat(root_str);
            free(is_arg_processed);
            return EXIT_FAILURE;
        }
        is_arg_processed[i]     = 1;
        is_arg_processed[i + 1] = 1;
        break;
    }

    PROCESS_ARG("-s", "--seed") {
        if (i == argc - 1) {
            showErrorParameterMissing("NUMBER", "-s or --seed");
            free(is_arg_processed);
            return EXIT_FAILURE;
        }
        if (sscanf(argv[i + 1], "%"SCNu32, &seed) != 1) {
            showErrorBadNumber(argv[i], argv[i + 1]);
            free(is_arg_processed);
            return EXIT_FAILURE;
        }
        is_arg_processed[i]     = 1;
        is_arg_processed[i + 1] = 1;
        break;
    }
    srand(seed);
    fprintf_verbose(stderr, "seed = %"PRIu32, seed);

    PROCESS_ARG("-S", "--same") {
        unique              = 0;
        is_arg_processed[i] = 1;
        break;
    }
    if (unique)
        fprintf_verbose(stderr, "UNIQUE = true");
    else
        fprintf_verbose(stderr, "UNIQUE = false");

    PROCESS_ARG("-t", "--timeout") {
        if (i == argc - 1) {
            showErrorParameterMissing("NUMBER", "-t or --timeout");
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
    fprintf_verbose(stderr, "t = %"PRIu32" seconds", t);

    fprintf_verbose(stderr, "IGNORED ARGS =");
    for (int i = 1; i < argc - 1; i++) {
        if (is_arg_processed[i]) continue;

        fprintf_verbose(stderr, " %s", argv[i]);
    }
    fprintf_verbose(stderr, "\n");

    bnf_file = fopen(bnf_filename, "r");
    if (bnf_file == NULL) {
        showErrorCannotOpenBNF(bnf_filename);
        free(is_arg_processed);
        return EXIT_FAILURE;
    }
    switch (construct_ggraph(graph, bnf_file, root_str, (uint32_t)root_len)) {
        case GRAMMAR_OK:
            break;
        case GRAMMAR_SYNTAX_ERROR:
        default:
            showErrorSyntax();
            fclose(bnf_file);
            free(is_arg_processed);
            return EXIT_FAILURE;
    }
    fclose(bnf_file);

    fprintf_verbose(stderr, "# Terms (Covered-Once) = %"PRIu32, graph->n_terms_covered_once);
    fprintf_verbose(stderr, "# Terms (Total) = %"PRIu32, nTerms_ggraph(graph));
    fprintf_verbose(stderr, "Term Coverage = %"PRIu32"%%", termCov_ggraph(graph));
    fprintf_verbose(stderr, "Finished.");

    free(is_arg_processed);
    return EXIT_SUCCESS;
}
#undef PROCESS_ARG
#undef LITEQ

#ifndef BNF_H
    #define BNF_H
    #include "padkit/chunk.h"

    #ifndef BNF_MAX_LEN_TERM
        #define BNF_MAX_LEN_TERM (1024)
    #endif

    #ifndef BNF_MAX_SZ_FILE
        #define BNF_MAX_SZ_FILE (1048576)
    #endif

    #ifndef BNF_STR_TERMINAL_OPEN
        #define BNF_STR_TERMINAL_OPEN "'"
    #endif

    #ifndef BNF_STR_TERMINAL_CLOSE
        #define BNF_STR_TERMINAL_CLOSE "'"
    #endif

    #ifndef BNF_STR_REGEX_OPEN
        #define BNF_STR_REGEX_OPEN "\""
    #endif

    #ifndef BNF_STR_REGEX_CLOSE
        #define BNF_STR_REGEX_CLOSE "\""
    #endif

    #ifndef BNF_STR_RULE_OPEN
        #define BNF_STR_RULE_OPEN "<"
    #endif

    #ifndef BNF_STR_RULE_CLOSE
        #define BNF_STR_RULE_CLOSE ">"
    #endif

    #ifndef BNF_STR_EQUIV
        #define BNF_STR_EQUIV "::="
    #endif

    #ifndef BNF_STR_LINE_COMMENT
        #define BNF_STR_LINE_COMMENT ";"
    #endif

    #ifndef BNF_STR_ALTERNATIVE
        #define BNF_STR_ALTERNATIVE "|"
    #endif

    bool isRuleNameWellFormed_bnf(char const* const name, size_t const len);

    bool isTerminalWellFormed_bnf(char const* const term, size_t const len);
#endif

#include <ctype.h>
#include <string.h>
#include "bnf.h"

bool isRuleNameWellFormed_bnf(char const* const name, size_t const len) {
    size_t const sz_open    = sizeof(BNF_STR_RULE_OPEN) - 1;
    size_t const sz_close   = sizeof(BNF_STR_RULE_CLOSE) - 1;
    size_t const min_len    = sz_open + sz_close + 1;

    if (name == NULL)                                                       return 0;
    if (len < min_len)                                                      return 0;
    if (strncmp(name, BNF_STR_RULE_OPEN, sz_open) != 0)                     return 0;
    if (strncmp(name + len - sz_close, BNF_STR_RULE_CLOSE, sz_close) != 0)  return 0;

    for (char const* p = name + sz_open; p < name + len - sz_close; p++)
        if (isspace((unsigned char)*p))
            return 0;

    return 1;
}

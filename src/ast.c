#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "ast.h"
#include "padkit/hash.h"
#include "padkit/implication.h"
#include "padkit/invalid.h"

#ifdef LITEQ
    #undef LITEQ
#endif
#define LITEQ(str, lit) (strncmp(str, lit, sizeof(lit) - 1) == 0)

int constructFromBNF_ast(
    ASTree* const ast,
    FILE* const bnf_file,
    char const* const root_str,
    uint32_t const root_len
) {
    ASTreeNode* parent;
    ASTreeNode* child;
    IndexMapping* mapping;
    bool ins_result;
    char token_str[AST_MAX_LEN_TOKEN];

    assert(ast != NULL);
    assert(bnf_file != NULL);
    assert(IMPLIES(root_str != NULL,
        0 < root_len && root_len < AST_MAX_LEN_TOKEN &&
        strlen(root_str) < AST_MAX_LEN_TOKEN
    ));

    constructEmpty_chunk(ast->chunk, CHUNK_RECOMMENDED_PARAMETERS);
    constructEmpty_itbl(ast->chunktbl, ITBL_RECOMMENDED_PARAMETERS);

    addF_chunk(ast->chunk, bnf_file, BNF_MAX_SZ_FILE, BUFSIZ);
    cutByDelimLast_chunk(ast->chunk, "\n");

    for (uint32_t line_id = LEN_CHUNK(ast->chunk) - 1; line_id > 0; i--) {
        Item const line             = get_chunk(ast->chunk, line_id);
        char const* const line_str  = (char*)line.p;
        uint32_t i                  = 0;
        uint32_t j                  = 0;
        uint32_t token_len          = 0;
        uint_fast64_t token_index   = 0;

        /* Skip spaces */
        while (i < line.sz && isspace(line_str[i])) i++;

        /* Ignore comments */
        if (i == line.sz || LITEQ(line_str + i, BNF_STR_LINE_COMMENT)) continue;

        /* Rule token must start with BNF_STR_RULE_OPEN */
        if (!LITEQ(line_str + i, BNF_STR_RULE_OPEN)) return AST_SYNTAX_ERROR;

        token_len   = 2;
        j           = i + 1;
        if (j == line.sz || isspace(line_str[j])) return AST_SYNTAX_ERROR;
        while (!LITEQ(line_str + j, BNF_STR_RULE_CLOSE)) {
            j++;
            token_len++;
            if (
                j == line.sz            ||
                isspace(line_str[j])    ||
                token_len >= AST_MAX_LEN_TOKEN
            ) return AST_SYNTAX_ERROR;
        }
        if (token_len <= 2) return AST_SYNTAX_ERROR;
        memcpy(token_str, line_str + i, token_len);
        token_index = hash64_str(token_str, token_len);
        ins_result  = insert_itbl(
            mapping,
            ast->chunktbl,
            token_index,
            LEN_CHUNK(chunk),
            ITBL_BEHAVIOR_RESPECT,
            ITBL_RELATION_ONE_TO_ONE
        );
        if (ins_result == ITBL_INSERT_UNIQUE) {
            parent              = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeNode)).p;
            parent->str_id      = LEN_CHUNK(chunk);
            parnet->cov_count   = 0;
            constructEmpty_alist(parent->children, ALIST_RECOMMENDED_PARAMETERS);
            add_chunk(ast->chunk, token_str, token_len);
        } else {
            parent = get_chunk(ast->chunk, mapping->value).p;
        }

        /* Skip the rule */
        i = j + 1;

        /* Skip spaces */
        while (i < line.sz && isspace(line_str[i])) i++;

        if (i == line.sz) return AST_SYNTAX_ERROR;

        do {
            /* Must have the BNF_STR_EQUIV operator */
            if (!LITEQ(line_str + i, BNF_STR_EQUIV)) return AST_SYNTAX_ERROR;

            /* Skip that operator */
            i += sizeof(BNF_STR_EQUIV) - 1;

            /* Skip spaces */
            while (i < line.sz && isspace(line_str[i])) i++;

            /* Cannot have empty expansion */
            if (i == line.sz) return AST_SYNTAX_ERROR;

            /* Add new child expansion */
            child = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeNode));

            if (LITEQ(line_str + i, BNF_STR_TERMINAL_OPEN)) {
            } else if (LITEQ(line_str + i, BNF_STR_RULE_OPEN)) {
            } else if (LITEQ(line_str + i, BNF_STR_ALTERNATIVE)) {
            }

            /* Skip spaces */
            while (i < line.sz && isspace(line_str[i])) i++;
        } while (i < line.sz);
    }

    return AST_OK;
}

#undef LITEQ

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "bnf.h"
#include "padkit/hash.h"
#include "padkit/implication.h"
#include "padkit/invalid.h"
#include "padkit/size.h"
#include "padkit/verbose.h"

#ifdef LITEQ
    #undef LITEQ
#endif
#define LITEQ(str, lit) (memcmp(str, lit, sizeof(lit) - 1) == 0)

int constructFromBNF_ast(
    ASTree* const ast,
    FILE* const bnf_file,
    char const* const root_str,
    uint32_t const root_len
) {
    IndexMapping* mapping;
    ASTreeNode* parent;
    ASTreeNode* child;
    ASTreeExpansion* expansion;
    bool ins_result;
    char term_str[AST_MAX_LEN_TERM];

    assert(ast != NULL);
    assert(bnf_file != NULL);
    assert(IMPLIES(root_str != NULL,
        0 < root_len && root_len < AST_MAX_LEN_TERM &&
        strlen(root_str) < AST_MAX_LEN_TERM
    ));

    constructEmpty_chunk(ast->chunk, CHUNK_RECOMMENDED_PARAMETERS);
    constructEmpty_itbl(ast->chunktbl, ITBL_RECOMMENDED_PARAMETERS);
    ast->root_id                = INVALID_UINT32;
    ast->n_terms_not_covered    = 0;
    ast->n_terms_covered_once   = 0;
    ast->n_terms_total          = 0;

    addF_chunk(ast->chunk, bnf_file, BNF_MAX_SZ_FILE, BUFSIZ);
    fprintf_verbose(stderr, "AST: # BNF size = %"PRIu32" bytes", AREA_CHUNK(ast->chunk));

    cutByDelimLast_chunk(ast->chunk, "\n");
    fprintf_verbose(stderr, "AST: # Lines in BNF = %"PRIu32, LEN_CHUNK(ast->chunk));

    for (uint32_t line_id = LEN_CHUNK(ast->chunk); line_id-- > 0;) {
        Item const line             = get_chunk(ast->chunk, line_id);
        char const* const line_str  = (char*)line.p;
        uint32_t i                  = 0;
        uint32_t j                  = 0;
        uint32_t term_len           = 0;
        uint_fast64_t term_index    = 0;

        /* fprintf_verbose(stderr, "AST: Line = %"PRIu32": \"%.*s\"", line_id + 1, (int)line.sz, line_str); */

        /* Skip spaces */
        while (i < line.sz && isspace(line_str[i])) i++;

        /* Ignore comments and empty lines */
        if (i == line.sz || line_str[i] == '\0' || LITEQ(line_str + i, BNF_STR_LINE_COMMENT)) continue;

        /* Rule term must start with BNF_STR_RULE_OPEN */
        if (!LITEQ(line_str + i, BNF_STR_RULE_OPEN)) return AST_SYNTAX_ERROR;

        term_len    = sizeof(BNF_STR_RULE_OPEN);
        j           = i + term_len - 1;
        if (j == line.sz || isspace(line_str[j])) return AST_SYNTAX_ERROR;
        while (!LITEQ(line_str + j, BNF_STR_RULE_CLOSE)) {
            j++;
            term_len++;
            if (
                j == line.sz            ||
                isspace(line_str[j])    ||
                term_len >= AST_MAX_LEN_TERM
            ) return AST_SYNTAX_ERROR;
        }
        if (term_len <= 2) return AST_SYNTAX_ERROR;
        memcpy(term_str, line_str + i, term_len);
        /* fprintf_verbose(stderr, "AST: Rule = %.*s", term_len, term_str); */

        term_index  = hash64_str(term_str, term_len);
        mapping     = insert_itbl(
            &ins_result,
            ast->chunktbl,
            term_index,
            LEN_CHUNK(ast->chunk) + 1,
            ITBL_BEHAVIOR_RESPECT,
            ITBL_RELATION_ONE_TO_ONE
        );
        if (ins_result == ITBL_INSERT_UNIQUE) {
            add_chunk(ast->chunk, term_str, term_len);
            ast->n_terms_total++;
            ast->n_terms_not_covered++;

            parent                  = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeNode)).p;
            parent->cov_count       = 0;
            parent->str_id          = LEN_CHUNK(ast->chunk) - 2;
            parent->n_expansions    = 0;
        } else {
            Item dup_rule_item      = get_chunk(ast->chunk, mapping->value);
            assert(dup_item.sz == sizeof(ASTreeNode));

            parent = parent_item.p;
            assert(isValid_astn(ast, parent));
        }
        if (root_str == NULL || (term_len == root_len && memcmp(term_str, root_str, term_len) == 0))
            ast->root_id = mapping->value;

        /* Skip BNF_STR_RULE_CLOSE */
        i = j + sizeof(BNF_STR_RULE_CLOSE) - 1;

        /* Skip spaces */
        while (i < line.sz && isspace(line_str[i])) i++;

        /* Must have something after the rule declaration */
        if (i == line.sz || line_str[i] == '\0') return AST_SYNTAX_ERROR;

        /* Must have the BNF_STR_EQUIV operator */
        if (!LITEQ(line_str + i, BNF_STR_EQUIV)) return AST_SYNTAX_ERROR;

        /* Skip that operator */
        i += sizeof(BNF_STR_EQUIV) - 1;

        /* Skip spaces */
        while (i < line.sz && isspace(line_str[i])) i++;

        /* Cannot have empty expansion */
        if (i == line.sz || line_str[i] == '\0') return AST_SYNTAX_ERROR;

        /* Must have at least one expansion. */
        parent->expansion_ids[parent->n_expansions++] = LEN_CHUNK(ast->chunk);
        expansion = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeExpansion)).p;
        expansion->cov_count    = 0;
        expansion->next_id      = INVALID_UINT32;

        while (i < line.sz && line_str[i] != '\0') {
            if (LITEQ(line_str + i, BNF_STR_ALTERNATIVE)) {
                assert(parent->n_expansions < AST_MAX_N_EXPANSIONS);
                parent->expansion_ids[parent->n_expansions++] = LEN_CHUNK(ast->chunk);
                expansion = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeExpansion)).p;
                expansion->cov_count    = 0;
                expansion->next_id      = INVALID_UINT32;

                /* Skip BNF_STR_ALTERNATIVE */
                i += sizeof(BNF_STR_ALTERNATIVE) - 1;

                /* Skip spaces */
                while (i < line.sz && isspace(line_str[i])) i++;

                /* Cannot have empty expansion */
                if (i == line.sz || line_str[i] == '\0')        return AST_SYNTAX_ERROR;
            } else if (LITEQ(line_str + i, BNF_STR_RULE_OPEN)) {
                j = i + sizeof(BNF_STR_RULE_OPEN) - 1;
                term_len = sizeof(BNF_STR_RULE_OPEN) - 1;
                if (j >= line.sz)                               return AST_SYNTAX_ERROR;
                if (LITEQ(line_str + j, BNF_STR_RULE_CLOSE))    return AST_SYNTAX_ERROR;
                do {
                    if (isspace(line_str[j]))                   return AST_SYNTAX_ERROR;
                    if (++term_len > AST_MAX_LEN_TERM)          return AST_SYNTAX_ERROR;
                    if (++j >= line.sz)                         return AST_SYNTAX_ERROR;
                } while (!LITEQ(line_str + j, BNF_STR_RULE_CLOSE));
                term_len += sizeof(BNF_STR_RULE_CLOSE) - 1;
                if (term_len > AST_MAX_LEN_TERM)                return AST_SYNTAX_ERROR;
                memcpy(term_str, line_str + i, term_len);
                /* fprintf_verbose(stderr, "AST: Subrule = %.*s", (int)term_len, term_str); */

                term_index  = hash64_str(term_str, term_len);
                mapping     = insert_itbl(
                    &ins_result,
                    ast->chunktbl,
                    term_index,
                    LEN_CHUNK(ast->chunk) + 1,
                    ITBL_BEHAVIOR_RESPECT,
                    ITBL_RELATION_ONE_TO_ONE
                );
                if (ins_result == ITBL_INSERT_UNIQUE) {
                    add_chunk(ast->chunk, term_str, term_len);
                    ast->n_terms_total++;
                    ast->n_terms_not_covered++;

                    child               = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeNode)).p;
                    child->cov_count    = 0;
                    child->str_id       = LEN_CHUNK(ast->chunk) - 2;
                    child->n_expansions = 0;
                }
                expansion->str_id   = INVALID_UINT32;
                expansion->node_id  = mapping->value;

                i = j + 1;

                /* Skip spaces */
                while (i < line.sz && isspace(line_str[i])) i++;

                if (i < line.sz) {
                    expansion->next_id = LEN_CHUNK(ast->chunk);
                    expansion = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeExpansion)).p;
                    expansion->cov_count    = 0;
                    expansion->next_id      = INVALID_UINT32;
                }
            } else if (LITEQ(line_str + i, BNF_STR_TERMINAL_OPEN)) {
                i += sizeof(BNF_STR_TERMINAL_OPEN) - 1;
                j = i;
                term_len = 0;
                if (j >= line.sz)                                   return AST_SYNTAX_ERROR;
                if (LITEQ(line_str + j, BNF_STR_TERMINAL_CLOSE))    return AST_SYNTAX_ERROR;
                do {
                    if (++term_len > AST_MAX_LEN_TERM)              return AST_SYNTAX_ERROR;
                    if (++j >= line.sz)                             return AST_SYNTAX_ERROR;
                } while (!LITEQ(line_str + j, BNF_STR_TERMINAL_CLOSE));
                memcpy(term_str, line_str + i, term_len);
                /*
                fprintf_verbose(
                    stderr,
                    "AST: Terminal = "BNF_STR_TERMINAL_OPEN"%.*s"BNF_STR_TERMINAL_CLOSE,
                    (int)term_len,
                    term_str
                );
                */

                expansion->node_id  = INVALID_UINT32;
                expansion->str_id   = LEN_CHUNK(ast->chunk);
                add_chunk(ast->chunk, term_str, term_len);
                ast->n_terms_total++;
                ast->n_terms_not_covered++;
                i = j + 1;

                /* Skip spaces */
                while (i < line.sz && isspace(line_str[i])) i++;

                if (i < line.sz && line_str[i] != '\0') {
                    expansion->next_id = LEN_CHUNK(ast->chunk);
                    expansion = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeExpansion)).p;
                    expansion->cov_count    = 0;
                    expansion->next_id      = INVALID_UINT32;
                }
            } else if (isspace(line_str[i])) {
                /* Skip spaces */
                i++;
            } else {
                return AST_SYNTAX_ERROR;
            }
        }
    }

    fprintf_verbose(stderr, "AST: # Terms = %"PRIu32, ast->n_terms_total);
    return AST_OK;
}

#undef LITEQ

void destruct_ast(ASTree* const ast) {
    assert(isValid_ast(ast));
    destruct_chunk(ast->chunk);
    destruct_itbl(ast->chunktbl);
    *ast = NOT_AN_ASTREE;
}

Item expandRandom_ast(
    Chunk* const str_builder,
    IndexTable* const str_builder_tbl,
    ASTree* const ast,
    uint32_t const root_id,
    bool const is_cov_guided,
    bool const unique
) {
    assert(isValid_chunk(str_builder));
    assert(isValid_itbl(str_builder_tbl));
    assert(isValid_ast(ast));
    assert(root_id < LEN_CHUNK(ast->chunk));

    addIndeterminate_chunk(str_builder, 0);
    {
        ArrayList stack[1]  = { NOT_AN_ALIST };
        uint32_t stack_id   = 0;
        constructEmpty_alist(stack, sizeof(uint32_t), ast->n_terms_total);

        pushTop_alist(stack, &root_id);
        while (stack_id < stack->len) {
            uint32_t const parent_id    = *(uint32_t*)get_alist(stack, stack_id++);
            ASTreeNode* parent          = getNode_ast(ast, parent_id);
            if (parent->cov_count == 0) {
                assert(ast->n_terms_covered_once > 0);
                ast->n_terms_covered_once--;
                ast->n_terms_not_covered++;
            }
            if (parent->cov_count < SZ32_MAX) {
                parent->cov_count++;
            }

            if (parent->n_expansions == 0) {
                Item const parent_item = get_chunk(ast->chunk, parent_id);
                appendLast_chunk(str_builder, parent_item.p, parent_item.sz);
            } else {
                uint32_t child_id = 0;
                if (is_cov_guided) {
                    uint32_t i = 0;
                    while (i < parent->n_expansions) {
                        child_id = parent->expansion_ids[i];
                        {
                            Item const child_item               = get_chunk(ast->chunk, child_id);
                            ASTreeExpansion const* const child  = child_item.p;
                            assert(child_item.sz == sizeof(ASTreeExpansion*));
                            if (child->cov_count == 0) break;
                        }
                        i++;
                    }
                    if (i == parent->n_expansions) child_id = (uint32_t)rand() % parent->n_expansions;
                } else {
                    child_id = (uint32_t)rand() % parent->n_expansions;
                }

                {
                    Item const child_item   = get_chunk(ast->chunk, child_id);
                    ASTreeExpansion* child  = child_item.p;
                    assert(child_item.sz == sizeof(ASTreeExpansion*));
                    child->cov_count++;
                    assert(ast->n_terms_covered_once > 0);
                    ast->n_terms_not_covered--;
                    ast->n_terms_covered_once++;
                    while (child != NULL) {
                        if (isTerminal_aste(ast, child)) {
                            Item const exp_item = get_chunk(ast->chunk, child->str_id);
                            appendLast_chunk(str_builder, exp_item.p, exp_item.sz);
                        } else {
                            pushTop_alist(stack, &(child->node_id));
                        }
                        child = next_aste(ast, child);
                    }
                }
            }
        }

        destruct_alist(stack);
    }

    {
        Item const item             = getLast_chunk(str_builder);
        uint_fast64_t const index   = hash64_str(item.p, item.sz);
        bool ins_result;
        IndexMapping* mapping = insert_itbl(
            &ins_result,
            str_builder_tbl,
            index,
            LEN_CHUNK(str_builder) - 1,
            ITBL_RELATION_ONE_TO_ONE,
            ITBL_BEHAVIOR_RESPECT
        );
        if (unique && ins_result == ITBL_INSERT_NOT_UNIQUE) {
            Item const dup_item = get_chunk(ast->chunk, mapping->value);
            while (!areEquiv_item(item, dup_item)) {
                if (areEquiv_item(item, dup_item)) {
                    deleteLast_chunk(str_builder);
                    return NOT_AN_ITEM;
                }
                mapping = nextMapping_itbl(str_builder_tbl, mapping);
            }
        }
        return item;
    }
}

ASTreeNode* getNode_ast(
    ASTree const* const ast,
    uint32_t const node_id
) {
    assert(isValid_ast(ast));
    assert(node_id < LEN_CHUNK(ast->chunk));
    {
        Item const item = get_chunk(ast->chunk, node_id);
        assert(item.sz == sizeof(ASTreeNode));
        assert(isValid_astn(ast, item.p));
        return item.p;
    }
}

uint32_t getTermCov_ast(ASTree const* const ast) {
    assert(isValid_ast(ast));
    return (100 * ast->n_terms_covered_once) / ast->n_terms_total;
}

bool isAllocated_ast(ASTree const* const ast) {
    if (ast == NULL)                        return 0;
    if (!isAllocated_chunk(ast->chunk))     return 0;
    assert(isAllocated_itbl(ast->chunktbl));

    return 1;
}

bool isTerminal_aste(
    ASTree const* const ast,
    ASTreeExpansion const* const expansion
) {
    assert(isValid_ast(ast));
    assert(isValid_aste(ast, expansion));
    return expansion->str_id < LEN_CHUNK(ast->chunk);
}

bool isValid_ast(ASTree const* const ast) {
    if (ast == NULL)                                                                    return 0;
    if (!isValid_chunk(ast->chunk))                                                     return 0;
    if (!isValid_itbl(ast->chunktbl))                                                   return 0;
    if (ast->root_id >= LEN_CHUNK(ast->chunk))                                          return 0;
    if (ast->n_terms_total >= LEN_CHUNK(ast->chunk))                                   return 0;
    if (ast->n_terms_not_covered >= LEN_CHUNK(ast->chunk))                             return 0;
    if (ast->n_terms_covered_once >= LEN_CHUNK(ast->chunk))                            return 0;
    if (ast->n_terms_covered_once + ast->n_terms_not_covered != ast->n_terms_total)  return 0;

    return 1;
}

bool isValid_aste(
    ASTree const* const ast,
    ASTreeExpansion const* const expansion
) {
    assert(isValid_ast(ast));
    if (expansion == NULL)                                                                          return 0;
    if (expansion->str_id < LEN_CHUNK(ast->chunk)   && expansion->node_id < LEN_CHUNK(ast->chunk))  return 0;
    if (expansion->str_id >= LEN_CHUNK(ast->chunk)  && expansion->node_id >= LEN_CHUNK(ast->chunk)) return 0;

    return 1;
}

bool isValid_astn(
    ASTree const* const ast,
    ASTreeNode const* const node
) {
    assert(isValid_ast(ast));
    if (node == NULL)                               return 0;
    if (node->str_id >= LEN_CHUNK(ast->chunk))      return 0;
    if (node->n_expansions > AST_MAX_N_EXPANSIONS)  return 0;

    return 1;
}

ASTreeExpansion* next_aste(
    ASTree const* const ast,
    ASTreeExpansion const* const expansion
) {
    assert(isValid_ast(ast));
    assert(isValid_aste(ast, expansion));

    if (expansion->next_id >= LEN_CHUNK(ast->chunk)) {
        return NULL;
    } else {
        Item const item = get_chunk(ast->chunk, expansion->next_id);
        assert(item.sz == sizeof(ASTreeExpansion));
        assert(isValid_aste(ast, item.p));
        return item.p;
    }
}

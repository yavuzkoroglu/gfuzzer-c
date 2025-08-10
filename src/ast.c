#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "padkit/hash.h"
#include "padkit/implication.h"
#include "padkit/invalid.h"
#include "padkit/size.h"

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
    ASTreeNode* parent;
    ASTreeNode* child;
    ASTreeExpansion* expansion;
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
    ast->root_id = INVALID_UINT32;
    ast->n_tokens_not_covered   = 0;
    ast->n_tokens_covered_once  = 0;
    ast->n_tokens_total         = 0;

    addF_chunk(ast->chunk, bnf_file, BNF_MAX_SZ_FILE, BUFSIZ);
    cutByDelimLast_chunk(ast->chunk, "\n");

    for (uint32_t line_id = LEN_CHUNK(ast->chunk) - 1; line_id > 0; i--) {
        Item const line             = get_chunk(ast->chunk, line_id);
        char const* const line_str  = (char*)line.p;
        uint32_t i                  = 0;
        uint32_t j                  = 0;
        uint32_t exp_len            = 0;
        uint32_t token_len          = 0;
        uint_fast64_t token_index   = 0;
        uint_fast64_t exp_index     = 0;

        /* Skip spaces */
        while (i < line.sz && isspace(line_str[i])) i++;

        /* Ignore comments */
        if (i == line.sz || LITEQ(line_str + i, BNF_STR_LINE_COMMENT)) continue;

        /* Rule token must start with BNF_STR_RULE_OPEN */
        if (!LITEQ(line_str + i, BNF_STR_RULE_OPEN)) return AST_SYNTAX_ERROR;

        token_len   = sizeof(BNF_STR_RULE_OPEN);
        j           = i + token_len - 1;
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
            if (root_str == NULL && LEN_CHUNK(ast->chunk) == 0)
                ast->root_id = 0;
            else if (token_len == root_len && memcmp(token_str, root_str, token_len) == 0)
                ast->root_id = LEN_CHUNK(ast->chunk);

            add_chunk(ast->chunk, token_str, token_len);
            parent                  = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeNode)).p;
            parent->cov_count       = 0;
            parent->str_id          = LEN_CHUNK(chunk) - 2;
            parent->n_expansions    = 0;

            ast->n_tokens_total++;
            ast->n_tokens_not_covered++;
        } else {
            parent = get_chunk(ast->chunk, mapping->value).p;
            assert(isValid_astn(parent));
        }

        /* Skip BNF_STR_RULE_CLOSE */
        i = j + sizeof(BNF_STR_RULE_CLOSE) - 1;

        /* Skip spaces */
        while (i < line.sz && isspace(line_str[i])) i++;

        /* Must have something after the rule declaration */
        if (i == line.sz) return AST_SYNTAX_ERROR;

        /* Must have the BNF_STR_EQUIV operator */
        if (!LITEQ(line_str + i, BNF_STR_EQUIV)) return AST_SYNTAX_ERROR;

        /* Skip that operator */
        i += sizeof(BNF_STR_EQUIV) - 1;

        /* Skip spaces */
        while (i < line.sz && isspace(line_str[i])) i++;

        /* Cannot have empty expansion */
        if (i == line.sz) return AST_SYNTAX_ERROR;

        /* Must have at least one expansion. */
        parent->expansion_ids[parent->n_expansions++] = LEN_CHUNK(ast->chunk);
        expansion = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeExpansion)).p;
        expansion->cov_count    = 0;
        expansion->next_id      = INVALID_UINT32;
        ast->n_tokens_total++;
        ast->n_tokens_not_covered++;

        while (i < line.sz) {
            if (LITEQ(line_str + i, BNF_STR_ALTERNATIVE)) {
                assert(parent->n_expansions < AST_MAX_N_EXPANSIONS);
                parent->expansion_ids[parent->n_expansions++] = LEN_CHUNK(ast->chunk);
                expansion = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeExpansion)).p;
                expansion->cov_count    = 0;
                expansion->next_id      = INVALID_UINT32;
                ast->n_tokens_total++;
                ast->n_tokens_not_covered++;

                /* Skip BNF_STR_ALTERNATIVE */
                i += sizeof(BNF_STR_ALTERNATIVE) - 1;

                /* Skip spaces */
                while (i < line.sz && isspace(line_str[i])) i++;

                /* Cannot have empty expansion */
                if (i == line.sz) return AST_SYNTAX_ERROR;
            } else if (LITEQ(line_str + i, BNF_STR_RULE_OPEN)) {
                j = i + sizeof(BNF_STR_RULE_OPEN) - 1;
                exp_len = sizeof(BNF_STR_RULE_OPEN) - 1;
                if (j >= line.sz)                                   return AST_SYNTAX_ERROR;
                if (LITEQ(line_str + j, BNF_STR_RULE_CLOSE))        return AST_SYNTAX_ERROR;
                do {
                    if (isspace(line_str[j]))                       return AST_SYNTAX_ERROR;
                    if (++exp_len > AST_MAX_LEN_EXPANSION)          return AST_SYNTAX_ERROR;
                    if (++j >= line.sz)                             return AST_SYNTAX_ERROR;
                } while (!LITEQ(line_str + j, BNF_STR_RULE_CLOSE));
                exp_len += sizeof(BNF_STR_RULE_CLOSE) - 1;
                if (exp_len > AST_MAX_LEN_EXPANSION)                return AST_SYNTAX_ERROR;
                expansion->str_id = INVALID_UINT32;
                exp_index = hash64_str(exp_str, exp_len);
                ins_result  = insert_itbl(
                    mapping,
                    ast->chunktbl,
                    exp_index,
                    LEN_CHUNK(chunk),
                    ITBL_BEHAVIOR_RESPECT,
                    ITBL_RELATION_ONE_TO_ONE
                );
                if (ins_result == ITBL_INSERT_UNIQUE) {
                    if (root_str != NULL && exp_len == root_len && memcmp(exp_str, root_str, exp_len) == 0)
                        ast->root_id = LEN_CHUNK(ast->chunk);

                    add_chunk(ast->chunk, token_str, token_len);
                    child               = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeNode)).p;
                    child->cov_count    = 0;
                    child->str_id       = LEN_CHUNK(chunk) - 2;
                    child->n_expansions = 0;

                    ast->n_tokens_total++;
                    ast->n_tokens_not_covered++;
                }

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
                exp_len = 0;
                if (j >= line.sz)                                   return AST_SYNTAX_ERROR;
                if (LITEQ(line_str + j, BNF_STR_TERMINAL_CLOSE))    return AST_SYNTAX_ERROR;
                do {
                    if (++exp_len > AST_MAX_LEN_EXPANSION)          return AST_SYNTAX_ERROR;
                    if (++j >= line.sz)                             return AST_SYNTAX_ERROR;
                } while (!LITEQ(line_str + j, BNF_STR_TERMINAL_CLOSE));
                expansion->node_id  = INVALID_UINT32;
                expansion->str_id   = LEN_CHUNK(ast->chunk);
                add_chunk(ast->chunk, line_str + i, exp_len);
                i = j + 1;

                /* Skip spaces */
                while (i < line.sz && isspace(line_str[i])) i++;

                if (i < line.sz) {
                    expansion->next_id = LEN_CHUNK(ast->chunk);
                    expansion = addIndeterminate_chunk(ast->chunk, sizeof(ASTreeExpansion)).p;
                    expansion->cov_count    = 0;
                    expansion->next_id      = INVALID_UINT32;
                }
            } else if (isspace(line_str + i)) {
                /* Skip spaces */
                i++;
            } else {
                return AST_SYNTAX_ERROR;
            }
        }
    }

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
    ASTree const* const ast,
    uint32_t const root_id,
    bool const is_cov_guided,
    bool const unique
) {
    assert(isValid_chunk(str_builder));
    assert(isValid_itbl(str_builder_tbl));
    assert(isValid_ast(ast));
    assert(root_id < LEN_CHUNK(ast->chunk));

    /* Must create an item with size 0 within str_builder */
    /* However, Chunk does not allow 0 size items. */
    /* The following code hacks str_builder and temporarily creates an item of size 0 */
    addIndeterminate_chunk(str_builder, BUFSIZ);
    deleteLastN_alist(str_builder->items, BUFSIZ);

    {
        ArrayList stack[1]  = { NOT_AN_ALIST };
        uint32_t stack_id   = 0;
        constructEmpty_alist(stack, sizeof(uint32_t), ast->n_tokens_total);

        pushTop_alist(stack, &root_id);
        while (stack_id < stack->len) {
            uint32_t const parent_id    = get_alist(stack, stack_id++);
            ASTreeNode* const parent    = getNode_ast(ast, parent_id);
            if (parent->cov_count == 0) {
                assert(ast->n_tokens_covered_once);
                ast->n_tokens_covered_once--;
                ast->n_tokens_not_covered++;
            }
            if (parent->cov_count < SZ32_MAX) {
                parent->cov_count++;
            }

            if (parent->n_expansions == 0) {
                Item const parent_item = get_chunk(ast->chunk, parent_id);
                appendLast_chunk(ast->chunk, parent_item.p, parent_item.sz);
            } else {
                uint32_t child_id = 0;
                if (is_cov_guided) {
                    uint32_t cov_count  = 0;
                    uint32_t i          = 0;
                    while (i < n_expansions) {
                        child_id = parent->expansions[i];
                        {
                            ASTreeNodeExpansion const* child    = getExpansion_ast(ast, child_id);
                            cov_count                           = child;
                        }
                        if (cov_count > 0) break;
                        i++;
                    }
                    if (i == n_expansions) child_id = rand() % n_expansions;
                } else {
                    child_id = rand() % n_expansions;
                }

                {
                    ASTreeNodeExpansion const* child = getExpansion_ast(ast, child_id);
                    while (child != NULL) {
                        if (isTerminal_aste(ast, child)) {
                            Item const exp_item = get_chunk(ast->chunk, child->str_id);
                            appendLast_chunk(ast->chunk, exp_item.p, exp_item.sz);
                        } else {
                            pushTop_alist(stack, child->node_id);
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
        IndexMapping const* mapping = findFirstMapping_itbl(str_builder_tbl, index);
        while (mapping != NULL) {
            Item const dup          = get_chunk(str_builder, mapping->value);
            if (areEquiv_item(item, dup)) {
                deleteLast_chunk(str_builder);
                if (unique)
                    return NULL;
                else
                    return dup;
            }
            mapping = nextMapping_itbl(str_builder_tbl, mapping);
        }
        return item;
    }
}

ASTreeNode* getNode_ast(
    ASTree const* const ast,
    uint32_t const node_id
) {
    assert(isValid_ast(ast));
    assert(parent_id < LEN_CHUNK(ast->chunk));
    {
        Item const item = get_chunk(ast->chunk, parent_id);
        assert(item.sz == sizeof(ASTreeNode));
        assert(isValid_astn(ast, item.p));
        return item.p;
    }
}

uint32_t getTokenCov_ast(ASTree const* const ast) {
    assert(isValid_ast(ast));
    return (100 * ast->n_tokens_covered_once) / ast->n_tokens_total;
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
    if (ast == NULL)                                                    return 0;
    if (!isValid_chunk(ast->chunk))                                     return 0;
    if (!isValid_itbl(ast->chunktbl))                                   return 0;
    if (root_id >= LEN_CHUNK(chunk))                                    return 0;
    if (n_tokens_total >= LEN_CHUNK(chunk))                             return 0;
    if (n_tokens_not_covered >= LEN_CHUNK(chunk))                       return 0;
    if (n_tokens_covered_once >= LEN_CHUNK(chunk))                      return 0;
    if (n_tokens_covered_once + n_tokens_not_covered != n_tokens_total) return 0;

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

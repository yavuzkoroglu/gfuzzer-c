#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <string.h>
#include "bnf.h"
#include "grammargraph.h"
#include "padkit/indextable.h"
#include "padkit/invalid.h"
#include "padkit/repeat.h"
#include "padkit/size.h"
#include "padkit/swap.h"
#include "padkit/verbose.h"

#ifdef LITEQ
    #undef LITEQ
#endif
#ifdef LITNEQ
    #undef LITNEQ
#endif
#ifdef IS_COMMENT_OR_EMPTY
    #undef IS_COMMENT_OR_EMPTY
#endif
#define LITEQ(str, lit)     (strncmp(str, lit, sizeof(lit) - 1) == 0)
#define LITNEQ(str, lit)    (strncmp(str, lit, sizeof(lit) - 1) != 0)

#define IS_COMMENT_OR_EMPTY(line_begin, i, line_sz)     \
        (i == line_sz || line_begin[i] == '\0' || LITEQ(line_begin + i, BNF_STR_LINE_COMMENT))

static int addRule(
    RuleTerm** parent,
    GrammarGraph* const graph,
    IndexTable* const rule_tbl,
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
);

static int determineRootRule(
    GrammarGraph* const graph,
    IndexTable* const rule_tbl,
    char* const root_str,
    uint32_t const root_len
);

static int load_ggraph(
    GrammarGraph* const graph,
    IndexTable* const rule_tbl,
    IndexTable* const terminal_tbl,
    Chunk* const lines,
    char* const root_str,
    uint32_t const root_len
);

static int skipEquiv(
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
);

static int skipSpaces(
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
);

static int addRule(
    RuleTerm** parent,
    GrammarGraph* const graph,
    IndexTable* const rule_tbl,
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
) {
    uint_fast64_t term_index;
    IndexMapping* mapping;
    bool ins_result;

    Item term   = add_chunk(graph->rule_names, BNF_STR_RULE_OPEN, sizeof(BNF_STR_RULE_OPEN) - 1);
    uint32_t j  = *i + term.sz;

    if (LITNEQ(line_begin + *i, BNF_STR_RULE_OPEN)) return GRAMMAR_SYNTAX_ERROR;
    if (j == line_sz || isspace(line_begin[j]))     return GRAMMAR_SYNTAX_ERROR;
    while (LITNEQ(line_begin + j, BNF_STR_RULE_CLOSE)) {
        term = appendLast_chunk(graph->rule_names, line_begin + j++, 1);
        if (j == line_sz || isspace(line_begin[j]) || term.sz >= BNF_MAX_LEN_TERM) return GRAMMAR_SYNTAX_ERROR;
    }
    *i = j + sizeof(BNF_STR_RULE_CLOSE) - 1;

    term_index  = hash64_item(term);
    mapping     = insert_itbl(
        &ins_result,
        rule_tbl,
        term_index,
        graph->rule_list->len,
        ITBL_BEHAVIOR_RESPECT,
        ITBL_RELATION_ONE_TO_ONE
    );
    if (ins_result == ITBL_INSERT_UNIQUE) {
        *parent                 = addIndeterminate_alist(graph->rule_list);
        parent[0]->name_id      = LEN_CHUNK(graph->rule_names) - 1;
        parent[0]->cov_count    = 0;
        parent[0]->first_exp_id = LEN_CHUNK(graph->exp_list);
        parent[0]->n_alt_exps   = 1;
    } else {
        RuleTerm* dup_term  = get_alist(graph->rule_list, mapping->value);
        Item dup_name       = get_chunk(graph->rule_names, dup_term->name_id);
        while (!areEquiv_item(dup_name, term)) {
            if (mapping->next_id >= rule_tbl->mappings->len) {
                mapping->next_id    = rule_tbl->mappings->len;
                mapping             = addIndeterminate_alist(rule_tbl->mappings);
                mapping->index      = term_index;
                mapping->value      = graph->rule_list->len;
                mapping->next_id    = INVALID_UINT32;
                add_alist(graph->rule_list, *parent);
                return GRAMMAR_OK;
            }
            mapping     = nextMapping_itbl(rule_tbl, mapping);
            dup_term    = get_alist(graph->rule_list, mapping->value);
            dup_name    = get_chunk(graph->rule_names, dup_term->name_id);
        }
        *parent = dup_term;
        deleteLast_chunk(graph->rule_names);
    }
    return GRAMMAR_OK;
}

bool isValid_ggraph(GrammarGraph const* const graph) {
    if (graph == NULL)                                                                  return 0;
    if (!isValid_chunk(graph->rule_names))                                              return 0;
    if (!isValid_chunk(graph->terminals))                                               return 0;
    if (!isValid_alist(graph->rule_list))                                               return 0;
    if (!isValid_alist(graph->exp_list))                                                return 0;
    if (graph->root_rule_id >= graph->rule_list->len)                                   return 0;
    if (graph->n_cov <= graph->rule_list->len + graph->exp_list->len)    return 0;

    return 1;
}

int construct_ggraph(
    GrammarGraph* const graph,
    FILE* const bnf_file,
    char* const root_str,
    uint32_t const root_len
) {
    Chunk lines[1]              = { NOT_A_CHUNK };
    IndexTable rule_tbl[1]      = { NOT_AN_ITBL };
    IndexTable terminal_tbl[2]  = { NOT_AN_ITBL };
    int construct_res           = GRAMMAR_OK;

    assert(graph != NULL);
    assert(bnf_file != NULL);

    constructEmpty_chunk(lines, CHUNK_RECOMMENDED_PARAMETERS);
    constructEmpty_itbl(rule_tbl, ITBL_RECOMMENDED_PARAMETERS);
    constructEmpty_itbl(terminal_tbl, ITBL_RECOMMENDED_PARAMETERS);

    addF_chunk(lines, bnf_file, BNF_MAX_SZ_FILE, BNF_MAX_SZ_FILE);
    cutByDelimLast_chunk(lines, "\n");
    fprintf_verbose(stderr, "GRAMMAR_GRAPH: # Lines = %"PRIu32, LEN_CHUNK(lines));

    constructEmpty_chunk(graph->rule_names, CHUNK_RECOMMENDED_PARAMETERS);
    constructEmpty_chunk(graph->terminals, CHUNK_RECOMMENDED_PARAMETERS);
    constructEmpty_alist(graph->rule_list, sizeof(RuleTerm), ALIST_RECOMMENDED_INITIAL_CAP);
    constructEmpty_alist(graph->exp_list, sizeof(ExpansionTerm), ALIST_RECOMMENDED_INITIAL_CAP);

    construct_res = load_ggraph(graph, rule_tbl, terminal_tbl, lines, root_str, root_len);

    if (construct_res != GRAMMAR_OK) {
        destruct_chunk(graph->rule_names);
        destruct_chunk(graph->terminals);
        destruct_chunk(graph->rule_list);
        destruct_chunk(graph->exp_list);
    }

    destruct_chunk(lines);
    destruct_itbl(rule_tbl);
    destruct_itbl(terminal_tbl);

    return construct_res;
}

void destruct_ggraph(GrammarGraph* const graph) {
    assert(isValid_ggraph(graph));

    destruct_chunk(graph->rule_names);
    destruct_chunk(graph->terminals);
    destruct_alist(graph->rule_list);
    destruct_alist(graph->exp_list);

    *graph = NOT_A_GGRAPH;
}

static int determineRootRule(
    GrammarGraph* const graph,
    IndexTable* const rule_tbl,
    char* const root_str,
    uint32_t const root_len
) {
    if (root_str == NULL) {
        graph->root_rule_id = 0;
        return GRAMMAR_OK;
    } else {
        Item const root             = { root_str, root_len, 0 };
        uint_fast64_t root_index    = hash64_item(root);
        IndexMapping* mapping       = findFirstMapping_itbl(rule_tbl, root_index);
        while (mapping != NULL) {
            RuleTerm const* const rule  = get_alist(graph->rule_list, mapping->value);
            Item const rule_name        = get_chunk(graph->rule_names, rule->name_id);
            if (areEquiv_item(root, rule_name)) {
                graph->root_rule_id = mapping->value;
                return GRAMMAR_OK;
            }
            mapping = nextMapping_itbl(rule_tbl, mapping);
        }
        return GRAMMAR_SYNTAX_ERROR;
    }
}

static ExpansionTerm const* getAltExp_ggraph(
    GrammarGraph const* const graph,
    RuleTerm const* const rule,
    uint32_t const nth_alt
) {
    ExpansionTerm const* exp = get_alist(graph->exp_list, rule->first_alt_id);
    for (uint32_t i = 0; i < nth_alt; i++) {
        while (exp->hasNext) exp++;
        exp++;
    }
    return exp;
}

int generateSentence_ggraph(
    Chunk* const str_builder,
    GrammarGraph* const graph,
    ArrayList const* const decision_sequence
) {
    assert(isValid_chunk(str_builder));
    assert(isValid_ggraph(graph));
    assert(isValid_alist(decision_sequence));

    if (decision_sequence->len == 0) {
        return GRAMMAR_SENTENCE_ERROR;
    } else {
        ArrayList stacks[2][1]  = { { NOT_AN_ALIST }, { NOT_AN_ALIST } };
        ArrayList* stack_A      = stacks[0];
        ArrayList* stack_B      = stacks[1];
        RuleTerm* rule          = get_alist(graph->rule_list, graph->root_rule_id);
        uint32_t decision_id    = 0;
        uint32_t alt_id         = *(uint32_t*)get_alist(decision_sequence, decision_id++);
        ExpansionTerm* exp      = getAltExp_ggraph(graph, rule, alt_id);
        uint32_t allTerminals   = 1;

        if (rule->cov_count == 0)       graph->n_cov++;
        if (rule->cov_count < SZ32_MAX) rule->cov_count++;

        constructEmpty_alist(stack_A, sizeof(ExpansionTerm), ALIST_RECOMMENDED_INITIAL_CAP);
        constructEmpty_alist(stack_B, sizeof(ExpansionTerm), ALIST_RECOMMENDED_INITIAL_CAP);

        allTerminals &= exp->is_terminal;
        add_alist(stack_A, exp);
        while (exp->has_next) {
            exp++;
            allTerminals &= exp->is_terminal;
            add_alist(stack_A, exp);
        }
        while (!allTerminals) {
            exp             = getFirst_alist(stack_A);
            allTerminals    = 1;
            REPEAT(stack_A->len) {
                if (exp->cov_count == 0)        graph->n_cov++;
                if (exp->cov_count < SZ32_MAX)  exp->cov_count++;

                if (exp->is_terminal) {
                    add_alist(stack_B, exp);
                } else {
                    rule = get_alist(graph->rule_list, exp->rt_id);
                    if (rule->cov_count == 0)           graph->n_cov++;
                    if (rule->cov_count < SZ32_MAX)     rule->cov_count++;

                    if (decision_id >= decision_sequence->len) {
                        destruct_alist(stack_A);
                        destruct_alist(stack_B);
                        return GRAMMAR_SENTENCE_ERROR;
                    }
                    alt_id  = *(uint32_t*)get_alist(decision_sequence, decision_id++);
                    exp     = getAltExp_ggraph(graph, rule, alt_id);

                    allTerminals &= exp->is_terminal;
                    add_alist(stack_B, exp);
                    while (exp->hasNext) {
                        exp++;
                        allTerminals &= exp->is_terminal;
                        add_alist(stack_B, exp);
                    }
                }
                exp++;
            }
            flush_alist(stack_A);
            swap(stack_A, stack_B, sizeof(ArrayList));
        }
        destruct_alist(stack_B);

        addIndeterminate_chunk(str_builder, 0);
        exp = getFirst_alist(stack_A);
        REPEAT(stack_A->len) {
            Item const terminal = get_chunk(graph->terminals, exp->rt_id);
            appendLast_chunk(str_builder, terminal.p, terminal.sz);
            exp++;
        }
        destruct_alist(stack_A);
        return GRAMMAR_SENTENCE_OK;
    }
}

static int load_ggraph(
    GrammarGraph* const graph,
    IndexTable* const rule_tbl,
    IndexTable* const terminal_tbl,
    Chunk* const lines,
    char* const root_str,
    uint32_t const root_len
) {
    int load_res = GRAMMAR_OK;

    assert(graph != NULL);
    assert(isValid_itbl(rule_tbl));
    assert(isValid_itbl(terminal_tbl));
    assert(isValid_chunk(lines));

    for (uint32_t line_id = 0; line_id < LEN_CHUNK(lines); line_id++) {
        Item const line                 = get_chunk(lines, line_id);
        char const* const line_begin    = line.p;
        uint32_t i                      = 0;
        RuleTerm* rule;

        load_res = skipSpaces(line_begin, line.sz, &i);
        assert(load_res == GRAMMAR_OK);

        if (IS_COMMENT_OR_EMPTY(line_begin, i, line.sz)) continue;

        load_res = addRule(&rule, graph, rule_tbl, line_begin, line.sz, &i);
        if (load_res != GRAMMAR_OK) return GRAMMAR_SYNTAX_ERROR;

        load_res = skipSpaces(line_begin, line.sz, &i);
        assert(load_res == GRAMMAR_OK);

        load_res = skipEquiv(line_begin, line.sz, &i);
        if (load_res != GRAMMAR_OK) return GRAMMAR_SYNTAX_ERROR;

        load_res = skipSpaces(line_begin, line.sz, &i);
        assert(load_res == GRAMMAR_OK);

        load_res = addExpansions(graph, terminal_tbl, rule, line_begin, line.sz, &i);
        if (load_res != GRAMMAR_OK) return GRAMMAR_SYNTAX_ERROR;
    }

    load_res = determineRootRule(graph, rule_tbl, root_str, root_len);
    if (load_res != GRAMMAR_OK)
        return GRAMMAR_SYNTAX_ERROR;
    else
        return GRAMMAR_OK;
}

uint32_t nTerms_ggraph(GrammarGraph const* const graph) {
    assert(isValid_ggraph(graph));
    return graph->rule_list->len + graph->expansion_list->len;
}

void printDot_ggraph(
    FILE* const output,
    GrammarGraph const* const graph
) {
    uint32_t exp_uid = 1;

    assert(output != NULL);
    assert(isValid_ggraph(graph));

    fprintf(output,
            "digraph GrammarGraph {\n"
            "    edge [fontname=\"PT Mono\"];\n"
            "    node [fontname=\"PT Mono\"];\n"
            "    root [shape=\"none\",width=0,height=0,label=\"\"];\n"
            "\n"
    );
    {
        RuleTerm const* rule = getFirst_alist(graph->rule_list);
        REPEAT(graph->rule_list->len) {
            Item const rule_name        = get_chunk(graph->rule_names, rule->name_id);
            ExpansionTerm const* exp    = get_alist(graph->exp_list, rule->first_alt_id);

            fprintf(output,
                "    \"%.*s\"->e%"PRIu32":p1;\n"
                "    e%"PRIu32" [shape=\"record\",label=\"",
                (int)rule_name.sz, (char*)rule_name.p, exp_uid,
                exp_uid
            );

            for (uint32_t i = 1; i <= rule->n_alt; i += !(exp++)->hasNext) {
                Item child_name     = NOT_AN_ITEM;
                uint32_t port_uid   = 1;

                fprintf(output, "<p%"PRIu32">", port_uid++);
                if (exp->is_terminal) {
                    child_name = get_chunk(graph->terminals, exp->rt_id);
                    fprintf(output, "%.*s", (int)child_name.sz, (char*)child_name.p);
                } else {
                    RuleTerm const* child   = get_alist(graph->rule_list, exp->rt_id);
                    child_name              = get_chunk(graph->rule_names, child->name_id);
                    fprintf(output,
                        "\\%.*s\\"BNF_STR_RULE_CLOSE,
                        (int)child_name.sz + 1 - sizeof(BNF_STR_RULE_CLOSE), (char*)child_name.p
                    );
                }

                if (exp->hasNext) {
                    fprintf(output, BNF_STR_ALTERNATIVE);
                } else {
                    uint32_t const n_ports = port_uid;

                    fprintf(output, "\"];\n");

                    exp -= n_ports - 1;
                    for (port_uid = 1; port_id <= n_ports; port_id++) {
                        if (exp->is_terminal) {
                            child_name = get_chunk(graph->terminals, exp->rt_id);
                        } else {
                            RuleTerm const* child   = get_alist(graph->rule_list, exp->rt_id);
                            child_name              = get_chunk(graph->rule_names, child->name_id);
                        }
                        fprintf(output,
                            "    e%"PRIu32":p%"PRIu32"->\"%.*s\";\n",
                            exp_uid, port_uid,
                            (int)child_name.sz, (char*)child_name.p
                        );
                    }
                    exp += n_ports - 1;

                    exp_uid++;
                    if (i < rule->n_alt)
                        fprintf(output,
                            "    \"%.*s\"->e%"PRIu32":p1;\n"
                            "    e%"PRIu32" [shape=\"record\",label=\"",
                            (int)rule_name.sz, (char*)rule_name.p, exp_uid,
                            exp_uid
                        );
                }
            }
        }
    }

    fprintf(output, "\n");

    for (uint32_t i = 0; i < LEN_CHUNK(graph->terminals); i++) {
        Item const terminal = get_chunk(graph->terminals, i);
        fprintf(output, "    \".*s\" [shape=\"none\",height=0];\n", (int)terminal.sz, (char*)terminal.p);
    }

    fprintf(output, "}\n");
}

static int skipSpaces(
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
) {
    while (*i < line_sz || isspace(line_begin[*i])) (*i)++;
    return GRAMMAR_OK;
}

static int skipEquiv(
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
) {
    if (*i == line_sz || line_begin[*i] == '\0')    return GRAMMAR_SYNTAX_ERROR;
    if (LITNEQ(line_begin + *i, BNF_STR_EQUIV))     return GRAMMAR_SYNTAX_ERROR;

    *i += sizeof(BNF_STR_EQUIV) - 1;
    return GRAMMAR_OK;
}

uint32_t termCov_ggraph(GrammarGraph const* const graph) {
    assert(isValid_ggraph(graph));
    {
        uint32_t const n_total      = nTerms_ggraph(graph);
        uint32_t const n_cov_x_100  = 100 * graph->n_cov;
        assert(n_cov_x_100 / 100 == graph->n_cov);

        return n_cov_x_100 / n_terms_total;
    }
}

#undef LITEQ
#undef LITNEQ
#undef IS_COMMENT_OR_EMPTY


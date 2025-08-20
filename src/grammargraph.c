#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <string.h>
#include "bnf.h"
#include "grammargraph.h"
#include "padkit/chunktable.h"
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

static int addExpansions(
    GrammarGraph* const graph,
    ChunkTable* const rule_tbl,
    ChunkTable* const terminal_tbl,
    uint32_t const rule_id,
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
);

static int addRule(
    uint32_t* const p_rule_id,
    GrammarGraph* const graph,
    ChunkTable* const rule_tbl,
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
);

static int determineRootRule(
    GrammarGraph* const graph,
    ChunkTable* const rule_tbl,
    char* const root_str,
    uint32_t const root_len
);

static ExpansionTerm* getAltExp(
    GrammarGraph* const graph,
    RuleTerm* const rule,
    uint32_t const jth_alt
);

static int load_ggraph(
    GrammarGraph* const graph,
    ChunkTable* const rule_tbl,
    ChunkTable* const terminal_tbl,
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

static int addExpansions(
    GrammarGraph* const graph,
    ChunkTable* const rule_tbl,
    ChunkTable* const terminal_tbl,
    uint32_t const rule_id,
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
) {
    RuleTerm* rule      = get_alist(graph->rule_list, rule_id);
    uint32_t j          = rule->n_alt;
    ExpansionTerm* exp  = getAltExp(graph, rule, j);
    exp->cov_count      = 0;
    exp->has_next       = 1;

    if (*i == line_sz || line_begin[*i] == '\0') return GRAMMAR_SYNTAX_ERROR;

    while (*i < line_sz && line_begin[*i] != '\0') {
        if (LITEQ(line_begin + *i, BNF_STR_RULE_OPEN)) {
            uint32_t child_id;
            int result = addRule(&child_id, graph, rule_tbl, line_begin, line_sz, i);
            if (result != GRAMMAR_OK) return GRAMMAR_SYNTAX_ERROR;

            exp->is_terminal    = 0;
            exp->rt_id          = child_id;

            exp = insertIndeterminate_alist(graph->exp_list, rule->first_alt_id + ++j);
            exp->cov_count  = 0;
            exp->has_next   = 1;
        } else if (LITEQ(line_begin + *i, BNF_STR_TERMINAL_OPEN)) {
            Item terminal;
            int result = addTerminal(&terminal, graph, terminal_tbl, line_begin, line_sz, i);
            if (result != GRAMMAR_OK) return GRAMMAR_SYNTAX_ERROR;

            exp = insertIndeterminate_alist(graph->exp_list, rule->first_alt_id + ++j);
            exp->cov_count  = 0;
            exp->has_next   = 1;
        } else if (LITEQ(line_begin + *i, BNF_STR_ALTERNATIVE)) {
            int const result = skipSpaces(line_begin, line_sz, i);
            assert(result == GRAMMAR_OK);

            if (i == line_sz || line_begin[*i] == '\0') return GRAMMAR_SYNTAX_ERROR;

            exp[-1].has_next = 0;
            rule->n_alt++;
        } else if (isspace((unsigned char)(line_begin + *i))) {
            i++;
        } else {
            return GRAMMAR_SYNTAX_ERROR;
        }
    }

    return GRAMMAR_OK;
}

static int addRule(
    uint32_t* const p_rule_id,
    GrammarGraph* const graph,
    ChunkTable* const rule_tbl,
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
) {
    ChunkMapping* mapping;
    bool ins_result;

    Item term   = add_chunk(graph->rule_names, BNF_STR_RULE_OPEN, sizeof(BNF_STR_RULE_OPEN) - 1);
    uint32_t j  = *i + term.sz;

    if (LITNEQ(line_begin + *i, BNF_STR_RULE_OPEN))             return GRAMMAR_SYNTAX_ERROR;
    if (j == line_sz || isspace((unsigned char)line_begin[j]))  return GRAMMAR_SYNTAX_ERROR;
    while (LITNEQ(line_begin + j, BNF_STR_RULE_CLOSE)) {
        term = appendLast_chunk(graph->rule_names, line_begin + j++, 1);
        if (j == line_sz || isspace((unsigned char)line_begin[j]) || term.sz >= BNF_MAX_LEN_TERM)
            return GRAMMAR_SYNTAX_ERROR;
    }
    *i = j + sizeof(BNF_STR_RULE_CLOSE) - 1;

    mapping = searchInsert_ctbl(&ins_result, rule_tbl, term, graph->rule_list->len, CTBL_MODE_INSERT_RESPECT);
    if (ins_result == CTBL_RESPECT_UNIQUE) {
        RuleTerm* const parent  = addIndeterminate_alist(graph->rule_list);
        parent->name_id         = LEN_CHUNK(graph->rule_names) - 1;
        parent->cov_count       = 0;
        parent->first_alt_id    = INVALID_UINT32;
        parent->n_alt           = 0;
    } else {
        deleteLast_chunk(graph->rule_names);
    }
    *p_rule_id = mapping->value;
    return GRAMMAR_OK;
}

int construct_ggraph(
    GrammarGraph* const graph,
    FILE* const bnf_file,
    char* const root_str,
    uint32_t const root_len
) {
    Chunk lines[1];
    ChunkTable rule_tbl[1];
    ChunkTable terminal_tbl[1];
    int construct_res = GRAMMAR_OK;

    assert(graph != NULL);
    assert(bnf_file != NULL);

    constructEmpty_chunk(lines, CHUNK_RECOMMENDED_PARAMETERS);
    constructEmpty_ctbl(rule_tbl, CTBL_RECOMMENDED_PARAMETERS);
    constructEmpty_ctbl(terminal_tbl, CTBL_RECOMMENDED_PARAMETERS);

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
    destruct_ctbl(rule_tbl);
    destruct_ctbl(terminal_tbl);

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
    ChunkTable* const rule_tbl,
    char* const root_str,
    uint32_t const root_len
) {
    if (root_str == NULL) {
        graph->root_rule_id = 0;
        return GRAMMAR_OK;
    } else {
        Item const root_item                = { root_str, root_len, 0 };
        ChunkMapping const* const mapping   = searchInsert_ctbl(
            NULL, rule_tbl, root_item, INVALID_UINT32, CTBL_MODE_SEARCH
        );
        if (mapping == NULL) return GRAMMAR_SYNTAX_ERROR;

        graph->root_rule_id = mapping->value;
        return GRAMMAR_OK;
    }
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
        ExpansionTerm* exp      = getAltExp(graph, rule, alt_id);
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
                    exp     = getAltExp(graph, rule, alt_id);

                    allTerminals &= exp->is_terminal;
                    add_alist(stack_B, exp);
                    while (exp->has_next) {
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

static ExpansionTerm* getAltExp(
    GrammarGraph* const graph,
    RuleTerm* const rule,
    uint32_t const jth_alt
) {
    if (rule->n_alt == 0) {
        assert(jth_alt == 0);
        assert(rule->first_alt_id == INVALID_UINT32);
        rule->n_alt         = 1;
        rule->first_alt_id  = graph->exp_list->len;
        return addIndeterminate_alist(graph->exp_list);
    } else {
        ExpansionTerm* exp = get_alist(graph->exp_list, rule->first_alt_id);

        uint32_t j = 0;
        for (uint32_t i = 0; i < jth_alt; i++, exp++, j++)
            while (exp->has_next) { exp++; j++; }

        if (jth_alt < rule->n_alt) {
            return exp;
        } else {
            rule->n_alt++;
            return insertIndeterminate_alist(graph->exp_list, rule->first_alt_id + j);
        }
    }
}

bool isValid_ggraph(GrammarGraph const* const graph) {
    if (graph == NULL)                                                  return 0;
    if (!isValid_chunk(graph->rule_names))                              return 0;
    if (!isValid_chunk(graph->terminals))                               return 0;
    if (!isValid_alist(graph->rule_list))                               return 0;
    if (!isValid_alist(graph->exp_list))                                return 0;
    if (graph->root_rule_id >= graph->rule_list->len)                   return 0;
    if (graph->n_cov <= graph->rule_list->len + graph->exp_list->len)   return 0;

    return 1;
}

static int load_ggraph(
    GrammarGraph* const graph,
    ChunkTable* const rule_tbl,
    ChunkTable* const terminal_tbl,
    Chunk* const lines,
    char* const root_str,
    uint32_t const root_len
) {
    uint32_t rule_id;
    int load_res = GRAMMAR_OK;

    assert(graph != NULL);
    assert(isValid_ctbl(rule_tbl));
    assert(isValid_ctbl(terminal_tbl));
    assert(isValid_chunk(lines));

    for (uint32_t line_id = 0; line_id < LEN_CHUNK(lines); line_id++) {
        Item const line                 = get_chunk(lines, line_id);
        char const* const line_begin    = line.p;
        uint32_t i                      = 0;

        load_res = skipSpaces(line_begin, line.sz, &i);
        assert(load_res == GRAMMAR_OK);

        if (IS_COMMENT_OR_EMPTY(line_begin, i, line.sz)) continue;

        load_res = addRule(&rule_id, graph, rule_tbl, line_begin, line.sz, &i);
        if (load_res != GRAMMAR_OK) return GRAMMAR_SYNTAX_ERROR;

        load_res = skipSpaces(line_begin, line.sz, &i);
        assert(load_res == GRAMMAR_OK);

        load_res = skipEquiv(line_begin, line.sz, &i);
        if (load_res != GRAMMAR_OK) return GRAMMAR_SYNTAX_ERROR;

        load_res = skipSpaces(line_begin, line.sz, &i);
        assert(load_res == GRAMMAR_OK);

        load_res = addExpansions(graph, rule_tbl, terminal_tbl, rule_id, line_begin, line.sz, &i);
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
    return graph->rule_list->len + graph->exp_list->len;
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

            for (uint32_t i = 1; i <= rule->n_alt; i += !(exp++)->has_next) {
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
                        (int)(child_name.sz + 1 - sizeof(BNF_STR_RULE_CLOSE)), (char*)child_name.p
                    );
                }

                if (exp->has_next) {
                    fprintf(output, BNF_STR_ALTERNATIVE);
                } else {
                    uint32_t const n_ports = port_uid;

                    fprintf(output, "\"];\n");

                    exp -= n_ports - 1;
                    for (port_uid = 1; port_uid <= n_ports; port_uid++) {
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
        fprintf(output, "    \"%.*s\" [shape=\"none\",height=0];\n", (int)terminal.sz, (char*)terminal.p);
    }

    fprintf(output, "}\n");
}

static int skipSpaces(
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
) {
    while (*i < line_sz || isspace((unsigned char)line_begin[*i])) (*i)++;
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

        return n_cov_x_100 / n_total;
    }
}

#undef LITEQ
#undef LITNEQ
#undef IS_COMMENT_OR_EMPTY


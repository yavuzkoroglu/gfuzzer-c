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

static uint32_t addRule(
    GrammarGraph* const graph,
    ChunkTable* const rule_tbl,
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
);

static uint32_t addTerminal(
    GrammarGraph* const graph,
    ChunkTable* const terminal_tbl,
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
    ExpansionTerm* exp  = NULL;

    add_alist(rule->alt_list, &(graph->exp_list->len));

    if (*i == line_sz || line_begin[*i] == '\0') return GRAMMAR_SYNTAX_ERROR;

    while (*i < line_sz && line_begin[*i] != '\0') {
        if (LITEQ(line_begin + *i, BNF_STR_RULE_OPEN)) {
            uint32_t const child_id = addRule(graph, rule_tbl, line_begin, line_sz, i);
            if (child_id == INVALID_UINT32) return GRAMMAR_SYNTAX_ERROR;

            exp                 = addIndeterminate_alist(graph->exp_list);
            exp->is_terminal    = 0;
            exp->rt_id          = child_id;
            exp->cov_count      = 0;
            exp->has_next       = 1;
        } else if (LITEQ(line_begin + *i, BNF_STR_TERMINAL_OPEN)) {
            uint32_t const terminal_id = addTerminal(graph, terminal_tbl, line_begin, line_sz, i);
            if (terminal_id == INVALID_UINT32) return GRAMMAR_SYNTAX_ERROR;

            exp                 = addIndeterminate_alist(graph->exp_list);
            exp->is_terminal    = 1;
            exp->rt_id          = terminal_id;
            exp->cov_count      = 0;
            exp->has_next       = 1;
        } else if (LITEQ(line_begin + *i, BNF_STR_ALTERNATIVE)) {
            int const result = skipSpaces(line_begin, line_sz, i);
            assert(result == GRAMMAR_OK);

            if (*i == line_sz || line_begin[*i] == '\0') return GRAMMAR_SYNTAX_ERROR;

            if (exp == NULL) return GRAMMAR_SYNTAX_ERROR;
            exp->has_next = 0;
            add_alist(rule->alt_list, &(graph->exp_list->len));
            (*i)++;
        } else if (isspace((unsigned char)*(line_begin + *i))) {
            (*i)++;
        } else {
            return GRAMMAR_SYNTAX_ERROR;
        }
    }
    if (exp == NULL) return GRAMMAR_SYNTAX_ERROR;
    exp->has_next = 0;

    return GRAMMAR_OK;
}

static uint32_t addRule(
    GrammarGraph* const graph,
    ChunkTable* const rule_tbl,
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
) {
    ChunkMapping* mapping   = NULL;
    bool ins_result         = 0;
    Item term               = add_chunk(graph->rule_names, BNF_STR_RULE_OPEN, sizeof(BNF_STR_RULE_OPEN) - 1);
    uint32_t j              = *i + term.sz;

    if (LITNEQ(line_begin + *i, BNF_STR_RULE_OPEN))             return INVALID_UINT32;
    if (j == line_sz || isspace((unsigned char)line_begin[j]))  return INVALID_UINT32;
    while (LITNEQ(line_begin + j, BNF_STR_RULE_CLOSE)) {
        term = appendLast_chunk(graph->rule_names, line_begin + j++, 1);
        if (j == line_sz || isspace((unsigned char)line_begin[j]) || term.sz >= BNF_MAX_LEN_TERM)
            return INVALID_UINT32;
    }
    term = appendLast_chunk(graph->rule_names, line_begin + j, sizeof(BNF_STR_RULE_CLOSE) - 1);
    j += sizeof(BNF_STR_RULE_CLOSE) - 1;
    *i = j;

    mapping = searchInsert_ctbl(&ins_result, rule_tbl, term, graph->rule_list->len, CTBL_MODE_INSERT_RESPECT);
    if (ins_result == CTBL_RESPECT_UNIQUE) {
        RuleTerm* const parent  = addIndeterminate_alist(graph->rule_list);
        parent->name_id         = LEN_CHUNK(graph->rule_names) - 1;
        parent->cov_count       = 0;
        parent->alt_list[0]     = NOT_AN_ALIST;
        constructEmpty_alist(parent->alt_list, sizeof(uint32_t), ALIST_RECOMMENDED_INITIAL_CAP);
    } else {
        deleteLast_chunk(graph->rule_names);
    }
    return mapping->value;
}

static uint32_t addTerminal(
    GrammarGraph* const graph,
    ChunkTable* const terminal_tbl,
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
) {
    ChunkMapping* mapping   = NULL;
    Item term               = addIndeterminate_chunk(graph->terminals, 0);
    uint32_t j              = *i;

    if (LITNEQ(line_begin + j, BNF_STR_TERMINAL_OPEN))  return INVALID_UINT32;
    j += sizeof(BNF_STR_TERMINAL_OPEN) - 1;
    if (j == line_sz || line_begin[j] == '\0')          return INVALID_UINT32;
    while (LITNEQ(line_begin + j, BNF_STR_TERMINAL_CLOSE)) {
        term = appendLast_chunk(graph->terminals, line_begin + j++, 1);
        if (j == line_sz || line_begin[j] == '\0' || term.sz >= BNF_MAX_LEN_TERM)
            return INVALID_UINT32;
    }
    *i = j + sizeof(BNF_STR_TERMINAL_CLOSE) - 1;

    mapping = searchInsert_ctbl(
        NULL, terminal_tbl, term, LEN_CHUNK(graph->terminals) - 1, CTBL_MODE_INSERT_RESPECT
    );
    if (mapping->value != LEN_CHUNK(graph->terminals) - 1)
        deleteLast_chunk(graph->terminals);

    return mapping->value;
}

int construct_ggraph(
    GrammarGraph* const graph,
    FILE* const bnf_file,
    char* const root_str,
    uint32_t const root_len
) {
    Chunk lines[1]              = { NOT_A_CHUNK };
    ChunkTable rule_tbl[1]      = { NOT_A_CTBL };
    ChunkTable terminal_tbl[1]  = { NOT_A_CTBL };
    int construct_res           = GRAMMAR_OK;

    assert(graph != NULL);
    assert(bnf_file != NULL);

    constructEmpty_chunk(lines, CHUNK_RECOMMENDED_PARAMETERS);
    constructEmpty_ctbl(rule_tbl, CTBL_RECOMMENDED_PARAMETERS);
    constructEmpty_ctbl(terminal_tbl, CTBL_RECOMMENDED_PARAMETERS);

    addF_chunk(lines, bnf_file, BNF_MAX_SZ_FILE, BNF_MAX_SZ_FILE);
    cutByDelimLast_chunk(lines, "\n");
    fprintf_verbose(stderr, "# Lines in BNF = %"PRIu32, LEN_CHUNK(lines));

    constructEmpty_chunk(graph->rule_names, CHUNK_RECOMMENDED_PARAMETERS);
    constructEmpty_chunk(graph->terminals, CHUNK_RECOMMENDED_PARAMETERS);
    constructEmpty_alist(graph->rule_list, sizeof(RuleTerm), ALIST_RECOMMENDED_INITIAL_CAP);
    constructEmpty_alist(graph->exp_list, sizeof(ExpansionTerm), ALIST_RECOMMENDED_INITIAL_CAP);

    construct_res = load_ggraph(graph, rule_tbl, terminal_tbl, lines, root_str, root_len);

    if (construct_res != GRAMMAR_OK) {
        destruct_chunk(graph->rule_names);
        destruct_chunk(graph->terminals);
        destruct_alist(graph->rule_list);
        destruct_alist(graph->exp_list);
    }

    destruct_chunk(lines);
    destruct_ctbl(rule_tbl);
    destruct_ctbl(terminal_tbl);

    return construct_res;
}

void destruct_ggraph(GrammarGraph* const graph) {
    RuleTerm* rule = NULL;

    assert(isValid_ggraph(graph));

    destruct_chunk(graph->rule_names);
    destruct_chunk(graph->terminals);

    rule = getFirst_alist(graph->rule_list);
    REPEAT(graph->rule_list->len) {
        destruct_alist(rule->alt_list);
        rule++;
    }

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

void fillCovMtx_ggraph(
    BitMatrix* const cov_mtx,
    GrammarGraph const* const graph,
    uint32_t const rule_id
) {
    RuleTerm const* rule        = NULL;
    uint32_t const* p_exp_id    = NULL;
    ExpansionTerm const* exp    = NULL;

    assert(isValid_bmtx(cov_mtx));
    assert(isValid_ggraph(graph));
    assert(rule_id < graph->rule_list->len);

    rule        = get_alist(graph->rule_list, rule_id);
    p_exp_id    = getFirst_alist(rule->alt_list);
    for (uint32_t alt_id = 0; alt_id < rule->alt_list->len; alt_id++) {
        exp = get_alist(graph->exp_list, *(p_exp_id++));
        if (exp->cov_count == 0)
            unset_bmtx(cov_mtx, 0, alt_id);
        else
            set_bmtx(cov_mtx, 0, alt_id);

        while (exp->has_next) exp++;
    }
}

void generateSentence_ggraph(
    Chunk* const str_builder,
    GrammarGraph* const graph,
    ArrayList const* const seq
) {
    ArrayList stack[1]          = { NOT_AN_ALIST };
    Item terminal               = NOT_AN_ITEM;
    RuleTerm* rule              = NULL;
    uint32_t const* p_decision  = NULL;
    uint32_t const* p_exp_id    = NULL;
    ExpansionTerm* exp          = NULL;
    uint32_t n_exps             = 0;

    assert(isValid_chunk(str_builder));
    assert(isValid_ggraph(graph));
    assert(isValid_alist(seq));
    assert(seq->len > 0);

    p_decision  = getFirst_alist(seq);
    rule        = get_alist(graph->rule_list, graph->root_rule_id);
    if (rule->cov_count == 0)       graph->n_cov++;
    if (rule->cov_count < SZ32_MAX) rule->cov_count++;

    constructEmpty_alist(stack, sizeof(ExpansionTerm), ALIST_RECOMMENDED_INITIAL_CAP);
    addIndeterminate_chunk(str_builder, 0);

    p_exp_id    = get_alist(rule->alt_list, *(p_decision++));
    exp         = get_alist(graph->exp_list, *p_exp_id);
    n_exps      = 1;
    while ((exp++)->has_next) n_exps++;
    REPEAT(n_exps) push_alist(stack, --exp);

    do {
        exp = pop_alist(stack);

        if (exp->cov_count == 0)        graph->n_cov++;
        if (exp->cov_count < SZ32_MAX)  exp->cov_count++;

        if (exp->is_terminal) {
            terminal = get_chunk(graph->terminals, exp->rt_id);
            appendLast_chunk(str_builder, terminal.p, terminal.sz);
        } else {
            rule = get_alist(graph->rule_list, exp->rt_id);
            if (rule->cov_count == 0)           graph->n_cov++;
            if (rule->cov_count < SZ32_MAX)     rule->cov_count++;

            p_exp_id    = get_alist(rule->alt_list, *(p_decision++));
            exp         = get_alist(graph->exp_list, *p_exp_id);
            n_exps      = 1;
            while ((exp++)->has_next) n_exps++;
            REPEAT(n_exps) push_alist(stack, --exp);
        }
    } while (stack->len > 0);
}

bool isValid_ggraph(GrammarGraph const* const graph) {
    if (graph == NULL)                                                  return 0;
    if (!isValid_chunk(graph->rule_names))                              return 0;
    if (!isValid_chunk(graph->terminals))                               return 0;
    if (!isValid_alist(graph->rule_list))                               return 0;
    if (!isValid_alist(graph->exp_list))                                return 0;
    if (graph->root_rule_id >= graph->rule_list->len)                   return 0;
    if (graph->n_cov > graph->rule_list->len + graph->exp_list->len)    return 0;

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
    uint32_t rule_id    = 0;
    int load_res        = GRAMMAR_OK;

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

        rule_id = addRule(graph, rule_tbl, line_begin, line.sz, &i);
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
    assert(output != NULL);
    assert(isValid_ggraph(graph));

    fprintf(output,
        "digraph GrammarGraph {\n"
        "    edge [fontname=\"PT Mono\"];\n"
        "    node [fontname=\"PT Mono\"];\n"
        "    root [shape=\"none\",width=0,height=0,label=\"\"];\n"
        "\n"
    );
    for (uint32_t rule_id = 0; rule_id < graph->rule_list->len; rule_id++) {
        RuleTerm const* const rule  = get_alist(graph->rule_list, rule_id);
        Item const rule_name        = get_chunk(graph->rule_names, rule->name_id);

        fprintf(output,
            "    r%"PRIu32" [label=\"%.*s\"",
            rule_id, (int)rule_name.sz, (char*)rule_name.p
        );

        if (rule->cov_count == 0)
            fprintf(output, "];\n");
        else
            fprintf(output, ",style=\"filled\"];\n");
    }
    for (uint32_t rule_id = 0; rule_id < graph->rule_list->len; rule_id++) {
        RuleTerm const* const rule  = get_alist(graph->rule_list, rule_id);
        uint32_t const* p_exp_id    = getFirst_alist(rule->alt_list);

        for (uint32_t alt_id = 0; alt_id < rule->alt_list->len; alt_id++) {
            ExpansionTerm const* exp    = get_alist(graph->exp_list, *(p_exp_id++));
            uint32_t port_id            = 0;

            fprintf(output, "    r%"PRIu32"e%"PRIu32" [label=\"", rule_id, alt_id);
            do {
                fprintf(output, "<p%"PRIu32">", port_id);
                if (exp->is_terminal) {
                    Item const terminal = get_chunk(graph->terminals, exp->rt_id);
                    fprintf(
                        output,
                        BNF_STR_TERMINAL_OPEN"%.*s"BNF_STR_TERMINAL_CLOSE,
                        (int)terminal.sz, (char*)terminal.p
                    );
                } else {
                    RuleTerm const* const child = get_alist(graph->rule_list, exp->rt_id);
                    Item const child_name       = get_chunk(graph->rule_names, child->name_id);
                    fprintf(
                        output,
                        "\\%.*s\\"BNF_STR_RULE_CLOSE,
                        (int)(child_name.sz + 1 - sizeof(BNF_STR_RULE_CLOSE)), (char*)child_name.p
                    );
                }
                port_id++;
                if (exp->has_next) fprintf(output, "|");
            } while ((exp++)->has_next);
            fprintf(output, "\",shape=\"record\"");
            if (exp->cov_count == 0)
                fprintf(output, "];\n");
            else
                fprintf(output, ",style=\"filled\"];\n");
        }
    }
    for (uint32_t rule_id = 0; rule_id < graph->rule_list->len; rule_id++) {
        RuleTerm const* const rule  = get_alist(graph->rule_list, rule_id);
        uint32_t const* p_exp_id    = getFirst_alist(rule->alt_list);

        for (uint32_t alt_id = 0; alt_id < rule->alt_list->len; alt_id++) {
            ExpansionTerm const* exp    = get_alist(graph->exp_list, *(p_exp_id++));
            uint32_t port_id            = 0;

            fprintf(
                output,
                "    r%"PRIu32"->r%"PRIu32"e%"PRIu32":p0;\n",
                rule_id, rule_id, alt_id
            );
            do {
                uint32_t const child_id = exp->rt_id;
                if (exp->is_terminal) continue;
                fprintf(
                    output,
                    "    r%"PRIu32"e%"PRIu32":p%"PRIu32"->r%"PRIu32";\n",
                    rule_id, alt_id, port_id++, child_id
                );
            } while ((exp++)->has_next);
        }
    }

    fprintf(output, "}\n");
}

static int skipSpaces(
    char const* const line_begin,
    uint32_t const line_sz,
    uint32_t* const i
) {
    while (*i < line_sz && isspace((unsigned char)line_begin[*i])) (*i)++;
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


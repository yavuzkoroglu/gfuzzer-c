#ifndef GRAMMAR_GRAPH_H
    #define GRAMMAR_GRAPH_H
    #include "padkit/bitmatrix.h"
    #include "padkit/chunk.h"
    #include "padkit/indextable.h"

    #define NOT_A_GGRAPH ((GrammarGraph){   \
        { NOT_A_CHUNK }, { NOT_A_CHUNK },   \
        { NOT_AN_ALIST }, { NOT_AN_ALIST }, \
        0, 0                                \
    })

    /* "exp" is an abbreviation for "expansion". */
    typedef struct GrammarGraphBody {
        Chunk       rule_names[1];
        Chunk       terminals[1];
        ArrayList   rule_list[1];
        ArrayList   exp_list[1];
        uint32_t    root_rule_id;
        uint32_t    n_cov;
    } GrammarGraph;

    typedef struct RuleTermBody {
        uint32_t    name_id;
        uint32_t    cov_count;
        uint32_t    first_alt_id;
        uint32_t    n_alt;
    } RuleTerm;

    /* is_terminal == !is_rule */
    typedef struct ExpansionTermBody {
        uint32_t    rt_id:31;
        uint32_t    is_terminal:1;
        uint32_t    cov_count:31;
        uint32_t    has_next:1;
    } ExpansionTerm;

    #define GRAMMAR_OK              (0)
    #define GRAMMAR_SYNTAX_ERROR    (1)
    int construct_ggraph(
        GrammarGraph* const graph,
        FILE* const bnf_file,
        char* const root_str,
        uint32_t const root_len
    );

    void destruct_ggraph(GrammarGraph* const graph);

    void fillCovMtx_ggraph(
        BitMatrix* const cov_mtx,
        GrammarGraph const* const graph,
        uint32_t const rule_id
    );

    bool isValid_ggraph(GrammarGraph const* const graph);

    void generateSentence_ggraph(
        Chunk* const str_builder,
        GrammarGraph* const graph,
        ArrayList const* const decision_sequence
    );

    ExpansionTerm* getAltExp_ggraph(
        GrammarGraph* const graph,
        uint32_t const rule_id,
        uint32_t const jth_alt
    );

    void printDot_ggraph(
        FILE* const output,
        GrammarGraph const* const graph
    );

    uint32_t nTerms_ggraph(GrammarGraph const* const graph);

    uint32_t termCov_ggraph(GrammarGraph const* const graph);
#endif


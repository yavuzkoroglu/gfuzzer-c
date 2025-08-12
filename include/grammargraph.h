#ifndef GRAMMAR_GRAPH_H
    #define GRAMMAR_GRAPH_H
    #include "padkit/chunk.h"
    #include "padkit/indextable.h"

    #define NOT_A_GGRAPH ((GrammarGraph){   \
        { NOT_A_CHUNK }, { NOT_A_CHUNK },   \
        { NOT_AN_ALIST }, { NOT_AN_ALIST }, \
        0, 0                                \
    })

    typedef struct GrammarGraphBody {
        Chunk       rule_names[1];
        Chunk       terminals[1];
        ArrayList   rule_list[1];
        ArrayList   expansion_list[1];
        uint32_t    root_rule_id;
        uint32_t    n_terms_covered_once;
    } GrammarGraph;

    typedef struct RuleTermBody {
        uint32_t    name_id;
        uint32_t    cov_count;
        ArrayList   expansion_id_list[1];
    } RuleTerm;

    typedef struct ExpansionTermBody {
        uint32_t    is_terminal;
        uint32_t    id;
        uint32_t    cov_count;
        uint32_t    next_expansion_id;
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

    bool isValid_ggraph(GrammarGraph const* const graph);

    #define GRAMMAR_SENTENCE_OK     (0)
    #define GRAMMAR_SENTENCE_ERROR  (1)
    int generateSentence_ggraph(
        Chunk* const str_builder,
        GrammarGraph* const graph,
        ArrayList const* const decision_sequence
    );

    uint32_t nTerms_ggraph(GrammarGraph const* const graph);

    uint32_t termCov_ggraph(GrammarGraph const* const graph);
#endif


#ifndef GRAMMAR_GRAPH_H
    #define GRAMMAR_GRAPH_H
    #include "padkit/chunk.h"
    #include "padkit/indextable.h"

    #define NOT_A_GGRAPH ((ASTree)){ { NOT_A_CHUNK }, { NOT_AN_ALIST }, { NOT_AN_ALIST }, 0, 0, 0, 0 })

    typedef struct GrammarGraphBody {
        Chunk       str_chunk[1];
        ArrayList   rule_list[1];
        ArrayList   expansion_list[1];
        uint32_t    root_node_id;
        uint32_t    n_terms_not_covered;
        uint32_t    n_terms_covered_once;
        uint32_t    n_terms_total;
    } GrammarGraph;

    typedef struct RuleTermBody {
        uint32_t    str_id;
        uint32_t    cov_count;
        ArrayList   expansion_id_list[1];
    } RuleTerm;

    typedef struct ExpansionTermBody {
        bool        is_terminal;
        uint32_t    id;
        uint32_t    cov_count;
        uint32_t    next_expansion_id;
    } ExpansionTerm;

    #define GRAMMAR_OK              (0)
    #define GRAMMAR_SYNTAX_ERROR    (1)
    int construct_ggraph(
        GrammarGraph* const graph,
        FILE* const bnf_file,
        char const* const root_str,
        uint32_t const root_len
    );

    void destruct_ggraph(GrammarGraph* const graph);

    #define GRAMMAR_SENTENCE_OK     (0)
    #define GRAMMAR_SENTENCE_ERROR  (1)
    int generateSentence_ggraph(
        Chunk* const str_builder,
        GrammarGraph* const graph,
        ArrayList const* const decision_sequence
    );

    uint32_t getTermCov_ggraph(GrammarGraph const* const graph);
#endif


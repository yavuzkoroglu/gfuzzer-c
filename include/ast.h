#ifndef AST_H
    #define AST_H
    #include "padkit/chunk.h"
    #include "padkit/indextable.h"

    #define AST_MAX_LEN_TERM        (1024)
    #define AST_MAX_N_EXPANSIONS    (29)

    #define NOT_AN_ASTREE           ((ASTree){ { NOT_A_CHUNK }, { NOT_AN_ITBL }, 0, 0, 0, 0 })

    typedef struct ASTreeBody {
        Chunk       chunk[1];
        IndexTable  chunktbl[1];
        uint32_t    root_id;
        uint32_t    n_terms_not_covered;
        uint32_t    n_terms_covered_once;
        uint32_t    n_terms_total;
    } ASTree;

    typedef struct ASTreeExpansionBody {
        uint32_t    cov_count;
        uint32_t    str_id;
        uint32_t    node_id;
        uint32_t    next_id;
    } ASTreeExpansion;

    typedef struct ASTreeNodeBody {
        uint32_t    cov_count;
        uint32_t    str_id;
        uint32_t    n_expansions;
        uint32_t    expansion_ids[AST_MAX_N_EXPANSIONS];
    } ASTreeNode;

    #define AST_OK                  (0)
    #define AST_SYNTAX_ERROR        (1)
    int constructFromBNF_ast(
        ASTree* const ast,
        FILE* const bnf_file,
        char const* const root_str,
        uint32_t const root_len
    );

    void destruct_ast(ASTree* const ast);

    Item expandRandom_ast(
        Chunk* const str_builder,
        IndexTable* const str_builder_tbl,
        ASTree* const ast,
        uint32_t const parent_id,
        bool const is_cov_guided,
        bool const unique
    );

    ASTreeNode* getNode_ast(
        ASTree const* const ast,
        uint32_t const node_id
    );

    uint32_t getTermCov_ast(ASTree const* const ast);

    bool isAllocated_ast(ASTree const* const ast);

    bool isTerminal_aste(
        ASTree const* const ast,
        ASTreeExpansion const* const expansion
    );

    bool isValid_ast(ASTree const* const ast);

    bool isValid_aste(
        ASTree const* const ast,
        ASTreeExpansion const* const expansion
    );

    bool isValid_astn(
        ASTree const* const ast,
        ASTreeNode const* const node
    );

    ASTreeExpansion* next_aste(
        ASTree const* const ast,
        ASTreeExpansion const* const expansion
    );
#endif

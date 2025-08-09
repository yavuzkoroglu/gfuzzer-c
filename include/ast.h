#ifndef AST_H
    #define AST_H
    #include "padkit/chunk.h"
    #include "padkit/indextable.h"

    #define AST_MAX_LEN_TOKEN   (1024)

    #define NOT_AN_ASTREENODE   ((ASTreeNode){ 0, 0, { NOT_AN_ALIST } })

    #define NOT_AN_ASTREE       ((ASTree){ { NOT_A_CHUNK }, { NOT_AN_ITBL }, { NOT_AN_ALIST }, 0, 0 })

    typedef struct ASTreeNodeBody {
        uint32_t    str_id;
        uint32_t    cov_count;
        ArrayList   children[1];
    } ASTreeNode;

    typedef struct ASTreeBody {
        Chunk       chunk[1];
        IndexTable  chunktbl[1];
        uint32_t    root_id;
        uint32_t    n_zero_covered;
    } ASTree;

    uint_fast64_t addToken_ast(
        ASTree* const ast,
        char const* const str,
        uint32_t const len
    );

    void constructFromBNF_ast(
        ASTree* const ast,
        FILE* const bnf_file,
        char const* const root_str,
        uint32_t const root_len
    );

    void destruct_ast(ASTree* const ast);

    void expandRandom_ast(
        ArrayList* const expanded_indices,
        ASTree const* const ast,
        uint32_t const parent_id,
        bool const is_cov_guided
    );

    bool isAllTerminal_ast(
        ASTree const* const ast,
        ArrayList const* const expanded_indices
    );

    bool isTerminal_ast(
        ASTree const* const ast,
        uint32_t const node_id
    );

    bool isValid_ast(ASTree const* const ast);
#endif

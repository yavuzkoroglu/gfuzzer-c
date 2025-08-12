#ifndef DECISION_TREE_H
    #define DECISION_TREE_H
    #include "grammargraph.h"

    typedef struct DecisionTreeBody {
        ArrayList   nodes[1];
    } DecisionTree;

    typedef struct DecisionTreeNodeBody {
        uint32_t    is_fully_explored;
        uint32_t    local_expansion_id;
        uint32_t    sibling_id;
        uint32_t    child_id;
    } DecisionTreeNode;

    void constructEmpty_dtree(DecisionTree* const dtree);

    void destruct_dtree(DecisionTree* const tree);

    void generateRandomDecisionSequence_dtree(
        ArrayList* const sequence,
        DecisionTree* const dtree,
        GrammarGraph* const graph,
        bool const is_cov_guided,
        bool const unique
    );

    bool isValid_dtree(DecisionTree const* const dtree);
#endif

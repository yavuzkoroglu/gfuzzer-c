#ifndef DECISION_TREE_H
    #define DECISION_TREE_H
    #include "grammargraph.h"

    typedef struct DecisionTreeBody {
        ArrayList   nodes[1];
    } DecisionTree;

    typedef struct DecisionTreeNodeBody {
        uint32_t    is_fully_explored;
        uint32_t    n_choices;
        uint32_t    parent_id;
        uint32_t    first_choice_id;
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

#ifndef DECISION_TREE_H
    #define DECISION_TREE_H
    #include "grammargraph.h"

    typedef struct DecisionTreeBody {
        ArrayList   node_list[1];
    } DecisionTree;

    #define DTREE_NODE_STATE_UNEXPLORED             (0)
    #define DTREE_NODE_STATE_PARTIALLY_EXPLORED     (1)
    #define DTREE_NODE_STATE_FULLY_EXPLORED         (2)
    typedef struct DecisionTreeNodeBody {
        uint32_t    state;
        uint32_t    n_choices;
        uint32_t    parent_id;
        uint32_t    first_child_id;
    } DecisionTreeNode;

    DecisionTreeNode* addUnexploredNode_dtree(
        DecisionTree* const dtree,
        uint32_t const parent_id
    );

    void constructEmpty_dtree(DecisionTree* const dtree);

    void destruct_dtree(DecisionTree* const tree);

    #define DTREE_GENERATE_OK                       (0)
    #define DTREE_GENERATE_NO_UNIQUE_SEQ_REMAINING  (1)
    int generateRandomDecisionSequence_dtree(
        ArrayList* const sequence,
        DecisionTree* const dtree,
        GrammarGraph* const graph,
        bool const is_cov_guided,
        bool const unique
    );

    bool isValid_dtree(DecisionTree const* const dtree);
#endif

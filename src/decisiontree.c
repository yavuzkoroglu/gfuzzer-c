#include <assert.h>
#include <stdlib.h>
#include "decisiontree.h"
#include "padkit/bitmatrix.h"
#include "padkit/invalid.h"
#include "padkit/repeat.h"
#include "padkit/unused.h"

DecisionTreeNode* addUnexploredNode_dtree(
    DecisionTree* const dtree,
    uint32_t const parent_id
) {
    assert(dtree != NULL);
    assert(isValid_alist(dtree->node_list));
    {
        DecisionTreeNode* const node    = addIndeterminate_alist(dtree->node_list);
        node->state                     = DTREE_NODE_STATE_UNEXPLORED;
        node->n_choices                 = INVALID_UINT32;
        node->parent_id                 = parent_id;
        node->first_child_id            = INVALID_UINT32;
        return node;
    }
}

void constructEmpty_dtree(DecisionTree* const dtree) {
    assert(dtree != NULL);
    constructEmpty_alist(dtree->node_list, sizeof(DecisionTreeNode), ALIST_RECOMMENDED_INITIAL_CAP);
    addUnexploredNode_dtree(dtree, INVALID_UINT32);
}

void destruct_dtree(DecisionTree* const dtree) {
    assert(isValid_dtree(dtree));
    destruct_alist(dtree->node_list);
}

int generateRandomDecisionSequence_dtree(
    ArrayList* const seq,
    DecisionTree* const dtree,
    GrammarGraph* const graph,
    uint32_t const min_depth,
    bool const is_cov_guided,
    bool const unique
) {
    MAYBE_UNUSED(min_depth)
    MAYBE_UNUSED(unique)
    assert(isValid_alist(seq));
    assert(seq->sz_elem == sizeof(uint32_t));
    assert(isValid_dtree(dtree));
    assert(isValid_ggraph(graph));
    {
        BitMatrix cov_mtx[1]    = { NOT_A_BMATRIX };
        uint32_t node_id        = 0;
        uint32_t rule_id        = graph->root_rule_id;
        uint32_t decision       = INVALID_UINT32;
        RuleTerm* rule          = get_alist(graph->rule_list, rule_id);
        DecisionTreeNode* node  = get_alist(dtree->node_list, node_id);
        ExpansionTerm* exp;

        if (is_cov_guided) {
            construct_bmtx(cov_mtx, 1, 64);
            fillCovMtx_ggraph(cov_mtx, graph, rule_id);
        }

        switch (node->state) {
            case DTREE_NODE_STATE_FULLY_EXPLORED:
                if (unique) return DTREE_GENERATE_NO_UNIQUE_SEQ_REMAINING;
                break;
            case DTREE_NODE_STATE_UNEXPLORED:
                node->state     = DTREE_NODE_STATE_PARTIALLY_EXPLORED;
                node->n_choices = rule->n_alt;
                break;
            case DTREE_NODE_STATE_PARTIALLY_EXPLORED:
            default:
                /* Do Nothing */;
        }

        if (!is_cov_guided) {
            decision = (uint32_t)rand() % node->n_choices;
        } else {
            uint32_t n_not_covered = 0;
            for (uint32_t i = 0; i < node->n_choices; i++)
                n_not_covered += !get_bmtx(cov_mtx, 0, i);

            if (n_not_covered == 0) {
                decision = (uint32_t)rand() % node->n_choices;
            } else {
                uint32_t const new_choice = (uint32_t)rand() % n_not_covered;
                decision = findInRow_bmtx(cov_mtx, 0, node->n_choices - 1, 0);
                REPEAT(new_choice)
                    decision = findInRow_bmtx(cov_mtx, 0, decision - 1, 0);
            }
        }
        add_alist(seq, &decision);

        exp = getAltExp_ggraph(graph, rule_id, decision);
        if (exp->is_terminal) {
            
        } else {
        }

        if (is_cov_guided)
            destruct_bmtx(cov_mtx);
    }

    return DTREE_GENERATE_OK;
}

bool isValid_dtree(DecisionTree const* const dtree) {
    if (dtree == NULL)                                          return 0;
    if (!isValid_alist(dtree->node_list))                       return 0;
    if (dtree->node_list->len == 0)                             return 0;
    if (dtree->node_list->sz_elem != sizeof(DecisionTreeNode))  return 0;

    return 1;
}

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include "bnf.h"
#include "decisiontree.h"
#include "padkit/bitmatrix.h"
#include "padkit/invalid.h"
#include "padkit/repeat.h"
#include "padkit/size.h"

void addUnexploredNodes_dtree(
    DecisionTree* const dtree,
    uint32_t const parent_id,
    uint32_t const n
) {
    DecisionTreeNode* node = NULL;

    assert(dtree != NULL);
    assert(isValid_alist(dtree->node_list));
    assert(n < SZ32_MAX);
    REPEAT(n) {
        node                    = addIndeterminate_alist(dtree->node_list);
        node->state             = DTREE_NODE_STATE_UNEXPLORED;
        node->n_choices         = INVALID_UINT32;
        node->parent_id         = parent_id;
        node->first_child_id    = INVALID_UINT32;
    }
}

void constructEmpty_dtree(DecisionTree* const dtree) {
    assert(dtree != NULL);
    constructEmpty_alist(dtree->node_list, sizeof(DecisionTreeNode), ALIST_RECOMMENDED_INITIAL_CAP);
    addUnexploredNodes_dtree(dtree, INVALID_UINT32, 1);
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
    bool const cov_guided,
    bool const unique
) {
    ArrayList stack[1]          = { NOT_AN_ALIST };
    uint32_t node_id            = 0;
    uint32_t n_exps             = 0;
    uint32_t rule_id            = INVALID_UINT32;
    uint32_t decision           = INVALID_UINT32;
    uint32_t exp_id             = INVALID_UINT32;
    RuleTerm const* rule        = NULL;
    ExpansionTerm const* exp    = NULL;

    assert(isValid_alist(seq));
    assert(seq->len == 0);
    assert(seq->sz_elem == sizeof(uint32_t));
    assert(isValid_dtree(dtree));
    assert(isValid_ggraph(graph));

    if (unique) {
        DecisionTreeNode const* const root_node = get_alist(dtree->node_list, node_id);
        if (root_node->state == DTREE_NODE_STATE_FULLY_EXPLORED)
            return DTREE_GENERATE_NO_UNIQUE_SEQ_REMAINING;
    }

    constructEmpty_alist(stack, sizeof(uint32_t), ALIST_RECOMMENDED_INITIAL_CAP);
    push_alist(stack, &(graph->root_rule_id));
    do {
        rule_id     = *(uint32_t*)pop_alist(stack);
        decision    = partiallyExploreNode_dtree(
            seq, &node_id,
            dtree, graph, rule_id,
            cov_guided, unique
        );
        rule        = get_alist(graph->rule_list, rule_id);
        exp_id      = *(uint32_t*)get_alist(rule->alt_list, decision);
        exp         = get_alist(graph->exp_list, exp_id);
        n_exps      = 1;
        while (exp->has_next) { exp++; n_exps++; }
        REPEAT(n_exps) {
            if (!exp->is_terminal) {
                uint32_t const child_rule_id = exp->rt_id;
                push_alist(stack, &(child_rule_id));
            }
            exp--;
        }
    } while (stack->len > 0);
    destruct_alist(stack);

    setLeaf_dtree(dtree, node_id);

    if (seq->len < min_depth)
        return DTREE_GENERATE_SHALLOW_SEQ;
    else
        return DTREE_GENERATE_OK;
}

bool isAllChildrenFullyExplored_dtree(
    DecisionTree const* const dtree,
    uint32_t const parent_id
) {
    assert(isValid_dtree(dtree));
    assert(parent_id < dtree->node_list->len);
    {
        DecisionTreeNode const* const parent = get_alist(dtree->node_list, parent_id);
        assert(parent->state != DTREE_NODE_STATE_UNEXPLORED);
        assert(parent->n_choices > 0);
        if (parent->state == DTREE_NODE_STATE_FULLY_EXPLORED) {
            return 1;
        } else {
            DecisionTreeNode const* child = get_alist(dtree->node_list, parent->first_child_id);
            REPEAT(parent->n_choices) if ((child++)->state != DTREE_NODE_STATE_FULLY_EXPLORED) return 0;
            return 1;
        }
    }
}

bool isValid_dtree(DecisionTree const* const dtree) {
    if (dtree == NULL)                                          return 0;
    if (!isValid_alist(dtree->node_list))                       return 0;
    if (dtree->node_list->len == 0)                             return 0;
    if (dtree->node_list->sz_elem != sizeof(DecisionTreeNode))  return 0;

    return 1;
}

uint32_t partiallyExploreNode_dtree(
    ArrayList* const seq, uint32_t* const p_node_id,
    DecisionTree* const dtree, GrammarGraph const* const graph, uint32_t const rule_id,
    bool const cov_guided, bool const unique
) {
    BitMatrix cov_mtx[1]        = { NOT_A_BMATRIX };
    ArrayList decision_list[1]  = { NOT_AN_ALIST };
    DecisionTreeNode* node      = NULL;
    DecisionTreeNode* child     = NULL;
    RuleTerm const* rule        = NULL;
    uint32_t choice             = 0;
    bool all_covered_once       = 1;
    uint32_t decision           = 0;

    assert(isValid_alist(seq));
    assert(seq->sz_elem == sizeof(uint32_t));
    assert(p_node_id != NULL);
    assert(isValid_dtree(dtree));
    assert(*p_node_id < dtree->node_list->len);
    assert(isValid_ggraph(graph));
    assert(rule_id < graph->rule_list->len);

    rule = get_alist(graph->rule_list, rule_id);
    node = get_alist(dtree->node_list, *p_node_id);
    assert(node->state != DTREE_NODE_STATE_FULLY_EXPLORED);
    if (node->state == DTREE_NODE_STATE_UNEXPLORED) {
        node->state             = DTREE_NODE_STATE_PARTIALLY_EXPLORED;
        node->n_choices         = rule->alt_list->len;
        node->first_child_id    = dtree->node_list->len;
        assert(node->n_choices > 0);
        addUnexploredNodes_dtree(dtree, *p_node_id, node->n_choices);
    }

    if (cov_guided) construct_bmtx(cov_mtx, 1, node->n_choices);
    constructEmpty_alist(decision_list, sizeof(uint32_t), node->n_choices + 1);

    if (cov_guided) {
        fillCovMtx_ggraph(cov_mtx, graph, *p_node_id);
        for (choice = 0; choice < node->n_choices && get_bmtx(cov_mtx, 0, choice); choice++) {}
        all_covered_once = (choice >= node->n_choices);
    }

    child = get_alist(dtree->node_list, node->first_child_id);
    for (choice = 0; choice < node->n_choices; choice++, child++) {
        if (unique && child->state == DTREE_NODE_STATE_FULLY_EXPLORED)          continue;
        if (cov_guided && !all_covered_once && get_bmtx(cov_mtx, 0, choice))    continue;

        add_alist(decision_list, &choice);
    }

    if (cov_guided) destruct_bmtx(cov_mtx);

    assert(decision_list->len > 0);
    decision    = *(uint32_t*)get_alist(decision_list, (uint32_t)rand() % decision_list->len);
    *p_node_id  = node->first_child_id + decision;
    add_alist(seq, &decision);

    destruct_alist(decision_list);

    return decision;
}

struct IdPair {
    uint32_t node_id;
    uint32_t rule_id;
};
void printDot_dtree(
    FILE* const output,
    DecisionTree const* const dtree,
    GrammarGraph const* const graph
) {
    DecisionTreeNode const* node    = NULL;
    RuleTerm const* rule            = NULL;
    RuleTerm const* child_rule      = NULL;
    ExpansionTerm const* exp        = NULL;
    struct IdPair* id_pair          = NULL;
    struct IdPair* child_id_pair    = NULL;
    ArrayList stack[1]              = { NOT_AN_ALIST };
    Item term                       = NOT_AN_ITEM;
    uint32_t child_id               = INVALID_UINT32;
    uint32_t exp_id                 = INVALID_UINT32;
    uint32_t n_exps                 = 0;

    assert(output != NULL);
    assert(isValid_dtree(dtree));
    assert(isValid_ggraph(graph));

    fprintf(output,
        "digraph GrammarGraph {\n"
        "    edge [fontname=\"PT Mono\"];\n"
        "    node [fontname=\"PT Mono\",label=\"\",shape=\"circle\"];\n"
        "    root [shape=\"none\",width=0,height=0,label=\"\"];\n"
        "\n"
    );
    node = getFirst_alist(dtree->node_list);
    for (uint32_t node_id = 0; node_id < dtree->node_list->len; node++, node_id++) {
        switch (node->state) {
            case DTREE_NODE_STATE_PARTIALLY_EXPLORED:
                fprintf(output, "    n%"PRIu32";\n", node_id);
                break;
            case DTREE_NODE_STATE_FULLY_EXPLORED:
                fprintf(output, "    n%"PRIu32" [peripheries=2];\n", node_id);
                break;
            case DTREE_NODE_STATE_UNEXPLORED:
            default:
                fprintf(output, "    n%"PRIu32" [label=\"??\",shape=\"none\",height=0];\n", node_id);
        }
    }

    constructEmpty_alist(stack, sizeof(struct IdPair), ALIST_RECOMMENDED_INITIAL_CAP);
    id_pair             = pushIndeterminate_alist(stack);
    id_pair->node_id    = 0;
    id_pair->rule_id    = graph->root_rule_id;
    do {
        id_pair = pop_alist(stack);

        node = get_alist(dtree->node_list, id_pair->node_id);
        if (node->n_choices == 0) continue;
        rule = get_alist(graph->rule_list, id_pair->rule_id);

        for (uint32_t choice = 0; choice < node->n_choices; choice++) {
            child_id    = node->first_child_id + choice;
            exp_id      = *(uint32_t*)get_alist(rule->alt_list, choice);
            exp         = get_alist(graph->exp_list, exp_id);
            n_exps      = 1;
            while (exp->has_next) { exp++; n_exps++; }

            fprintf(output, "    n%"PRIu32"->n%"PRIu32" [label=\"", id_pair->node_id, child_id);
            REPEAT(n_exps) {
                child_id_pair = pushIndeterminate_alist(stack);
                child_id_pair->node_id = child_id;
                child_id_pair->rule_id = exp->rt_id;
                if (exp->is_terminal) {
                    term = get_chunk(graph->terminals, exp->rt_id);
                    fprintf(
                        output, BNF_STR_TERMINAL_OPEN"%.*s"BNF_STR_TERMINAL_CLOSE,
                        (int)term.sz, (char*)term.p
                    );
                } else {
                    child_rule  = get_alist(graph->rule_list, exp->rt_id);
                    term        = get_chunk(graph->rule_names, child_rule->name_id);
                    fprintf(output, "%.*s", (int)term.sz, (char*)term.p);
                }
                exp--;
            }
            fprintf(output, "\"];\n");
        }
    } while (stack->len > 0);
    destruct_alist(stack);

    fprintf(output, "}\n");
}

void propagateUpState_dtree(
    DecisionTree* const dtree,
    uint32_t const node_id
) {
    DecisionTreeNode* node = NULL;

    assert(isValid_dtree(dtree));
    assert(node_id < dtree->node_list->len);

    node = get_alist(dtree->node_list, node_id);
    if (node->state != DTREE_NODE_STATE_FULLY_EXPLORED) return;
    while (node->parent_id < dtree->node_list->len) {
        if (!isAllChildrenFullyExplored_dtree(dtree, node->parent_id)) return;
        node        = get_alist(dtree->node_list, node->parent_id);
        node->state = DTREE_NODE_STATE_FULLY_EXPLORED;
    }
}

void setLeaf_dtree(
    DecisionTree* const dtree,
    uint32_t const leaf_id
) {
    DecisionTreeNode* leaf = NULL;

    assert(isValid_dtree(dtree));
    assert(leaf_id < dtree->node_list->len);

    leaf = get_alist(dtree->node_list, leaf_id);
    assert(leaf->state == DTREE_NODE_STATE_UNEXPLORED);
    leaf->state     = DTREE_NODE_STATE_FULLY_EXPLORED;
    leaf->n_choices = 0;
    propagateUpState_dtree(dtree, leaf_id);
}

#include <assert.h>
#include "decisiontree.h"

void constructEmpty_dtree(DecisionTree* const dtree) {
    assert(dtree != NULL);
    constructEmpty_alist(dtree->nodes, sizeof(DecisionTreeNode), ALIST_RECOMMENDED_INITIAL_CAP);
}

void destruct_dtree(DecisionTree* const dtree) {
    assert(isValid_dtree(dtree));
    destruct_alist(dtree->nodes);
}

void generateRandomDecisionSequence_dtree(
    ArrayList* const sequence,
    DecisionTree* const dtree,
    GrammarGraph* const graph,
    bool const is_cov_guided,
    bool const unique
) {
    assert(isValid_alist(sequence));
    assert(sequence->sz_elem == sizeof(uint32_t));
    assert(isValid_dtree(dtree));
    assert(isValid_ggraph(graph));

    
}

bool isValid_dtree(DecisionTree const* const dtree) {
    if (dtree == NULL)                                      return 0;
    if (!isValid_alist(dtree->nodes))                       return 0;
    if (dtree->nodes->sz_elem != sizeof(DecisionTreeNode))  return 0;

    return 1;
}

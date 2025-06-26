#include <cassert>
#include <iostream>
#include "../raft/raft_node.h"

int main() {
    // 1) Fresh node should grant a vote for term 1 from candidate 2
    RaftNode node(1, {2,3,4});
    bool granted = node.handleRequestVote(1, 2, 0, 0);
    assert(granted && "Expected vote granted for candidate 2 in term 1");

    // 2) Same term, another candidate: should be denied (already voted this term)
    granted = node.handleRequestVote(1, 3, 0, 0);
    assert(!granted && "Expected vote denied for candidate 3 in term 1");

    // 3) Stale term (0 < currentTerm=1): should be denied
    granted = node.handleRequestVote(0, 2, 0, 0);
    assert(!granted && "Expected vote denied for stale term");

    // 4) Higher term should reset state and grant vote for the new candidate
    granted = node.handleRequestVote(2, 3, 0, 0);
    assert(granted && "Expected vote granted for candidate 3 in term 2");

    std::cout << "All RaftNode::handleRequestVote tests passed." << std::endl;
    return 0;
}

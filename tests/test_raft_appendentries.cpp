#include <cassert>
#include <iostream>
#include <vector>
#include "raft_node.h"

int main() {
    // 1) Simple heartbeat (no entries) in new term should succeed
    {
        RaftNode node(1, {2,3});
        bool ok = node.handleAppendEntries(
            /*term=*/1,
            /*leaderId=*/2,
            /*prevLogIndex=*/0,
            /*prevLogTerm=*/0,
            /*entries=*/{},
            /*leaderCommit=*/0
        );
        assert(ok && "Heartbeat should succeed on empty log");
    }

    // 2) Reject stale term
    {
        RaftNode node(1, {2,3});
        // first bring term to 1
        node.handleRequestVote(1, 2, 0, 0);
        bool ok = node.handleAppendEntries(
            /*term=*/0,
            /*leaderId=*/2,
            /*prevLogIndex=*/0,
            /*prevLogTerm=*/0,
            /*entries=*/{},
            /*leaderCommit=*/0
        );
        assert(!ok && "Should reject AppendEntries with stale term");
    }

    // 3) Reject if prevLogIndex/Term don’t match
    {
        RaftNode node(1, {2,3});
        // term 1 heartbeat to set term
        node.handleAppendEntries(1, 2, 0, 0, {}, 0);
        // now log.size()==1 (dummy)
        bool ok = node.handleAppendEntries(
            /*term=*/1,
            /*leaderId=*/2,
            /*prevLogIndex=*/1,  // out of bounds
            /*prevLogTerm=*/0,
            /*entries=*/{},
            /*leaderCommit=*/0
        );
        assert(!ok && "Should reject if prevLogIndex >= log.size()");
    }

    // 4) Append a new entry and update commitIndex
    {
        RaftNode node(1, {2,3});
        // push entry {term=1,index=1,cmd="x=5"}
        std::vector<LogEntry> ent = {{1,1,"x=5"}};
        bool ok = node.handleAppendEntries(
            /*term=*/1,
            /*leaderId=*/2,
            /*prevLogIndex=*/0,
            /*prevLogTerm=*/0,
            ent,
            /*leaderCommit=*/1
        );
        assert(ok && "Should append new entries");

        // Now the log has two entries: [dummy, x=5], and commitIndex==1.
        // A heartbeat with leaderCommit=2 should succeed and bring commitIndex up to 2.
        ok = node.handleAppendEntries(
            /*term=*/1,
            /*leaderId=*/2,
            /*prevLogIndex=*/1,
            /*prevLogTerm=*/1,
            /*entries=*/{},
            /*leaderCommit=*/2
        );
        assert(ok && "Heartbeat should succeed after log append");
        std::cout << "RaftNode::handleAppendEntries tests passed\n";
    }

    return 0;
}

// raft/raft_node.h
#ifndef RAFT_NODE_H
#define RAFT_NODE_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <string>

using namespace std;
using namespace std::chrono;

// Raft consensus states
enum class RaftState {
    Follower,
    Candidate,
    Leader
};

// Log entry structure for Raft
struct LogEntry {
    int64_t term;        // term when entry was received by leader
    int64_t index;       // position in the log (1-based)
    string command;      // client command (e.g., "PUT key value")
};

class RaftNode {
public:
    // Constructor takes this node's ID and list of peer IDs
    RaftNode(int nodeId, const vector<int>& peerIds);

    // Start the Raft node: begins election timeout
    void start();

    // Shutdown the Raft node gracefully
    void shutdown();

    // RPC handlers
    bool handleRequestVote(int candidateTerm, int candidateId,
                           int lastLogIndex, int lastLogTerm);

    bool handleAppendEntries(int leaderTerm, int leaderId,
                             int prevLogIndex, int prevLogTerm,
                             const vector<LogEntry>& entries,
                             int leaderCommit);

private:
    // Raft state and identifiers
    int nodeId;                      // this node's unique ID
    vector<int> peers;               // IDs of peer nodes
    RaftState state;                 // current state (Follower/Candidate/Leader)
    int64_t currentTerm;             // latest term server has seen
    int votedFor;                    // candidateId that received vote in current term
    vector<LogEntry> log;            // log entries
    int64_t commitIndex;             // index of highest log entry known to be committed
    int64_t lastApplied;             // index of highest log entry applied to state machine

    // Concurrency primitives
    mutex mtx;
    condition_variable cv;

    // Election and heartbeat timers
    milliseconds electionTimeout;
    milliseconds heartbeatInterval;

    // Internal methods
    void runElectionTimer();
    void runHeartbeatTimer();
    void becomeFollower(int term);
    void becomeCandidate();
    void becomeLeader();
    void resetElectionTimeout();

    // Helpers for RPCs
    bool isLogUpToDate(int lastLogIndex, int lastLogTerm);
    void replicateLogEntries();
};

#endif // RAFT_NODE_H

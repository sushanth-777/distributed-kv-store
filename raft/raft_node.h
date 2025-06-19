#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>

// Raft states
enum class RaftState {Follower, Candidate, Leader};

// A log entry: term and command
struct LogEntry {
    int term;
    int index;
    std::string command;
};

class RaftNode {
public:
    RaftNode(int id, const std::vector<int>& peerIds);
    ~RaftNode();

    // Start the node's main loop (election timer, heartbeats)
    void start();
    void shutdown();

    // RPC handlers called by the network layer
    bool handleRequestVote(int candidateTerm, int candidateId,
                           int lastLogIndex, int lastLogTerm);

    bool handleAppendEntries(int leaderTerm, int leaderId,
                             int prevLogIndex, int prevLogTerm,
                             const std::vector<LogEntry>& entries,
                             int leaderCommit);

private:
    void runElectionTimer();
    void sendHeartbeats();
    void becomeLeader();
    void becomeFollower(int term);
    void replicateLog();

    int nodeId;
    std::vector<int> peers;
    RaftState state;

    std::vector<LogEntry> log;
    int currentTerm;
    int votedFor;
    int commitIndex;
    int lastApplied;

    // Election timeout and heartbeat interval
    std::chrono::milliseconds electionTimeout;
    std::chrono::milliseconds heartbeatInterval;

    std::mutex mtx;
    std::condition_variable cv;
    bool running;
};
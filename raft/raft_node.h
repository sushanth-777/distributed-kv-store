#pragma once
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <string>

// Raft states
enum class RaftState {Follower, Candidate, Leader};

// A log entry: term, index, and command
struct LogEntry {
    int term;
    int index;
    std::string command;
};

class RaftNode {
public:
    using RequestVoteFn = std::function<bool(int peerId,
                                             int candidateTerm,
                                             int candidateId,
                                             int lastLogIndex,
                                             int lastLogTerm)>;

    using AppendEntriesFn = std::function<bool(int peerId,
                                               int term,
                                               int leaderId,
                                               int prevLogIndex,
                                               int prevLogTerm,
                                               const std::vector<LogEntry>& entries,
                                               int leaderCommit)>;

    using ApplyFn = std::function<void(const LogEntry& entry)>;

    RaftNode(int id, const std::vector<int>& peerIds);
    ~RaftNode();

    // Start/stop node threads
    void start();
    void shutdown();

    // Incoming RPC handlers
    bool handleRequestVote(int candidateTerm,
                           int candidateId,
                           int lastLogIndex,
                           int lastLogTerm);

    bool handleAppendEntries(int leaderTerm,
                             int leaderId,
                             int prevLogIndex,
                             int prevLogTerm,
                             const std::vector<LogEntry>& entries,
                             int leaderCommit);

    // Configure outgoing RPC callbacks
    void setRequestVoteCallback(RequestVoteFn fn);
    void setAppendEntriesCallback(AppendEntriesFn fn);

    // Configure the state‐machine apply callback
    void setApplyCallback(ApplyFn fn);

    // Check whether this node believes itself to be the leader
    bool isLeader() const {
        std::lock_guard<std::mutex> lock(mtx);
        return state == RaftState::Leader;
    }

    // Read current term
    int getCurrentTerm() const {
        std::lock_guard<std::mutex> lock(mtx);
        return currentTerm;
    }

    // Client‐side: propose & wait for a command to replicate+commit
    bool replicateCommand(const std::string& command, int timeout_ms = 1000);

private:
    // Timer, election, replication internals
    void runElectionTimer();
    void startElection();
    void sendHeartbeats();
    void becomeLeader();
    void becomeFollower(int term);
    void replicateLog();

    // Node identity & cluster
    int nodeId;
    std::vector<int> peers;
    RaftState state;

    // Persistent log & indices
    std::vector<LogEntry> log;
    int currentTerm;
    int votedFor;
    int commitIndex;
    int lastApplied;

    // RPC callbacks
    RequestVoteFn    requestVoteRpc;
    AppendEntriesFn appendEntriesRpc;
    ApplyFn          applyCb;

    // For waiting on commits
    std::condition_variable commitCv;

    // Timing parameters
    std::chrono::milliseconds electionTimeout;
    std::chrono::milliseconds heartbeatInterval;

    // Synchronization
    mutable std::mutex mtx;
    std::condition_variable cv;
    bool running;
};

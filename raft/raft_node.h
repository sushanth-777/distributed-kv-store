#pragma once
#include <vector>
#include <functional>
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
    using RequestVoteFn =
      std::function<bool(int peerId,
                         int candidateTerm,
                         int candidateId,
                         int lastLogIndex,
                         int lastLogTerm)>;

    using AppendEntriesFn =
    std::function<bool(int peerId,
                       int term,
                       int leaderId,
                       int prevLogIndex,
                       int prevLogTerm,
                       const std::vector<LogEntry>& entries,
                       int leaderCommit)>;
    
    


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

    // let the caller provide a function that actually sends a
    // RequestVote RPC to peerId
    void setRequestVoteCallback(RequestVoteFn fn);

    // Let caller provide the RPC stub for AppendEntries
    void setAppendEntriesCallback(AppendEntriesFn fn) {
      std::lock_guard<std::mutex> lock(mtx);
      appendEntriesRpc = std::move(fn);
    }

    // Thread-safe accessor for currentTerm
    int getCurrentTerm() const {
        std::lock_guard<std::mutex> lock(mtx);
        return currentTerm;
    }

private:
    mutable std::mutex mtx;
    void runElectionTimer();
    void sendHeartbeats();
    void becomeLeader();
    void becomeFollower(int term);
    void replicateLog();
    // Election logic
    void startElection();
    int nodeId;
    std::vector<int> peers;
    RaftState state;

    std::vector<LogEntry> log;
    int currentTerm;
    int votedFor;
    int commitIndex;
    int lastApplied;

    // holds the user‐supplied RPC stub
    RequestVoteFn requestVoteRpc;

    // add a member to hold the callback:
    AppendEntriesFn appendEntriesRpc;

    // Election timeout and heartbeat interval
    std::chrono::milliseconds electionTimeout;
    std::chrono::milliseconds heartbeatInterval;

    // std::mutex mtx;
    std::condition_variable cv;
    bool running;
};
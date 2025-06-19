#include "raft_node.h"
#include <thread>
#include <random>

RaftNode::RaftNode(int id, const std::vector<int>& peerIds)
    : nodeId(id), peers(peerIds), state(RaftState::Follower),
      currentTerm(0), votedFor(-1), commitIndex(0), lastApplied(0),
      electionTimeout(150 + (rand() % 150)),  // ms
      heartbeatInterval(50), running(true) {
    // Initialize with a dummy log entry at index 0
    log.push_back({0, 0, ""});
}

RaftNode::~RaftNode() {
    shutdown();
}

void RaftNode::start() {
    // Launch election timer thread
    std::thread(&RaftNode::runElectionTimer, this).detach();
}

void RaftNode::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        running = false;
    }
    cv.notify_all();
}

bool RaftNode::handleRequestVote(int candidateTerm, int candidateId,
                                  int lastLogIndex, int lastLogTerm) {
    // TODO: implement vote granting logic
    return false;
}

bool RaftNode::handleAppendEntries(int leaderTerm, int leaderId,
                                    int prevLogIndex, int prevLogTerm,
                                    const std::vector<LogEntry>& entries,
                                    int leaderCommit) {
    // TODO: implement log append & consistency check
    return false;
}

void RaftNode::runElectionTimer() {
    // TODO: wait for electionTimeout; if no heartbeat, start election
    while (running) {
        std::unique_lock<std::mutex> lock(mtx);
        if (cv.wait_for(lock, electionTimeout) == std::cv_status::timeout) {
            // start election
        }
    }
}

void RaftNode::sendHeartbeats() {
    // TODO: leader periodically sends AppendEntries with no entries
}

void RaftNode::becomeLeader() {
    state = RaftState::Leader;
    // initialize leader state, nextIndex, matchIndex
}

void RaftNode::becomeFollower(int term) {
    state = RaftState::Follower;
    currentTerm = term;
    votedFor = -1;
}

void RaftNode::replicateLog() {
    // TODO: send log entries to followers
}

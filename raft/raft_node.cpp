#include "raft_node.h"
#include <thread>
#include <random>
#include <mutex>
#include <condition_variable>

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
    // Implement vote granting logic

    std::lock_guard<std::mutex> lock(mtx);

    // 1. Reply FALSE if candidate's term < ours
    if(candidateTerm < currentTerm){
        return false;
    }

    // 2. If Candidate Term > currentTerm, update term to latest and convert it to follower
    if(candidateTerm > currentTerm){
        becomeFollower(candidateTerm);
    }

    // 3, Check if we have already voted for this term
    if(votedFor != -1 && votedFor != candidateId){
        return false;
    }

    // 4. Check candidate's log up-to-date
    const LogEntry &lastEntry = log.back();
    int myLastTerm = lastEntry.term;
    int myLastIndex = lastEntry.index;

    bool logOk = (lastLogTerm > myLastTerm) || (lastLogTerm == myLastTerm && lastLogIndex >= myLastIndex);
    if(!logOk){
        return false;
    }

    // 5. Grant Vote
    votedFor = candidateId;

    // Reset election timer
    cv.notify_all();
    return true;
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

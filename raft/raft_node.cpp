// raft/raft_node.cpp
#include "raft_node.h"
#include <random>
#include <thread>
#include <iostream>

RaftNode::RaftNode(int id, const vector<int>& peerIds)
    : nodeId(id), peers(peerIds), state(RaftState::Follower),
      currentTerm(0), votedFor(-1), commitIndex(0), lastApplied(0),
      electionTimeout(milliseconds(150 + rand() % 150)),
      heartbeatInterval(milliseconds(50)), running(true) {
    // Dummy log entry at index 0
    log.push_back({0, 0, ""});
}

void RaftNode::start() {
    // Start election timer in a separate thread
    thread([this]() { runElectionTimer(); }).detach();
}

void RaftNode::shutdown() {
    {
        lock_guard<mutex> lock(mtx);
        running = false;
    }
    cv.notify_all();
}

bool RaftNode::handleRequestVote(int candidateTerm, int candidateId,
                                 int lastLogIndex, int lastLogTerm) {
    lock_guard<mutex> lock(mtx);
    if (candidateTerm < currentTerm) return false;
    if (candidateTerm > currentTerm) {
        becomeFollower(candidateTerm);
    }
    if ((votedFor == -1 || votedFor == candidateId) &&
        isLogUpToDate(lastLogIndex, lastLogTerm)) {
        votedFor = candidateId;
        resetElectionTimeout();
        return true;
    }
    return false;
}

bool RaftNode::handleAppendEntries(int leaderTerm, int leaderId,
                                   int prevLogIndex, int prevLogTerm,
                                   const vector<LogEntry>& entries,
                                   int leaderCommit) {
    lock_guard<mutex> lock(mtx);
    if (leaderTerm < currentTerm) return false;
    becomeFollower(leaderTerm);
    resetElectionTimeout();
    // Check if log contains entry at prevLogIndex with matching term
    if (prevLogIndex >= (int)log.size() || log[prevLogIndex].term != prevLogTerm)
        return false;
    // Append new entries, overwriting conflicts
    log.resize(prevLogIndex + 1);
    for (const auto& e : entries) {
        log.push_back(e);
    }
    // Update commit index
    commitIndex = min<int64_t>(leaderCommit, (int64_t)log.size() - 1);
    return true;
}

void RaftNode::runElectionTimer() {
    unique_lock<mutex> lock(mtx);
    while (running) {
        if (cv.wait_for(lock, electionTimeout) == cv_status::timeout) {
            if (state != RaftState::Leader) {
                becomeCandidate();
            }
            resetElectionTimeout();
        }
    }
}

void RaftNode::runHeartbeatTimer() {
    while (true) {
        this_thread::sleep_for(heartbeatInterval);
        lock_guard<mutex> lock(mtx);
        if (!running || state != RaftState::Leader) break;
        replicateLogEntries();
    }
}

void RaftNode::becomeFollower(int term) {
    state = RaftState::Follower;
    currentTerm = term;
    votedFor = -1;
}

void RaftNode::becomeCandidate() {
    state = RaftState::Candidate;
    currentTerm++;
    votedFor = nodeId;
    int votes = 1;  // self-vote
    int majority = peers.size() / 2 + 1;
    int lastIndex = log.back().index;
    int lastTerm = log.back().term;
    for (int peer : peers) {
        if (sendRequestVoteRpc(peer, currentTerm, nodeId, lastIndex, lastTerm)) {
            votes++;
        }
        if (votes >= majority) {
            becomeLeader();
            return;
        }
    }
    // If no majority, revert to follower
    if (votes < majority) {
        becomeFollower(currentTerm);
    }
}

void RaftNode::becomeLeader() {
    state = RaftState::Leader;
    // Append no-op entry to log
    log.push_back({currentTerm, (int64_t)log.size(), ""});
    // Start heartbeat thread
    thread([this]() { runHeartbeatTimer(); }).detach();
}

void RaftNode::resetElectionTimeout() {
    electionTimeout = milliseconds(150 + rand() % 150);
    cv.notify_all();
}

bool RaftNode::isLogUpToDate(int lastLogIndex, int lastLogTerm) {
    const auto& last = log.back();
    if (lastLogTerm != last.term) return lastLogTerm > last.term;
    return lastLogIndex >= last.index;
}

void RaftNode::replicateLogEntries() {
    int prevIndex = log.size() - 2;
    int prevTerm = log[prevIndex].term;
    vector<LogEntry> newEntries = { log.back() };
    for (int peer : peers) {
        sendAppendEntriesRpc(peer, currentTerm, nodeId,
                             prevIndex, prevTerm,
                             newEntries, commitIndex);
    }
}

bool RaftNode::sendRequestVoteRpc(int peerId, int term, int candidateId,
                                  int lastLogIndex, int lastLogTerm) {
    // Placeholder: actual gRPC client call to peer
    cout << "[Node " << nodeId << "] RequestVote to peer " << peerId
         << " for term " << term << endl;
    return true;
}

bool RaftNode::sendAppendEntriesRpc(int peerId, int leaderTerm, int leaderId,
                                    int prevLogIndex, int prevLogTerm,
                                    const vector<LogEntry>& entries,
                                    int leaderCommit) {
    // Placeholder: actual gRPC client call to peer
    cout << "[Node " << nodeId << "] AppendEntries to peer " << peerId
         << " prevIndex=" << prevLogIndex
         << " entriesCount=" << entries.size() << endl;
    return true;
}

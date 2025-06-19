// raft/raft_node.cpp
#include "raft_node.h"
#include <random>
#include <thread>
#include <iostream>

RaftNode::RaftNode(int id, const vector<int>& peerIds)
    : nodeId(id), peers(peerIds), state(RaftState::Follower),
      currentTerm(0), votedFor(-1), commitIndex(0), lastApplied(0),
      electionTimeout(milliseconds(150 + rand() % 150)),
      heartbeatInterval(milliseconds(50)) {
    // Dummy log entry at index 0
    log.push_back({0, 0, ""});
}

void RaftNode::start() {
    thread([this]() { runElectionTimer(); }).detach();
}

void RaftNode::shutdown() {
    { lock_guard<mutex> lock(mtx); running = false; }
    cv.notify_all();
}

bool RaftNode::handleRequestVote(int candidateTerm, int candidateId,
                                 int lastLogIndex, int lastLogTerm) {
    lock_guard<mutex> lock(mtx);
    if (candidateTerm < currentTerm) return false;
    if (candidateTerm > currentTerm) becomeFollower(candidateTerm);
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
    if (prevLogIndex >= log.size() || log[prevLogIndex].term != prevLogTerm)
        return false;
    log.resize(prevLogIndex + 1);
    for (auto& e : entries) log.push_back(e);
    commitIndex = min<int64_t>(leaderCommit, log.size() - 1);
    return true;
}

void RaftNode::runElectionTimer() {
    unique_lock<mutex> lock(mtx);
    while (running) {
        if (cv.wait_for(lock, electionTimeout) == std::cv_status::timeout) {
            if (state != RaftState::Leader) becomeCandidate();
            resetElectionTimeout();
        }
    }
}

void RaftNode::runHeartbeatTimer() {
    while (true) {
        this_thread::sleep_for(heartbeatInterval);
        lock_guard<mutex> lock(mtx);
        if (state != RaftState::Leader || !running) break;
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
    int votes = 1;  // vote for self
    int majority = peers.size() / 2 + 1;
    for (int peer : peers) {
        if (sendRequestVoteRpc(peer, currentTerm, nodeId,
                               log.back().index, log.back().term)) {
            votes++;
        }
        if (votes >= majority) {
            becomeLeader();
            return;
        }
    }
    // Stay candidate or revert
    if (votes < majority) becomeFollower(currentTerm);
}

void RaftNode::becomeLeader() {
    state = RaftState::Leader;
    log.push_back({currentTerm, log.size(), ""});  // no-op entry for leader
    thread([this]() { runHeartbeatTimer(); }).detach();
}

void RaftNode::resetElectionTimeout() {
    electionTimeout = milliseconds(150 + rand() % 150);
    cv.notify_all();
}

bool RaftNode::isLogUpToDate(int lastLogIndex, int lastLogTerm) {
    auto& last = log.back();
    if (lastLogTerm != last.term) return lastLogTerm > last.term;
    return lastLogIndex >= last.index;
}

void RaftNode::replicateLogEntries() {
    for (int peer : peers) {
        sendAppendEntriesRpc(peer, currentTerm, nodeId,
                             (int)log.size() - 2, log[log.size()-2].term,
                             vector<LogEntry>{log.back()},
                             commitIndex);
    }
}

bool RaftNode::sendRequestVoteRpc(int peerId, int term, int candidateId,
                                  int lastLogIndex, int lastLogTerm) {
    // TODO: replace with real RPC call to peer
    cout << "RequestVote sent to " << peerId << " for term " << term << endl;
    return true;  // assume success for now
}

bool RaftNode::sendAppendEntriesRpc(int peerId, int leaderTerm, int leaderId,
                                    int prevLogIndex, int prevLogTerm,
                                    const vector<LogEntry>& entries,
                                    int leaderCommit) {
    // TODO: replace with real RPC call to peer
    cout << "AppendEntries sent to " << peerId << " prevIndex=" << prevLogIndex
         << " entries=" << entries.size() << endl;
    return true;
}

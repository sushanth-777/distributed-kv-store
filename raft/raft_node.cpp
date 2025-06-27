#include "raft_node.h"
#include <thread>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <sstream>

// Out-of-line definition we just declared in the header:
void RaftNode::setAppendEntriesCallback(AppendEntriesFn fn) {
    std::lock_guard<std::mutex> lock(mtx);
    appendEntriesRpc = std::move(fn);
}

void RaftNode::setRequestVoteCallback(RequestVoteFn fn) {
    std::lock_guard<std::mutex> lock(mtx);
    requestVoteRpc = std::move(fn);
}

RaftNode::RaftNode(int id, const std::vector<int>& peerIds)
    : nodeId(id),
      peers(peerIds),
      state(RaftState::Follower),
      currentTerm(0),
      votedFor(-1),
      commitIndex(0),
      lastApplied(0),
      electionTimeout(150 + (std::rand() % 150)),
      heartbeatInterval(50),
      running(true) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    // Dummy entry at index 0
    log.push_back({0, 0, ""});
}

RaftNode::~RaftNode() {
    shutdown();
}

void RaftNode::start() {
    std::thread(&RaftNode::runElectionTimer, this).detach();
}

void RaftNode::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        running = false;
    }
    cv.notify_all();
}

bool RaftNode::handleRequestVote(int candidateTerm,
                                 int candidateId,
                                 int lastLogIndex,
                                 int lastLogTerm) {
    std::lock_guard<std::mutex> lock(mtx);

    if (candidateTerm < currentTerm) {
        return false;
    }
    if (candidateTerm > currentTerm) {
        becomeFollower(candidateTerm);
    }
    if (votedFor != -1 && votedFor != candidateId) {
        return false;
    }

    const LogEntry& lastEntry = log.back();
    int myLastTerm = lastEntry.term;
    int myLastIndex = lastEntry.index;
    bool logOk = (lastLogTerm > myLastTerm) ||
                 (lastLogTerm == myLastTerm && lastLogIndex >= myLastIndex);
    if (!logOk) {
        return false;
    }

    votedFor = candidateId;
    cv.notify_all();
    return true;
}

bool RaftNode::handleAppendEntries(int leaderTerm,
                                   int leaderId,
                                   int prevLogIndex,
                                   int prevLogTerm,
                                   const std::vector<LogEntry>& entries,
                                   int leaderCommit) {
    std::lock_guard<std::mutex> lock(mtx);

    if (leaderTerm < currentTerm) {
        return false;
    }
    if (leaderTerm > currentTerm) {
        becomeFollower(leaderTerm);
    }
    cv.notify_all();

    if (prevLogIndex >= static_cast<int>(log.size()) ||
        log[prevLogIndex].term != prevLogTerm) {
        return false;
    }

    int insertIdx = prevLogIndex + 1;
    for (const auto& entry : entries) {
        if (insertIdx < static_cast<int>(log.size())) {
            if (log[insertIdx].term != entry.term) {
                log.resize(insertIdx);
                log.push_back(entry);
            }
        } else {
            log.push_back(entry);
        }
        insertIdx++;
    }

    int lastNewIndex = prevLogIndex + static_cast<int>(entries.size());
    if (leaderCommit > commitIndex) {
        commitIndex = std::min(leaderCommit, lastNewIndex);
        commitCv.notify_all();
        while (lastApplied < commitIndex) {
            lastApplied++;
            if (applyCb) applyCb(log[lastApplied]);
        }
    }

    return true;
}

void RaftNode::runElectionTimer() {
    std::unique_lock<std::mutex> lock(mtx);
    while (running) {
        if (cv.wait_for(lock, electionTimeout) == std::cv_status::timeout) {
            lock.unlock();
            startElection();
            lock.lock();
        }
    }
}

void RaftNode::startElection() {
    int lastIndex, lastTerm;
    int voteCount = 1;  // vote for self

    {
        std::lock_guard<std::mutex> lock(mtx);
        currentTerm++;
        state = RaftState::Candidate;
        votedFor = nodeId;
        cv.notify_all();

        lastIndex = log.back().index;
        lastTerm  = log.back().term;
    }

    for (int peer : peers) {
        bool granted = false;
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (requestVoteRpc) {
                granted = requestVoteRpc(
                    peer,
                    currentTerm,
                    nodeId,
                    lastIndex,
                    lastTerm
                );
            }
        }
        if (granted) voteCount++;
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        int majority = static_cast<int>(peers.size() + 1) / 2 + 1;
        if (state == RaftState::Candidate && voteCount >= majority) {
            state = RaftState::Leader;
            int newIndex = log.back().index + 1;
            log.push_back({currentTerm, newIndex, ""});
            commitIndex = newIndex;
            while (lastApplied < commitIndex) {
                lastApplied++;
                if (applyCb) applyCb(log[lastApplied]);
            }
            std::thread(&RaftNode::sendHeartbeats, this).detach();
        }
        electionTimeout = std::chrono::milliseconds(
            150 + (std::rand() % 150)
        );
    }
}

void RaftNode::sendHeartbeats() {
    std::vector<LogEntry> empty;
    while (true) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (state != RaftState::Leader || !running) break;
            for (int peer : peers) {
                if (appendEntriesRpc) {
                    int prevIndex = log.back().index;
                    int prevTerm  = log.back().term;
                    appendEntriesRpc(
                        peer,
                        currentTerm,
                        nodeId,
                        prevIndex,
                        prevTerm,
                        empty,
                        commitIndex
                    );
                }
            }
        }
        std::this_thread::sleep_for(heartbeatInterval);
    }
}

void RaftNode::becomeLeader() {
    state = RaftState::Leader;
    std::thread(&RaftNode::sendHeartbeats, this).detach();
}

void RaftNode::becomeFollower(int term) {
    state = RaftState::Follower;
    currentTerm = term;
    votedFor = -1;
}

void RaftNode::replicateLog() {
    // (leader will fire off per-peer AppendEntries via sendHeartbeats)
}

void RaftNode::setApplyCallback(ApplyFn fn) {
    std::lock_guard<std::mutex> lock(mtx);
    applyCb = std::move(fn);
}

bool RaftNode::replicateCommand(const std::string& command, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mtx);

    if (state != RaftState::Leader) return false;

    int newIndex = log.back().index + 1;
    log.push_back({currentTerm, newIndex, command});
    lock.unlock();

    replicateLog();

    lock.lock();
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);
    bool ok = commitCv.wait_until(lock, deadline, [&] {
        return commitIndex >= newIndex;
    });
    return ok;
}

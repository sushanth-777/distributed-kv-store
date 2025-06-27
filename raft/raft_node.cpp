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
   
    // seed once (you can do this globally too)
    std::srand((unsigned)std::time(nullptr));
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

void RaftNode::setRequestVoteCallback(RequestVoteFn fn) {
    std::lock_guard<std::mutex> lock(mtx);
    requestVoteRpc = std::move(fn);
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
    // Implement log append & consistency check
    std::lock_guard<std::mutex> lock(mtx);

    // 1) Reply false if term < currentTerm
    if (leaderTerm < currentTerm) {
        return false;
    }
    // 2) If term > currentTerm, become follower
    if (leaderTerm > currentTerm) {
        becomeFollower(leaderTerm);
    }
    // 3) Reset election timer
    cv.notify_all();

    // 4) Check log consistency
    if (prevLogIndex >= static_cast<int>(log.size()) ||
        log[prevLogIndex].term != prevLogTerm) {
        return false;
    }

    // 5) Append new entries, handling conflicts
    int insertIdx = prevLogIndex + 1;
    for (const auto& entry : entries) {
        if (insertIdx < static_cast<int>(log.size())) {
            // conflict?
            if (log[insertIdx].term != entry.term) {
                // delete the existing entry and all that follow
                log.resize(insertIdx);
                log.push_back(entry);
            }
        } else {
            // no entry there yet → just append
            log.push_back(entry);
        }
        insertIdx++;
    }

    // 6) Update commitIndex
    int lastNewIndex = prevLogIndex + entries.size();
    if (leaderCommit > commitIndex) {
        commitIndex = std::min(leaderCommit, lastNewIndex);
        // Apply all newly committed entries
        while (lastApplied < commitIndex) {
            lastApplied++;
            if (applyCb) applyCb(log[lastApplied]);
        }
    }

    return true;
}

void RaftNode::runElectionTimer() {
    //wait for electionTimeout; if no heartbeat, start election
    std::unique_lock<std::mutex> lock(mtx);

    while (running) {
        if (cv.wait_for(lock, electionTimeout) == std::cv_status::timeout) {
            // start election
            lock.unlock();
            startElection();
            lock.lock();
        }
    }
}

void RaftNode::startElection() {
    int lastIndex, lastTerm;
    int voteCount = 1;  // we always vote for ourselves

    // 1) Become Candidate in a new term
    {
        std::lock_guard<std::mutex> lock(mtx);
        currentTerm++;
        state = RaftState::Candidate;
        votedFor = nodeId;
        // reset the election timer
        cv.notify_all();

        // snapshot our log’s last index/term for the RPCs below
        lastIndex = log.back().index;
        lastTerm  = log.back().term;
    }

    // 2) Send RequestVote RPCs to all peers
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

    // 3) If we have a majority, become Leader
    {
        std::lock_guard<std::mutex> lock(mtx);
        // majority = (N+1)/2 of total nodes including self
        int majority = static_cast<int>(peers.size() + 1) / 2 + 1;
        if (state == RaftState::Candidate && voteCount >= majority) {
            // transition to Leader
            state = RaftState::Leader;

            // a) append a no-op entry at the head of our new leadership
            int newIndex = log.back().index + 1;
            log.push_back({ currentTerm, newIndex, "" });

            // b) immediately commit that entry
            commitIndex = newIndex;

            // c) apply any newly committed entries
            while (lastApplied < commitIndex) {
                lastApplied++;
                if (applyCb) {
                    applyCb(log[lastApplied]);
                }
            }

            // d) start heartbeats in the background
            std::thread(&RaftNode::sendHeartbeats, this).detach();
        }

        // 4) Randomize next election timeout to avoid livelock
        electionTimeout = std::chrono::milliseconds(
            150 + (std::rand() % 150)
        );
    }
}



void RaftNode::sendHeartbeats() {
    // Leader periodically sends AppendEntries with no entries
    std::vector<LogEntry> empty;  // heartbeat has no new entries
    while (state == RaftState::Leader && running) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            for (int peer : peers) {
                if (appendEntriesRpc) {
                    // always send prevLogIndex = last log index,
                    // prevLogTerm = term of that entry
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
    // initialize leader state, nextIndex, matchIndex
    std::thread(&RaftNode::sendHeartbeats, this).detach();
}

void RaftNode::becomeFollower(int term) {
    state = RaftState::Follower;
    currentTerm = term;
    votedFor = -1;
}

void RaftNode::replicateLog() {
    // TODO: send log entries to followers
}

void RaftNode::setApplyCallback(ApplyFn fn) {
    std::lock_guard<std::mutex> lock(mtx);
    applyCb = std::move(fn);
}


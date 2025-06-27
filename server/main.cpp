#include <grpcpp/grpcpp.h>
#include "kv.grpc.pb.h"
#include "kv.pb.h"
#include "raft_node.h"
#include "kv_store.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// Consensus RPCs (all defined in kv.proto under package kv)
using kv::Raft;
using kv::RequestVoteArgs;
using kv::RequestVoteReply;
using kv::AppendEntriesArgs;
using kv::AppendEntriesReply;

// Client-facing KV RPCs (defined in the same kv.proto)
using kv::KV;
using kv::PutRequest;
using kv::PutReply;
using kv::GetRequest;
using kv::GetReply;
using kv::DeleteRequest;
using kv::DeleteReply;

// ------------------------------------------------------------
// RaftServiceImpl: forwards Raft RPCs into the RaftNode instance
// ------------------------------------------------------------
class RaftServiceImpl final : public Raft::Service {
public:
  explicit RaftServiceImpl(RaftNode& node) : node_(node) {}

  Status RequestVote(ServerContext* /*ctx*/,
                     const RequestVoteArgs* req,
                     RequestVoteReply* resp) override {
    bool granted = node_.handleRequestVote(
      req->term(),
      req->candidateid(),
      req->lastlogindex(),
      req->lastlogterm()
    );
    resp->set_term(node_.getCurrentTerm());
    resp->set_votegranted(granted);
    return Status::OK;
  }

  Status AppendEntries(ServerContext* /*ctx*/,
                       const AppendEntriesArgs* req,
                       AppendEntriesReply* resp) override {
    // Convert each protobuf LogEntry → our internal LogEntry
    std::vector<LogEntry> entries;
    entries.reserve(req->entries_size());
    for (int i = 0; i < req->entries_size(); ++i) {
      const auto& e = req->entries(i);
      // <-- explicit LogEntry construction fixes the push_back error:
      entries.push_back({
        static_cast<int>(e.term()),
        static_cast<int>(e.index()),
        e.command()
      });
    }

    bool success = node_.handleAppendEntries(
      req->term(),
      req->leaderid(),
      req->prevlogindex(),
      req->prevlogterm(),
      entries,
      req->leadercommit()
    );
    resp->set_term(node_.getCurrentTerm());
    resp->set_success(success);
    return Status::OK;
  }

private:
  RaftNode& node_;
};

// ------------------------------------
// KVServiceImpl: handles client requests
// ------------------------------------
class KVServiceImpl final : public KV::Service {
public:
  KVServiceImpl(RaftNode& node, KVStore& kv)
    : node_(node), kv_(kv) {}

  Status Put(ServerContext* /*ctx*/,
             const PutRequest* req,
             PutReply* resp) override {
    // Only the leader accepts writes
    if (!node_.isLeader()) {
      resp->set_success(false);
      return Status::OK;
    }
    kv_.put(req->key(), req->value());
    resp->set_success(true);
    return Status::OK;
  }

  Status Get(ServerContext* /*ctx*/,
             const GetRequest* req,
             GetReply* resp) override {
    auto opt = kv_.get(req->key());
    resp->set_found(opt.has_value());
    if (opt) resp->set_value(*opt);
    return Status::OK;
  }

  Status Delete(ServerContext* /*ctx*/,
                const DeleteRequest* req,
                DeleteReply* resp) override {
    if (!node_.isLeader()) {
      resp->set_success(false);
      return Status::OK;
    }
    kv_.del(req->key());
    resp->set_success(true);
    return Status::OK;
  }

private:
  RaftNode& node_;
  KVStore&  kv_;
};

// -------------
// Entry point
// -------------
int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <myId> <peerId1:addr1> [peerId2:addr2 ...]\n";
    return 1;
  }

  int myId = std::stoi(argv[1]);
  std::vector<int> peerIds;
  std::vector<std::string> peerAddrs;
  for (int i = 2; i < argc; ++i) {
    std::string spec(argv[i]);
    auto colon = spec.find(':');
    peerIds.push_back(std::stoi(spec.substr(0, colon)));
    peerAddrs.push_back(spec.substr(colon + 1));
  }

  RaftNode node(myId, peerIds);
  KVStore kv;

  node.setApplyCallback([&kv](const LogEntry& entry) {
    std::istringstream iss(entry.command);
    std::string op, key, val;
    iss >> op >> key;
    if (op == "put" && (iss >> val)) {
      kv.put(key, val);
    } else if (op == "del") {
      kv.del(key);
    }
  });

  std::unordered_map<int, std::unique_ptr<Raft::Stub>> stubs;
  for (size_t i = 0; i < peerIds.size(); ++i) {
    stubs[peerIds[i]] = Raft::NewStub(
      grpc::CreateChannel(peerAddrs[i],
                          grpc::InsecureChannelCredentials())
    );
  }

  node.setRequestVoteCallback(
    [&stubs](int peer, int term, int candidateId, int lastLogIndex, int lastLogTerm) {
      RequestVoteArgs req;
      req.set_term(term);
      req.set_candidateid(candidateId);
      req.set_lastlogindex(lastLogIndex);
      req.set_lastlogterm(lastLogTerm);
      RequestVoteReply resp;
      grpc::ClientContext ctx;
      return stubs[peer]->RequestVote(&ctx, req, &resp).ok()
             && resp.votegranted();
    }
  );

  node.setAppendEntriesCallback(
    [&stubs](int peer, int term, int leaderId,
             int prevLogIndex, int prevLogTerm,
             const std::vector<LogEntry>& entries,
             int leaderCommit) {
      AppendEntriesArgs req;
      req.set_term(term);
      req.set_leaderid(leaderId);
      req.set_prevlogindex(prevLogIndex);
      req.set_prevlogterm(prevLogTerm);
      for (const auto& e : entries) {
        auto* pe = req.add_entries();
        pe->set_term(e.term);
        pe->set_index(e.index);
        pe->set_command(e.command);
      }
      req.set_leadercommit(leaderCommit);
      AppendEntriesReply resp;
      grpc::ClientContext ctx;
      return stubs[peer]->AppendEntries(&ctx, req, &resp).ok()
             && resp.success();
    }
  );

  node.start();

  RaftServiceImpl raftSvc(node);
  KVServiceImpl  kvSvc (node, kv);

  ServerBuilder builder;
  std::string listenAddr = "0.0.0.0:" + std::to_string(50050 + myId);
  builder.AddListeningPort(listenAddr, grpc::InsecureServerCredentials());
  builder.RegisterService(&raftSvc);
  builder.RegisterService(&kvSvc);

  auto server = builder.BuildAndStart();
  std::cout << "Node " << myId << " listening on " << listenAddr << "\n";
  server->Wait();

  return 0;
}

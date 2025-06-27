#include <grpcpp/grpcpp.h>
#include "raft.grpc.pb.h"
#include "raft.pb.h"
#include "raft_node.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>           // for parsing command strings

#include "kv_store.h"        // your in-memory KV store

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using raft::Raft;
using raft::RequestVoteRequest;
using raft::RequestVoteReply;
using raft::AppendEntriesRequest;
using raft::AppendEntriesReply;

class RaftServiceImpl final : public Raft::Service {
public:
  explicit RaftServiceImpl(RaftNode& node) : node_(node) {}

  Status RequestVote(ServerContext* /*ctx*/,
                     const RequestVoteRequest* req,
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
                       const AppendEntriesRequest* req,
                       AppendEntriesReply* resp) override {
    // Convert protobuf LogEntry → your LogEntry struct
    std::vector<LogEntry> entries;
    entries.reserve(req->entries_size());
    for (int i = 0; i < req->entries_size(); ++i) {
      const auto& e = req->entries(i);
      entries.push_back({ e.term(), e.index(), e.command() });
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

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <myId> <peerId1:addr1> [peerId2:addr2 ...]\n";
    return 1;
  }

  // Parse this node's ID
  int myId = std::stoi(argv[1]);

  // Parse peers: each arg "peerId:host:port"
  std::vector<int> peerIds;
  std::vector<std::string> peerAddrs;
  for (int i = 2; i < argc; ++i) {
    std::string spec(argv[i]);
    auto colon = spec.find(':');
    peerIds.push_back(std::stoi(spec.substr(0, colon)));
    peerAddrs.push_back(spec.substr(colon + 1));
  }

  // 1) Create the Raft node
  RaftNode node(myId, peerIds);

  // 2) Create the in-memory KV store and hook up apply-callback
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
    // ignore any unknown commands
  });

  // 3) Build gRPC stubs for talking to peers
  std::unordered_map<int, std::unique_ptr<Raft::Stub>> stubs;
  for (size_t i = 0; i < peerIds.size(); ++i) {
    stubs[peerIds[i]] = Raft::NewStub(
      grpc::CreateChannel(peerAddrs[i],
                          grpc::InsecureChannelCredentials())
    );
  }

  // 4a) Wire up outgoing RequestVote RPCs
  node.setRequestVoteCallback(
    [&stubs](int peer,
             int term,
             int candidateId,
             int lastLogIndex,
             int lastLogTerm) -> bool {
      RequestVoteRequest req;
      req.set_term(term);
      req.set_candidateid(candidateId);
      req.set_lastlogindex(lastLogIndex);
      req.set_lastlogterm(lastLogTerm);

      RequestVoteReply resp;
      grpc::ClientContext ctx;
      auto status = stubs[peer]->RequestVote(&ctx, req, &resp);
      if (!status.ok()) {
        std::cerr << "RequestVote RPC to peer " << peer
                  << " failed: " << status.error_message() << "\n";
        return false;
      }
      return resp.votegranted();
    }
  );

  // 4b) Wire up outgoing AppendEntries RPCs
  node.setAppendEntriesCallback(
    [&stubs](int peer,
             int term,
             int leaderId,
             int prevLogIndex,
             int prevLogTerm,
             const std::vector<LogEntry>& entries,
             int leaderCommit) -> bool {
      AppendEntriesRequest req;
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
      auto status = stubs[peer]->AppendEntries(&ctx, req, &resp);
      return status.ok() && resp.success();
    }
  );

  // 5) Start Raft’s election timer (and, upon election, heartbeats)
  node.start();

  // 6) Launch the gRPC server to accept incoming Raft RPCs
  RaftServiceImpl service(node);
  ServerBuilder builder;
  std::string listenAddr = "0.0.0.0:" + std::to_string(50050 + myId);
  builder.AddListeningPort(listenAddr, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Node " << myId << " listening on " << listenAddr << "\n";
  server->Wait();

  return 0;
}

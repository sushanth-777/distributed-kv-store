#include <grpcpp/grpcpp.h>
#include "kv.grpc.pb.h"
#include <iostream>
#include <string>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using kv::KV;
using kv::PutRequest;
using kv::PutReply;
using kv::GetRequest;
using kv::GetReply;
using kv::DeleteRequest;
using kv::DeleteReply;

class KVClient {
public:
  KVClient(std::shared_ptr<Channel> channel)
    : stub_(KV::NewStub(channel)) {}

  bool Put(const std::string& key, const std::string& value) {
    PutRequest req;
    req.set_key(key);
    req.set_value(value);
    PutReply resp;
    ClientContext ctx;
    Status ok = stub_->Put(&ctx, req, &resp);
    return ok.ok() && resp.success();
  }

  bool Get(const std::string& key, std::string& out) {
    GetRequest req;
    req.set_key(key);
    GetReply resp;
    ClientContext ctx;
    Status ok = stub_->Get(&ctx, req, &resp);
    if (ok.ok() && resp.found()) {
      out = resp.value();
      return true;
    }
    return false;
  }

  bool Delete(const std::string& key) {
    DeleteRequest req;
    req.set_key(key);
    DeleteReply resp;
    ClientContext ctx;
    Status ok = stub_->Delete(&ctx, req, &resp);
    return ok.ok() && resp.success();
  }

private:
  std::unique_ptr<KV::Stub> stub_;
};

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0]
              << " <serverAddr> <put|get|del> <key> [value]\n";
    return 1;
  }
  std::string addr  = argv[1];
  std::string cmd   = argv[2];
  std::string key   = argv[3];
  std::string value = (argc>=5 ? argv[4] : "");

  KVClient client(
    grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())
  );

  if (cmd == "put") {
    bool ok = client.Put(key, value);
    std::cout << (ok ? "OK\n" : "FAIL\n");
  }
  else if (cmd == "get") {
    std::string out;
    bool ok = client.Get(key, out);
    if (ok) std::cout << "VALUE: " << out << "\n";
    else    std::cout << "NOT FOUND\n";
  }
  else if (cmd == "del") {
    bool ok = client.Delete(key);
    std::cout << (ok ? "OK\n" : "FAIL\n");
  }
  else {
    std::cerr << "Unknown command: " << cmd << "\n";
    return 1;
  }
  return 0;
}

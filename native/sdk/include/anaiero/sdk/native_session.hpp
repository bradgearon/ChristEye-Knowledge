#pragma once
#include "anaiero/sdk/host.hpp"
#include "anaiero/sdk/lifecycle.hpp"
namespace anaiero::sdk::host {
// Native composition for an already initialized canonical modular session.
// Opening checks retained module/model identities and the persisted capacity.
// No legacy/live-session format is silently migrated by this adapter.
class NativeSession {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  NativeSession();
  ~NativeSession();
  Result open(std::string_view store_directory);
  Result ensure_engines();
  Result retain_host_artifact(std::string_view executable_path);
  SessionService& runtime();
  SessionRunner& runner();
  evidence::ICommitStore& store();
  lifecycle::Service& lifecycle();
};
}

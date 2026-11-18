#pragma once
#include "anaiero/sdk/evidence.hpp"
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>

namespace anaiero::storage {
namespace fs=std::filesystem;
using namespace sdk;
struct Failure:std::runtime_error { Code code; Failure(Code c,const std::string& m):std::runtime_error(m),code(c){} };
Bytes read_file(const fs::path&,std::size_t maximum);
void write_file(const fs::path&,std::string_view);
bool regular_exists(const fs::path&);
void remove_file(const fs::path&);
std::string number(std::uint64_t);
std::uint64_t parse_number(std::string_view);
Bytes seal(std::string_view schema,const std::vector<std::string>&);
std::vector<std::string> unseal(std::string_view,std::string_view schema,std::size_t max_fields=100000,std::size_t max_bytes=64U*1024U*1024U);

class SealedIndex final {
public:
  struct Node {std::string key,value;Digest left,right;std::uint64_t height=1,count=1,bytes=0,maximum_node_bytes=0;};
  explicit SealedIndex(fs::path directory):directory_(std::move(directory)){}
  std::string get(const Digest&,std::string_view,bool& found);
  void scan(const Digest&,std::string_view prefix,std::string_view after,std::size_t limit,
      std::vector<std::pair<std::string,std::string>>&,bool& more);
  Digest put(const Digest&,std::string key,std::string value);
  Node load(const Digest&);
  void begin();
  void discard_cache() noexcept;
  void publish_cleanup(const Digest&);
  void finish_cleanup(const Digest&);
  void audit(const Digest&);
  evidence::IndexStatistics stats;
  const fs::path& directory()const noexcept{return directory_;}
private:
  fs::path directory_;
  std::map<Digest,Node> cache_;
  std::set<Digest> obsolete_,pending_;
  bool caching_=false;
  std::size_t cache_bytes_=0;
  void cache(const Digest&,const Node&);
  Digest save(Node);
  Digest insert(const Digest&,const std::string&,const std::string&);
  Digest balance(Node);
  Digest rotate_left(Node);
  Digest rotate_right(Node);
  void retire(const Digest&);
  std::uint64_t height(const Digest&);
  Digest node_at(const Digest&,std::string_view);
  void audit_node(const Digest&,const std::string*,const std::string*,std::size_t);
};
}

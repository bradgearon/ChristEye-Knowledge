#pragma once
#include "sealed_index.hpp"

namespace anaiero::storage {
// Immutable B+tree pages. A batch constructs its complete replacement paths
// under a fixed memory budget before canonical admission. Unmodified siblings
// stay reachable through their checksummed descriptors.
class PackedIndex final {
public:
  using Row=std::pair<std::string,std::string>;
  using Updates=std::map<std::string,std::string>;
  using RowReader=std::function<Row(std::uint64_t)>;
  PackedIndex(const fs::path& directory,evidence::IndexStatistics& statistics)
      :directory_(directory),stats_(statistics){}
  SealedIndex::Node metadata(const Digest&);
  std::string get(const Digest&,std::string_view,bool&);
  void scan(const Digest&,std::string_view,std::string_view,std::size_t,std::vector<Row>&,bool&);
  Digest prepare(const Digest&,const Updates&);
  void publish(const Digest&);
  void finish(const Digest&);
  Digest build_sorted(std::uint64_t,const RowReader&);
  void audit(const Digest&);
  void begin();
  void clear() noexcept;
  std::uint64_t work_bytes() const;
  std::uint64_t work_files() const;
  static std::uint64_t bulk_file_bound(std::uint64_t rows,std::uint64_t wire_upper);
private:
  struct Ref {
    std::string first,last;
    Digest digest;
    std::uint64_t height=0,count=0,bytes=0,largest=0;
    bool operator==(const Ref&) const=default;
  };
  struct Page {
    Ref ref;
    std::vector<Row> rows;
    std::vector<Ref> children;
  };
  using Refs=std::vector<Ref>;
  using Iter=Updates::const_iterator;
  const fs::path directory_;
  evidence::IndexStatistics& stats_;
  std::map<Digest,Page> cache_;
  std::map<Digest,Bytes> writes_;
  std::set<Digest> retired_;
  std::uint64_t cache_bytes_=0,write_bytes_=0;
  bool caching_=false;
  bool bulk_=false;
  Page read(const Digest&);
  Page checked(const Ref&);
  void write(const Digest&,const Bytes&);
  Ref make(Page);
  Refs leaves(std::vector<Row>);
  Refs branches(const Refs&);
  Refs update(const Page&,Iter,Iter);
  bool collect(const Page&,std::string_view,std::string_view,std::size_t,std::vector<Row>&);
  void verify(const Page&);
};
}

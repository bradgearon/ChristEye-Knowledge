#include "packed_index.hpp"
#include <algorithm>
#include <limits>

namespace anaiero::storage {
namespace {
constexpr std::size_t fanout=32;
constexpr std::size_t leaf_bytes=32U*1024U;
constexpr std::size_t decoder_limit=32U*1024U*1024U;
constexpr std::uint64_t stage_limit=32U*1024U*1024U;
constexpr std::uint64_t read_cache_limit=8U*1024U*1024U;
constexpr std::string_view page_schema="ANAIERO-PACKED-INDEX/1";
constexpr std::string_view gc_schema="ANAIERO-PACKED-GC/1";

void require(bool valid,std::string_view message){
  if(!valid)throw Failure(Code::corrupt,std::string(message));
}
std::uint64_t add(std::uint64_t a,std::uint64_t b){
  if(b>std::numeric_limits<std::uint64_t>::max()-a)throw Failure(Code::capacity,"packed index size overflow");
  return a+b;
}
std::size_t wire_bound(const PackedIndex::Row& row){
  return row.first.size()+row.second.size()+64U;
}
std::size_t groups(std::size_t count){
  return count/fanout+static_cast<std::size_t>(count%fanout!=0);
}
bool intersects(std::string_view first,std::string_view last,std::string_view prefix,std::string_view after){
  return last>=prefix&&(after.empty()||last>after)&&(first<=prefix||first.starts_with(prefix));
}
}

// Each adjacent pair of bulk leaves contains more than 32 rows or 32 KiB
// of admission-accounted row bytes. Charge disjoint pairs to those totals.
// Each subsequent branch level is constructed in groups of exactly 32.
std::uint64_t PackedIndex::bulk_file_bound(std::uint64_t rows,std::uint64_t wire_upper){
  if(rows==0)return 0;
  const auto pairs=add(rows/fanout,wire_upper/leaf_bytes);
  auto level=std::min(rows,add(add(pairs,pairs),1));
  auto total=level;
  while(level>1){
    level=level/fanout+static_cast<std::uint64_t>(level%fanout!=0);
    total=add(total,level);
  }
  return total;
}
void PackedIndex::clear() noexcept{
  cache_.clear();writes_.clear();retired_.clear();cache_bytes_=0;write_bytes_=0;caching_=false;
}
void PackedIndex::begin(){clear();caching_=true;}

PackedIndex::Page PackedIndex::read(const Digest& digest){
  require(valid_digest(digest),"invalid packed page identity");
  const auto cached=cache_.find(digest);
  if(cached!=cache_.end())return cached->second;
  Bytes raw;
  try{raw=read_file(directory_/(digest+".page"),decoder_limit);}
  catch(const Failure& e){
    if(e.code==Code::not_found)throw Failure(Code::corrupt,"referenced packed page is missing");
    throw;
  }
  ++stats_.nodes_read;
  require(sha256(raw)==digest,"packed page digest mismatch");
  const auto records=unseal(raw,page_schema,fanout+1,decoder_limit);
  require(records.size()>=2&&records.size()<=fanout+1,"invalid packed page arity");
  Page page;
  auto& ref=page.ref;
  ref.digest=digest;ref.bytes=raw.size();ref.largest=raw.size();
  ref.height=parse_number(records.front());
  require(ref.height>0&&ref.height<=96,"invalid packed page height");
  for(std::size_t i=1;i<records.size();++i){
    std::vector<std::string> fields;
    require(unpack(records[i],fields,7,decoder_limit).ok(),"invalid packed page entry");
    if(ref.height==1){
      require(fields.size()==2&&!fields[0].empty()&&(i==1||fields[0]>ref.last),"unordered packed leaf");
      ref.last=fields[0];
      if(i==1)ref.first=ref.last;
      ++ref.count;
      page.rows.emplace_back(std::move(fields[0]),std::move(fields[1]));
    }else{
      require(fields.size()==7,"invalid packed branch descriptor");
      Ref child{fields[0],fields[1],fields[2],parse_number(fields[3]),parse_number(fields[4]),
                parse_number(fields[5]),parse_number(fields[6])};
      require(!child.first.empty()&&child.last>=child.first&&(i==1||child.first>ref.last)&&valid_digest(child.digest)&&
              child.height==ref.height-1&&child.count>0&&child.largest>0&&child.bytes>=child.largest,
              "invalid packed child range or aggregate");
      ref.last=child.last;
      if(i==1)ref.first=child.first;
      ref.count=add(ref.count,child.count);ref.bytes=add(ref.bytes,child.bytes);
      ref.largest=std::max(ref.largest,child.largest);
      page.children.push_back(std::move(child));
    }
  }
  const auto cost=static_cast<std::uint64_t>(raw.size()*2U+records.size()*512U);
  if(caching_&&cache_.size()<2048&&cost<=read_cache_limit-cache_bytes_){
    cache_.emplace(digest,page);cache_bytes_+=cost;
    stats_.cache_peak_nodes=std::max(stats_.cache_peak_nodes,static_cast<std::uint64_t>(cache_.size()+writes_.size()));
    stats_.cache_peak_bytes=std::max(stats_.cache_peak_bytes,cache_bytes_+write_bytes_);
  }
  return page;
}
PackedIndex::Page PackedIndex::checked(const Ref& ref){
  auto page=read(ref.digest);
  require(page.ref==ref,"packed child disagrees with its parent descriptor");
  return page;
}
SealedIndex::Node PackedIndex::metadata(const Digest& root){
  if(root.empty())return {"","","","",0,0,0,0};
  const auto r=read(root).ref;
  return {r.first,"","","",r.height,r.count,r.bytes,r.largest};
}
void PackedIndex::write(const Digest& digest,const Bytes& bytes){
  const auto path=directory_/(digest+".page");
  if(regular_exists(path)){
    require(read_file(path,decoder_limit)==bytes,"existing packed page bytes differ");
    return;
  }
  write_file(path,bytes);
  ++stats_.nodes_written;
}
PackedIndex::Ref PackedIndex::make(Page page){
  Ref ref;
  std::vector<std::string> entries;
  if(!page.rows.empty()){
    ref.height=1;ref.count=page.rows.size();ref.first=page.rows.front().first;ref.last=page.rows.back().first;
    entries.push_back(number(1));
    for(const auto& row:page.rows)entries.push_back(pack({row.first,row.second}));
  }else{
    require(!page.children.empty(),"empty packed branch");
    ref.height=page.children.front().height+1;
    require(ref.height<=96,"packed tree height exceeds limit");
    ref.first=page.children.front().first;ref.last=page.children.back().last;
    entries.push_back(number(ref.height));
    for(const auto& child:page.children){
      ref.count=add(ref.count,child.count);ref.bytes=add(ref.bytes,child.bytes);ref.largest=std::max(ref.largest,child.largest);
      entries.push_back(pack({child.first,child.last,child.digest,number(child.height),number(child.count),number(child.bytes),number(child.largest)}));
    }
  }
  auto raw=seal(page_schema,entries);
  if(raw.size()>decoder_limit)throw Failure(Code::capacity,"packed page exceeds decoder limit");
  ref.digest=sha256(raw);ref.bytes=add(ref.bytes,raw.size());ref.largest=std::max(ref.largest,static_cast<std::uint64_t>(raw.size()));
  if(bulk_)write(ref.digest,raw);
  else if(!writes_.contains(ref.digest)){
    const auto cost=static_cast<std::uint64_t>(raw.size()+256U);
    if(writes_.size()>=8192||cost>stage_limit-write_bytes_)throw Failure(Code::capacity,"packed publication staging limit exceeded");
    writes_.emplace(ref.digest,std::move(raw));write_bytes_+=cost;++stats_.staged_nodes;
    stats_.cache_peak_nodes=std::max(stats_.cache_peak_nodes,static_cast<std::uint64_t>(cache_.size()+writes_.size()));
    stats_.cache_peak_bytes=std::max(stats_.cache_peak_bytes,cache_bytes_+write_bytes_);
  }
  return ref;
}
PackedIndex::Refs PackedIndex::leaves(std::vector<Row> rows){
  Refs refs;
  if(rows.empty())return refs;
  const auto partitions=groups(rows.size());
  const auto width=(rows.size()+partitions-1)/partitions;
  Page current;std::size_t bytes=0;
  for(auto& row:rows){
    const auto added=wire_bound(row);
    if(!current.rows.empty()&&(current.rows.size()==width||added>leaf_bytes-std::min(bytes,leaf_bytes))){
      refs.push_back(make(std::move(current)));current={};bytes=0;
    }
    bytes+=added;current.rows.emplace_back(std::move(row));
  }
  if(!current.rows.empty())refs.push_back(make(std::move(current)));
  return refs;
}
PackedIndex::Refs PackedIndex::branches(const Refs& children){
  Refs refs;
  if(children.empty())return refs;
  const auto partitions=groups(children.size());
  const auto width=(children.size()+partitions-1)/partitions;
  for(std::size_t start=0;start<children.size();start+=width){
    Page branch;
    const auto end=std::min(children.size(),start+width);
    branch.children.assign(children.begin()+static_cast<std::ptrdiff_t>(start),children.begin()+static_cast<std::ptrdiff_t>(end));
    refs.push_back(make(std::move(branch)));
  }
  return refs;
}
PackedIndex::Refs PackedIndex::update(const Page& old,Iter first_update,Iter last_update){
  if(first_update==last_update)return {old.ref};
  Refs replacements;
  if(old.children.empty()){
    Updates merged(old.rows.begin(),old.rows.end());
    for(auto it=first_update;it!=last_update;++it)merged[it->first]=it->second;
    replacements=leaves({merged.begin(),merged.end()});
  }else{
    Refs children;
    for(std::size_t i=0;i<old.children.size();++i){
      const auto& child=old.children[i];
      auto until=first_update;
      while(until!=last_update&&(i+1==old.children.size()||until->first<old.children[i+1].first))++until;
      if(first_update==until)children.push_back(child);
      else{
        const auto changed=update(checked(child),first_update,until);
        children.insert(children.end(),changed.begin(),changed.end());
      }
      first_update=until;
    }
    replacements=branches(children);
  }
  retired_.insert(old.ref.digest);
  return replacements;
}
Digest PackedIndex::prepare(const Digest& root,const Updates& changes){
  if(!writes_.empty()||!retired_.empty())throw Failure(Code::conflict,"packed publication already staged");
  if(changes.empty())return root;
  if(changes.begin()->first.empty())throw Failure(Code::invalid,"empty packed index key");
  Refs next=root.empty()?leaves({changes.begin(),changes.end()}):update(read(root),changes.begin(),changes.end());
  while(next.size()>1)next=branches(next);
  for(const auto& item:writes_)retired_.erase(item.first);
  return next.front().digest;
}
std::uint64_t PackedIndex::work_bytes() const{
  // Staging charge includes per-page bookkeeping. Reserve a second copy of
  // the largest temporary write and the bounded retirement journal.
  std::uint64_t largest=0;
  for(const auto& item:writes_)largest=std::max(largest,static_cast<std::uint64_t>(item.second.size()));
  return add(add(write_bytes_,largest),retired_.size()*128U+4096U);
}
std::uint64_t PackedIndex::work_files() const{return writes_.size()+4U;}
void PackedIndex::publish(const Digest& root){
  for(const auto& item:writes_)write(item.first,item.second);
  std::vector<std::string> entries{root};
  entries.insert(entries.end(),retired_.begin(),retired_.end());
  write_file(directory_/"GC",seal(gc_schema,entries));
}
void PackedIndex::finish(const Digest& root){
  clear();
  const auto journal=directory_/"GC";
  if(!regular_exists(journal))return;
  const auto entries=unseal(read_file(journal,64U*1024U*1024U),gc_schema);
  require(!entries.empty(),"empty packed cleanup journal");
  if(entries.front()!=root)return;
  for(auto it=std::next(entries.begin());it!=entries.end();++it){
    require(valid_digest(*it),"invalid packed cleanup identity");
    remove_file(directory_/(*it+".page"));
  }
  remove_file(journal);
}
std::string PackedIndex::get(const Digest& root,std::string_view key,bool& found){
  ++stats_.point_queries;found=false;
  if(root.empty())return {};
  auto page=read(root);
  for(;;){
    if(key<page.ref.first||key>page.ref.last)return {};
    if(page.children.empty()){
      const auto it=std::lower_bound(page.rows.begin(),page.rows.end(),key,[](const Row& row,std::string_view needle){return row.first<needle;});
      if(it==page.rows.end()||it->first!=key)return {};
      found=true;return it->second;
    }
    const auto it=std::lower_bound(page.children.begin(),page.children.end(),key,[](const Ref& child,std::string_view needle){return child.last<needle;});
    if(it==page.children.end()||key<it->first)return {};
    page=checked(*it);
  }
}
bool PackedIndex::collect(const Page& page,std::string_view prefix,std::string_view after,std::size_t limit,std::vector<Row>& out){
  if(!intersects(page.ref.first,page.ref.last,prefix,after))return false;
  for(const auto& row:page.rows){
    if(!row.first.starts_with(prefix)||(!after.empty()&&row.first<=after))continue;
    if(out.size()==limit)return true;
    out.push_back(row);
  }
  for(const auto& child:page.children){
    if(intersects(child.first,child.last,prefix,after)&&collect(checked(child),prefix,after,limit,out))return true;
  }
  return false;
}
void PackedIndex::scan(const Digest& root,std::string_view prefix,std::string_view after,std::size_t limit,std::vector<Row>& out,bool& more){
  ++stats_.range_queries;out.clear();more=!root.empty()&&collect(read(root),prefix,after,limit,out);
}
void PackedIndex::verify(const Page& page){
  for(const auto& child:page.children)verify(checked(child));
}
void PackedIndex::audit(const Digest& root){
  clear();
  if(!root.empty())verify(read(root));
}
Digest PackedIndex::build_sorted(std::uint64_t count,const RowReader& row_at){
  if(!fs::is_empty(directory_)||caching_)throw Failure(Code::conflict,"bulk packed construction requires an empty unpublished directory");
  bulk_=true;
  struct Reset { bool& flag;~Reset(){flag=false;} } reset{bulk_};
  std::vector<Refs> levels;
  const auto append=[&](auto&& self,std::size_t level,const Ref& ref)->void{
    if(level>=96)throw Failure(Code::capacity,"packed bulk build height exceeded");
    if(levels.size()<=level)levels.resize(level+1);
    levels[level].push_back(ref);
    if(levels[level].size()==fanout){
      Page branch;branch.children.swap(levels[level]);const auto parent=make(std::move(branch));self(self,level+1,parent);
    }
  };
  Page leaf;std::size_t bytes=0;std::string previous;
  const auto flush=[&]{
    if(leaf.rows.empty())return;
    append(append,0,make(std::move(leaf)));leaf={};bytes=0;
  };
  for(std::uint64_t n=0;n<count;++n){
    auto row=row_at(n);require(!row.first.empty()&&(n==0||row.first>previous),"packed base rows are not strictly ordered");
    previous=row.first;
    const auto size=wire_bound(row);
    if(!leaf.rows.empty()&&(leaf.rows.size()==fanout||size>leaf_bytes-std::min(bytes,leaf_bytes)))flush();
    bytes+=size;leaf.rows.push_back(std::move(row));
  }
  flush();
  for(std::size_t level=0;level<levels.size();++level){
    if(levels[level].empty())continue;
    if(level+1==levels.size()&&levels[level].size()==1){
      const auto digest=levels[level].front().digest;audit(digest);return digest;
    }
    Page branch;branch.children.swap(levels[level]);append(append,level+1,make(std::move(branch)));
  }
  return {};
}
}

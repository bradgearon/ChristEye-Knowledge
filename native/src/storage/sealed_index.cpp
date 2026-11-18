#include "sealed_index.hpp"
#include "christeye/living/durable_io.hpp"
#include <charconv>
#include <fstream>
#include <limits>

namespace anaiero::storage {
namespace os=christeye::living::durable::os;
bool regular_exists(const fs::path& p){
  std::error_code ec;
  const auto status=fs::symlink_status(p,ec);
  if(ec==std::errc::no_such_file_or_directory)return false;
  if(ec)throw Failure(Code::io_error,"cannot inspect storage path");
  if(fs::is_symlink(status))throw Failure(Code::corrupt,"storage symlink rejected");
  if(!fs::exists(status))return false;
  if(!fs::is_regular_file(status))throw Failure(Code::corrupt,"storage object is not a regular file");
  return true;
}
Bytes read_file(const fs::path& p,std::size_t maximum){
  if(!regular_exists(p))throw Failure(Code::not_found,"storage object absent");
  std::error_code ec;const auto size=fs::file_size(p,ec);
  if(ec)throw Failure(Code::io_error,"cannot size storage object");
  if(size>maximum)throw Failure(Code::corrupt,"storage object exceeds bounded decoder");
  std::ifstream in(p,std::ios::binary);if(!in)throw Failure(Code::io_error,"cannot open storage object");
  Bytes bytes(static_cast<std::size_t>(size),'\0');
  if(size!=0&&!in.read(bytes.data(),static_cast<std::streamsize>(size)))throw Failure(Code::io_error,"short storage read");
  return bytes;
}
void write_file(const fs::path& p,std::string_view bytes){
  (void)regular_exists(p);
  if(!os::atomic_write(p,bytes))throw Failure(Code::io_error,"durable storage write failed");
}
void remove_file(const fs::path& p){
  if(!regular_exists(p))return;
  std::error_code ec;if(!fs::remove(p,ec)||ec)throw Failure(Code::io_error,"storage removal failed");
  if(!os::sync_directory(p.parent_path()))throw Failure(Code::io_error,"storage removal flush failed");
}
std::string number(std::uint64_t v){
  constexpr char hex[]="0123456789abcdef";std::string s(16,'0');
  for(std::size_t i=0;i<16;++i){s[15-i]=hex[v&15U];v>>=4U;}return s;
}
std::uint64_t parse_number(std::string_view s){
  if(s.size()!=16)throw Failure(Code::corrupt,"invalid canonical integer");
  std::uint64_t v=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),v,16);
  if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||number(v)!=s)throw Failure(Code::corrupt,"invalid canonical integer");
  return v;
}
Bytes seal(std::string_view schema,const std::vector<std::string>& fields){
  const auto body=pack(fields);return pack({std::string(schema),body,sha256(body)});
}
std::vector<std::string> unseal(std::string_view data,std::string_view schema,std::size_t max_fields,std::size_t max_bytes){
  std::vector<std::string> envelope,fields;
  if(!unpack(data,envelope,3,max_bytes)||envelope.size()!=3||envelope[0]!=schema||!valid_digest(envelope[2])||sha256(envelope[1])!=envelope[2])throw Failure(Code::corrupt,"invalid sealed storage envelope");
  if(!unpack(envelope[1],fields,max_fields,max_bytes))throw Failure(Code::corrupt,"invalid canonical storage fields");
  return fields;
}
SealedIndex::Node SealedIndex::load(const Digest& hash){
  if(hash.empty())return Node{"","","","",0,0,0,0};
  if(!valid_digest(hash))throw Failure(Code::corrupt,"invalid index node identity");
  const auto cached=cache_.find(hash);if(cached!=cache_.end())return cached->second;
  ++stats.nodes_read;
  Bytes raw;try{raw=read_file(directory_/hash,32U*1024U*1024U);}catch(const Failure& e){if(e.code==Code::not_found)throw Failure(Code::corrupt,"sealed index node is missing");throw;}
  if(sha256(raw)!=hash)throw Failure(Code::corrupt,"index node digest mismatch");
  const auto f=unseal(raw,"ANAIERO-INDEX-NODE/2",8);
  if(f.size()!=8)throw Failure(Code::corrupt,"invalid index node");
  Node n{f[0],f[1],f[2],f[3],parse_number(f[4]),parse_number(f[5]),parse_number(f[6]),parse_number(f[7])};
  if(n.key.empty()||(!n.left.empty()&&!valid_digest(n.left))||(!n.right.empty()&&!valid_digest(n.right))||n.height==0||n.height>96||n.count==0||n.bytes<raw.size()||n.maximum_node_bytes<raw.size())throw Failure(Code::corrupt,"invalid index node metadata");
  cache(hash,n);
  return n;
}
void SealedIndex::cache(const Digest& hash,const Node& n){
  constexpr std::size_t maximum_cache_bytes=32U*1024U*1024U;
  const auto bytes=n.key.size()+n.value.size()+512U;
  if(caching_&&cache_.size()<8192&&cache_.count(hash)==0&&bytes<=maximum_cache_bytes-cache_bytes_){cache_.emplace(hash,n);cache_bytes_+=bytes;stats.cache_peak_nodes=std::max(stats.cache_peak_nodes,static_cast<std::uint64_t>(cache_.size()));stats.cache_peak_bytes=std::max(stats.cache_peak_bytes,static_cast<std::uint64_t>(cache_bytes_));}
}
void SealedIndex::discard_cache()noexcept{cache_.clear();cache_bytes_=0;caching_=false;obsolete_.clear();pending_.clear();}
void SealedIndex::begin(){discard_cache();caching_=true;}
std::uint64_t SealedIndex::height(const Digest& d){return load(d).height;}
void SealedIndex::retire(const Digest& d){if(!d.empty())obsolete_.insert(d);}
Digest SealedIndex::save(Node n){
  const auto l=load(n.left),r=load(n.right);
  n.height=1+std::max(l.height,r.height);n.count=1+l.count+r.count;
  n.bytes=0;
  // Fixed-width integer/digest fields make this exact size computable without
  // hashing the temporary zero-counter envelope a second time.
  const auto body=pack({n.key,n.value,n.left,n.right,number(n.height),number(n.count),number(0),number(0)});
  const auto own=pack({"ANAIERO-INDEX-NODE/2",body,std::string(64,'0')}).size();
  n.bytes=static_cast<std::uint64_t>(own)+l.bytes+r.bytes;
  n.maximum_node_bytes=std::max({static_cast<std::uint64_t>(own),l.maximum_node_bytes,r.maximum_node_bytes});
  const auto raw=seal("ANAIERO-INDEX-NODE/2",{n.key,n.value,n.left,n.right,number(n.height),number(n.count),number(n.bytes),number(n.maximum_node_bytes)});
  const auto digest=sha256(raw);const auto cached=cache_.find(digest);
  if(cached!=cache_.end()){
    const auto& previous=cached->second;
    if(previous.key!=n.key||previous.value!=n.value||previous.left!=n.left||previous.right!=n.right||previous.height!=n.height||previous.count!=n.count||previous.bytes!=n.bytes||previous.maximum_node_bytes!=n.maximum_node_bytes)throw Failure(Code::corrupt,"index cached CAS collision");
    return digest;
  }
  cache(digest,n);
  if(cache_.count(digest)!=0){pending_.insert(digest);++stats.staged_nodes;return digest;}
  const auto p=directory_/digest;
  if(regular_exists(p)){if(read_file(p,32U*1024U*1024U)!=raw)throw Failure(Code::corrupt,"index CAS collision");return digest;}
  // The same fixed cache budget includes read nodes and every staged path copy.
  // If it fills, spill this reconstructible node using the original write path.
  os::File file;if(!file.open(p,true)||!file.write_all(raw))throw Failure(Code::io_error,"index cache spill failed");file.close();++stats.nodes_written;++stats.staged_spills;return digest;
}

Digest SealedIndex::rotate_left(Node n){
  Node right=load(n.right);retire(n.right);n.right=right.left;right.left=save(std::move(n));return save(std::move(right));
}
Digest SealedIndex::rotate_right(Node n){
  Node left=load(n.left);retire(n.left);n.left=left.right;left.right=save(std::move(n));return save(std::move(left));
}
Digest SealedIndex::balance(Node n){
  const auto lh=height(n.left),rh=height(n.right);
  if(lh>rh+1){const auto left=load(n.left);if(height(left.right)>height(left.left)){retire(n.left);n.left=rotate_left(left);}return rotate_right(std::move(n));}
  if(rh>lh+1){const auto right=load(n.right);if(height(right.left)>height(right.right)){retire(n.right);n.right=rotate_right(right);}return rotate_left(std::move(n));}
  return save(std::move(n));
}
Digest SealedIndex::insert(const Digest& root,const std::string& key,const std::string& value){
  if(root.empty())return save(Node{key,value,"","",1,1,0,0});
  auto node=load(root);
  if(key==node.key){if(value==node.value)return root;retire(root);node.value=value;return save(std::move(node));}
  retire(root);if(key<node.key)node.left=insert(node.left,key,value);else node.right=insert(node.right,key,value);
  return balance(std::move(node));
}
Digest SealedIndex::put(const Digest& root,std::string key,std::string value){return insert(root,key,value);}
std::string SealedIndex::get(const Digest& root,std::string_view key,bool& found){
  ++stats.point_queries;auto cursor=root;std::size_t depth=0;found=false;
  while(!cursor.empty()){if(++depth>96)throw Failure(Code::corrupt,"index depth exceeded");const auto node=load(cursor);if(key==node.key){found=true;return node.value;}cursor=key<node.key?node.left:node.right;}return {};
}
Digest SealedIndex::node_at(const Digest& root,std::string_view key){
  auto cursor=root;std::size_t depth=0;
  while(!cursor.empty()){if(++depth>96)throw Failure(Code::corrupt,"index depth exceeded");const auto n=load(cursor);if(key==n.key)return cursor;cursor=key<n.key?n.left:n.right;}return {};
}
void SealedIndex::scan(const Digest& root,std::string_view prefix,std::string_view after,std::size_t limit,std::vector<std::pair<std::string,std::string>>& out,bool& more){
  ++stats.range_queries;out.clear();more=false;std::vector<Node> stack;auto cursor=root;
  const std::string lower=std::string(after)>std::string(prefix)?std::string(after):std::string(prefix);
  while(!cursor.empty()){auto n=load(cursor);if(n.key>=lower){stack.push_back(n);cursor=n.left;}else cursor=n.right;if(stack.size()>96)throw Failure(Code::corrupt,"index range depth exceeded");}
  while(!stack.empty()){
    auto n=std::move(stack.back());stack.pop_back();
    if(n.key.compare(0,prefix.size(),prefix)!=0){if(n.key>prefix)break;}
    else if(after.empty()||n.key>after){if(out.size()==limit){more=true;break;}out.emplace_back(n.key,n.value);}
    cursor=n.right;while(!cursor.empty()){auto child=load(cursor);stack.push_back(child);cursor=child.left;if(stack.size()>96)throw Failure(Code::corrupt,"index range depth exceeded");}
  }
}
void SealedIndex::publish_cleanup(const Digest& root){
  // Many path copies have the same key. Resolve each final key once within a
  // separate bounded 8 MiB memo, rather than seeking for every transient version.
  std::map<std::string,Digest> final_nodes;std::size_t memo_bytes=0;
  const auto current=[&](const Node& n){const auto found=final_nodes.find(n.key);if(found!=final_nodes.end())return found->second;const auto final=node_at(root,n.key);const auto bytes=n.key.size()+final.size()+128U;if(final_nodes.size()<8192&&bytes<=8U*1024U*1024U-memo_bytes){final_nodes.emplace(n.key,final);memo_bytes+=bytes;}return final;};
  // Staging does not inspect the filesystem for every temporary path copy.
  // An existing live node must have been loaded before it is replaced, so it
  // is already in the non-evicting admission cache (or uses the spill path).
  // Check an existing destination's exact bytes only if the staged node is
  // reachable at publication. This also reconciles an interrupted publication.
  // No transient staged node reaches the filesystem. All reachable staged
  // nodes are written before directory flush and coverage publication; a crash
  // still repairs the cache from the already durable canonical transaction.
  for(const auto& digest:pending_){const auto& n=cache_.at(digest);if(current(n)!=digest){++stats.transient_nodes_elided;continue;}const auto raw=seal("ANAIERO-INDEX-NODE/2",{n.key,n.value,n.left,n.right,number(n.height),number(n.count),number(n.bytes),number(n.maximum_node_bytes)});const auto path=directory_/digest;if(regular_exists(path)){if(read_file(path,32U*1024U*1024U)!=raw)throw Failure(Code::corrupt,"staged index CAS collision");}else {os::File file;if(!file.open(path,true)||!file.write_all(raw))throw Failure(Code::io_error,"staged index cache write failed");file.close();++stats.nodes_written;}}
  std::vector<std::string> chunks{root},dead;
  for(const auto& d:obsolete_){
    // A staged identity was not written by this update. Any live predecessor
    // was loaded before replacement and was therefore not staged a second
    // time. Current pending identities must remain, even after rotations.
    if(pending_.count(d)!=0)continue;
    const auto n=load(d);if(current(n)!=d)dead.push_back(d);
    if(dead.size()==4096){chunks.push_back(pack(dead));dead.clear();}
  }
  if(!dead.empty())chunks.push_back(pack(dead));
  const auto sealed=seal("ANAIERO-INDEX-GC/2",chunks);
  if(sealed.empty()||sealed.size()>64U*1024U*1024U)throw Failure(Code::capacity,"bounded index cleanup manifest exhausted");
  if(!os::sync_directory(directory_))throw Failure(Code::io_error,"index cache directory flush failed");
  write_file(directory_.parent_path()/"GC",sealed);
}

void SealedIndex::finish_cleanup(const Digest& root){
  const auto gc=directory_.parent_path()/"GC";
  if(!regular_exists(gc)){discard_cache();return;}
  const auto fields=unseal(read_file(gc,64U*1024U*1024U),"ANAIERO-INDEX-GC/2");
  if(fields.empty()||fields[0]!=root)return; // Unpublished update; replay replaces it.
  for(std::size_t i=1;i<fields.size();++i){std::vector<std::string> dead;if(!unpack(fields[i],dead,4096,1024U*1024U))throw Failure(Code::corrupt,"bad index GC chunk");for(const auto& d:dead){if(!valid_digest(d))throw Failure(Code::corrupt,"bad index GC identity");const auto path=directory_/d;if(regular_exists(path)){std::error_code error;if(!fs::remove(path,error)||error)throw Failure(Code::io_error,"index cache cleanup failed");}}}
  if(!os::sync_directory(directory_))throw Failure(Code::io_error,"index cache cleanup flush failed");
  remove_file(gc);discard_cache();
}
void SealedIndex::audit_node(const Digest& d,const std::string* low,const std::string* high,std::size_t depth){
  if(d.empty())return;
  if(depth>96)throw Failure(Code::corrupt,"invalid index cycle/depth");
  const auto n=load(d),l=load(n.left),r=load(n.right);
  if((low&&n.key<=*low)||(high&&n.key>=*high)||n.height!=1+std::max(l.height,r.height)||n.count!=1+l.count+r.count||l.height>r.height+1||r.height>l.height+1)throw Failure(Code::corrupt,"index ordering/aggregate violation");
  const auto own=fs::file_size(directory_/d);
  if(n.bytes!=own+l.bytes+r.bytes||n.maximum_node_bytes!=std::max({own,l.maximum_node_bytes,r.maximum_node_bytes}))throw Failure(Code::corrupt,"index size aggregate violation");
  audit_node(n.left,low,&n.key,depth+1);audit_node(n.right,&n.key,high,depth+1);
}
void SealedIndex::audit(const Digest& root){discard_cache();audit_node(root,nullptr,nullptr,0);}
}

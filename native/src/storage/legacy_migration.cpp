#include "anaiero/sdk/storage_tools.hpp"
#include "sealed_index.hpp"
#include "christeye/living/durable_registry.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace anaiero::storage {
namespace legacy=christeye::living::durable;
namespace os=legacy::os;
using namespace sdk::evidence;
namespace {
constexpr std::size_t maximum_source_files=65536;
constexpr std::size_t maximum_source_file_bytes=64U*1024U*1024U;
struct SourceFile {std::string relative;std::uint64_t bytes=0;Digest digest;};
struct Inventory {std::vector<SourceFile> files;std::uint64_t bytes=0;Digest digest;};
std::string utf8(const fs::path& path){const auto value=path.generic_u8string();return {reinterpret_cast<const char*>(value.data()),value.size()};}
fs::path path_of(std::string_view value){if(value.empty()||value.find('\0')!=std::string_view::npos)throw Failure(Code::invalid,"invalid migration path");return fs::path(std::u8string(reinterpret_cast<const char8_t*>(value.data()),value.size()));}
void require(Result r){if(!r)throw Failure(r.code,r.message);}
void make_directory(const fs::path& path){std::error_code ec;if(fs::is_symlink(path,ec))throw Failure(Code::corrupt,"migration directory symlink rejected");ec.clear();fs::create_directories(path,ec);if(ec||!fs::is_directory(path))throw Failure(Code::io_error,"cannot create migration staging directory");}
bool contains(const fs::path& parent,const fs::path& child){const auto relative=child.lexically_relative(parent);if(relative.empty()||relative==".")return true;if(relative.is_absolute())return false;for(const auto& part:relative)if(part=="..")return false;return true;}
Inventory inventory(const fs::path& root,std::uint64_t byte_limit){
  Inventory result;std::error_code ec;
  if(fs::is_symlink(root,ec)||!fs::is_directory(root))throw Failure(Code::invalid,"migration source is not a real directory");
  for(const auto& entry:fs::recursive_directory_iterator(root)){
    const auto status=entry.symlink_status();if(fs::is_symlink(status))throw Failure(Code::corrupt,"source snapshot contains a symlink");if(fs::is_directory(status))continue;if(!fs::is_regular_file(status))throw Failure(Code::corrupt,"source snapshot contains a nonregular file");
    const auto relative=utf8(entry.path().lexically_relative(root));if(relative.empty()||relative.size()>1024||result.files.size()==maximum_source_files)throw Failure(Code::capacity,"legacy migration file/path bound exceeded (65536 files, 1024 path bytes)");
    const auto size=entry.file_size();if(size>maximum_source_file_bytes||result.bytes>byte_limit||size>byte_limit-result.bytes)throw Failure(Code::capacity,"legacy snapshot byte budget exceeded");
    const auto raw=read_file(entry.path(),maximum_source_file_bytes);result.bytes+=size;result.files.push_back({relative,size,sha256(raw)});
  }
  std::sort(result.files.begin(),result.files.end(),[](const SourceFile& a,const SourceFile& b){return a.relative<b.relative;});
  Digest chain=sha256("ANAIERO-LEGACY-SOURCE/1");for(const auto& file:result.files)chain=sha256(pack({chain,file.relative,number(file.bytes),file.digest}));result.digest=chain;return result;
}
void copy_snapshot(const fs::path& source,const fs::path& target,const Inventory& inventory){
  make_directory(target);
  for(const auto& file:inventory.files){const auto relative=path_of(file.relative);const auto destination=target/relative;make_directory(destination.parent_path());const auto raw=read_file(source/relative,maximum_source_file_bytes);if(raw.size()!=file.bytes||sha256(raw)!=file.digest)throw Failure(Code::conflict,"quiescent legacy source changed while copying");
    if(regular_exists(destination)){if(read_file(destination,maximum_source_file_bytes)!=raw)throw Failure(Code::corrupt,"resumable snapshot file differs from pinned source");}
    else write_file(destination,raw);
  }
  if(!os::sync_directory(target))throw Failure(Code::io_error,"snapshot directory flush failed");
}
void no_replace_directory(const fs::path& from,const fs::path& to){
#ifdef _WIN32
  if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_WRITE_THROUGH))throw Failure(GetLastError()==ERROR_ALREADY_EXISTS||GetLastError()==ERROR_FILE_EXISTS?Code::conflict:Code::io_error,"migration activation rename failed");
#else
  if(::syscall(SYS_renameat2,AT_FDCWD,from.c_str(),AT_FDCWD,to.c_str(),1U)!=0)throw Failure(errno==EEXIST?Code::conflict:Code::io_error,"migration no-replace activation rename failed");
#endif
  if(!os::sync_directory(to.parent_path())||!os::sync_directory(from.parent_path()))throw Failure(Code::io_error,"migration activation directory flush failed");
}
Bytes migration_manifest(const std::string& source,const std::string& destination,const Inventory& inventory){return seal("ANAIERO-LEGACY-MIGRATION/1",{source,destination,inventory.digest,number(inventory.files.size()),number(inventory.bytes)});}
void commit_batch(ICommitStore& store,CommitBatch& batch){CommitReceipt receipt;const auto result=store.commit(batch,receipt);if(result.code==Code::committed_degraded){require(store.repair_index());require(store.lookup_operation(batch.operation_id,receipt));return;}require(result);}
CommitBatch operation(std::string id,std::string_view request){CommitBatch batch;batch.operation_id=std::move(id);batch.request_digest=sha256(request);return batch;}
std::vector<fs::path> journal_paths(const fs::path& recovery,legacy::Registry& registry){
  if(!registry.is_segmented())return {recovery/"events.log"};
  const auto raw=read_file(recovery/"MANIFEST",16384);std::istringstream input(raw);std::string line;std::uint64_t active=0,archived=0;
  while(std::getline(input,line))if(line.starts_with("root ")){std::istringstream row(line);std::string tag,extra;std::uint64_t generation=0,first=0,offset=0,sequence=0;if(!(row>>tag>>generation>>active>>first>>offset>>sequence>>archived)||(row>>extra)||active==0||archived>=active||active>maximum_source_files)throw Failure(Code::corrupt,"invalid verified legacy segment catalog");break;}
  if(active==0)throw Failure(Code::corrupt,"legacy segment root absent");
  std::vector<fs::path> paths;paths.reserve(static_cast<std::size_t>(active));
  for(std::uint64_t id=1;id<=active;++id){std::ostringstream name;name<<std::setw(20)<<std::setfill('0')<<id<<".log";paths.push_back(recovery/(id<=archived?"archive":"segments")/name.str());}return paths;
}
class EventWires {
  std::vector<fs::path> paths_;std::size_t next_=0;std::ifstream current_;
public:
  explicit EventWires(std::vector<fs::path> paths):paths_(std::move(paths)){}
  std::string next(const legacy::Event& expected){
    for(;;){
      if(!current_.is_open()){if(next_==paths_.size())throw Failure(Code::corrupt,"original event wire missing");current_.open(paths_[next_++],std::ios::binary);if(!current_)throw Failure(Code::io_error,"cannot read original event wire");}
      std::string line;if(!std::getline(current_,line)){if(!current_.eof())throw Failure(Code::io_error,"original event wire read failed");current_.close();continue;}
      if(line.size()>8192)throw Failure(Code::corrupt,"legacy event line exceeds bounded wire size");
      legacy::Event parsed;
      if(!legacy::parse_event(line,parsed))continue; // Verified segment/header lines.
      if(legacy::serialize_event(parsed)!=legacy::serialize_event(expected))throw Failure(Code::corrupt,"legacy event wire order differs from verified traversal");
      return line+'\n';
    }
  }
};
void report_completed(ICommitStore& store,const Inventory& inventory,const fs::path& backup,MigrationReport& report){
  StoredRecord complete;require(store.latest("migration","legacy",complete));std::vector<std::string> values;require(unpack(complete.record.payload,values,8));if(values.size()!=8||values[0]!="legacy-migration/1"||values[2]!=inventory.digest||values[3]!=number(inventory.files.size())||values[4]!=number(inventory.bytes))throw Failure(Code::conflict,"destination belongs to another pinned legacy source");
  if(inventory.files.size()!=report.source_files)throw Failure(Code::corrupt,"migration source count changed");
  report.events=parse_number(values[5]);report.objects=parse_number(values[6]);report.backup_directory=utf8(backup);report.retained_backup_bytes=inventory.bytes;report.destination_root=store.info().root_digest;report.target_bytes=store.info().physical_bytes;
}
}

Result migrate_legacy(std::string_view source_value,std::string_view destination_value,const StoreOptions& options,MigrationReport& report){
  report={};
  try{
    if(options.maximum_bytes<65536)throw Failure(Code::invalid,"invalid migration target byte quota");
    const auto source=fs::canonical(path_of(source_value));const auto destination=fs::weakly_canonical(fs::absolute(path_of(destination_value))).lexically_normal();
    if(contains(source,destination)||contains(destination,source))throw Failure(Code::invalid,"legacy source and destination must be separate directory trees");
    if(!regular_exists(source/"events.log")&&!regular_exists(source/"MANIFEST"))throw Failure(Code::invalid,"source is not a supported legacy registry");
    if(!regular_exists(source/"writer.lock"))throw Failure(Code::busy,"legacy writer lock is absent; quiescent migration requires an initialized registry");
    auto stage=destination;stage+=".migration";auto backup=destination;backup+=".legacy-source";auto lock_path=destination;lock_path+=".migration.lock";
    if(contains(source,stage)||contains(stage,source)||contains(source,backup)||contains(backup,source))throw Failure(Code::invalid,"migration staging/snapshot overlaps the original source");
    make_directory(destination.parent_path());
    (void)regular_exists(lock_path);os::File migration_lock;if(!migration_lock.open(lock_path)||!migration_lock.lock_exclusive())throw Failure(Code::busy,"another migration owns this destination");
    os::File source_lock;if(!source_lock.open(source/"writer.lock")||!source_lock.lock_exclusive())throw Failure(Code::busy,"legacy registry is active; stop its writer before migration");
    const auto source_inventory=inventory(source,options.maximum_bytes);report.source_fingerprint=source_inventory.digest;report.source_files=source_inventory.files.size();report.source_bytes=source_inventory.bytes;report.backup_directory=utf8(backup);
    const auto file_reserve=static_cast<std::uint64_t>(source_inventory.files.size())*65536U;
    if(source_inventory.bytes>std::numeric_limits<std::uint64_t>::max()-file_reserve)throw Failure(Code::capacity,"migration snapshot capacity arithmetic overflow");
    const auto snapshot_reserve=source_inventory.bytes+file_reserve;
    if(options.maximum_bytes>std::numeric_limits<std::uint64_t>::max()-options.minimum_free_bytes||snapshot_reserve>(std::numeric_limits<std::uint64_t>::max()-options.maximum_bytes-options.minimum_free_bytes)/2U)throw Failure(Code::capacity,"migration capacity arithmetic overflow");
    report.peak_required_bytes=snapshot_reserve*2U+options.maximum_bytes+options.minimum_free_bytes;
    const auto marker=migration_manifest(utf8(source),utf8(destination),source_inventory);
    if(fs::exists(destination)){
      if(!regular_exists(destination/"LEGACY_MIGRATION")||read_file(destination/"LEGACY_MIGRATION",16384)!=marker)throw Failure(Code::conflict,"destination already exists and is not this completed legacy migration");
      std::unique_ptr<ICommitStore> existing;require(open_native_store(utf8(destination),options,existing));if(!existing->info().index_ready)require(existing->repair_index());AuditReport audit;require(existing->audit(audit));report_completed(*existing,source_inventory,backup,report);
      if(inventory(backup,options.maximum_bytes).digest!=source_inventory.digest)throw Failure(Code::corrupt,"retained legacy source snapshot is missing or changed");
      report.already_complete=true;report.activated=true;report.source_unchanged=true;return {};
    }
    const auto available=fs::space(destination.parent_path()).available;if(available<report.peak_required_bytes)throw Failure(Code::capacity,"migration requires capacity for target quota plus two bounded source snapshots");
    if(fs::exists(stage)){if(!regular_exists(stage/"MIGRATION")||read_file(stage/"MIGRATION",16384)!=marker)throw Failure(Code::conflict,"existing migration staging does not match the pinned source");report.resumed=true;}else{make_directory(stage);write_file(stage/"MIGRATION",marker);}
    const bool backup_ready=fs::exists(backup);const auto snapshot=backup_ready?backup:stage/"snapshot";
    if(backup_ready){if(inventory(backup,options.maximum_bytes).digest!=source_inventory.digest)throw Failure(Code::conflict,"existing source snapshot has another identity");}
    else {copy_snapshot(source,snapshot,source_inventory);if(inventory(snapshot,options.maximum_bytes).digest!=source_inventory.digest)throw Failure(Code::corrupt,"private source snapshot verification failed");}
    const auto recovery=stage/"recovery";if(fs::is_symlink(recovery))throw Failure(Code::corrupt,"recovery clone symlink rejected");std::error_code cleanup_error;fs::remove_all(recovery,cleanup_error);if(cleanup_error)throw Failure(Code::io_error,"cannot reset private recovery clone");copy_snapshot(snapshot,recovery,source_inventory);
    legacy::Registry registry;const auto legacy_open=registry.open(recovery);if(legacy_open!=legacy::Code::ok||!registry.audit_disk())throw Failure(Code::corrupt,"private legacy recovery/full audit failed");
    std::unique_ptr<ICommitStore> target;require(open_native_store(utf8(stage/"store"),options,target));if(!target->info().index_ready)require(target->repair_index());
    auto start=operation("legacy-start/"+source_inventory.digest,marker);start.guards={{"migration","source",0,{}}};start.records={{"migration","source",marker,{},false}};commit_batch(*target,start);
    std::set<Digest> imported;
    for(const auto& file:source_inventory.files){
      const auto relative=path_of(file.relative);const auto parent=utf8(relative.parent_path());if(parent!="objects"&&parent!="archive_objects")continue;const auto digest=utf8(relative.filename());if(digest.ends_with(".tmp"))continue;if(!valid_digest(digest)||digest!=file.digest)throw Failure(Code::corrupt,"legacy CAS object filename/content digest mismatch");if(!imported.insert(digest).second)continue;
      std::string bytes;if(!registry.get(digest,bytes)||sha256(bytes)!=digest)throw Failure(Code::corrupt,"verified legacy CAS object unavailable");if(bytes.size()>options.maximum_blob_bytes)throw Failure(Code::capacity,"legacy object exceeds target blob capacity");
      const auto provenance=pack({"legacy-object/1",source_inventory.digest,digest,number(bytes.size())});auto object=operation("legacy-object/"+source_inventory.digest+'/'+digest,provenance);object.blobs={{digest,std::move(bytes),{}}};object.records={{"legacy.object",digest,provenance,{digest},false}};object.guards={{"legacy.object",digest,0,{}}};commit_batch(*target,object);
    }
    EventWires wires(journal_paths(recovery,registry));Result event_result;std::uint64_t events=0;
    const bool visited=registry.visit_events(0,[&](const legacy::Event& event){
      try{const auto raw=wires.next(event);const auto provenance=pack({"legacy-event/1",source_inventory.digest,event.hash,sha256(raw),raw});auto imported_event=operation("legacy-event/"+source_inventory.digest+'/'+number(event.sequence),provenance);imported_event.records={{"legacy.event",number(event.sequence),provenance,{event.object_hash},false}};imported_event.guards={{"legacy.event",number(event.sequence),0,{}}};imported_event.events={{{event.kind,event.entity,event.clock,event.producer,event.source_tick},event.object_hash,event.origin}};commit_batch(*target,imported_event);++events;return true;}
      catch(const Failure& e){event_result={e.code,e.what()};return false;}
    });
    require(event_result);if(!visited||events!=registry.capacity().total_events)throw Failure(Code::corrupt,"legacy history traversal did not cover the complete committed journal");
    const auto completion=pack({"legacy-migration/1",utf8(source),source_inventory.digest,number(source_inventory.files.size()),number(source_inventory.bytes),number(events),number(imported.size()),utf8(backup)});auto finish=operation("legacy-complete/"+source_inventory.digest,completion);finish.guards={{"migration","legacy",0,{}}};finish.records={{"migration","legacy",completion,{},false}};commit_batch(*target,finish);AuditReport audit;require(target->audit(audit));report_completed(*target,source_inventory,backup,report);
    if(inventory(source,options.maximum_bytes).digest!=source_inventory.digest)throw Failure(Code::conflict,"original legacy source changed despite quiescent writer lock");
    report.source_unchanged=true;write_file(stage/"store"/"LEGACY_MIGRATION",marker);target.reset();registry.close();
    if(!backup_ready)no_replace_directory(snapshot,backup);
    report.retained_backup_bytes=source_inventory.bytes;
    // Apply backup protection before exposing the completed canonical target.
    // A permission failure leaves the verified target staged and retryable.
    for(const auto& file:source_inventory.files){std::error_code permission_error;fs::permissions(backup/path_of(file.relative),fs::perms::owner_read,fs::perm_options::replace,permission_error);if(permission_error)throw Failure(Code::io_error,"cannot protect the retained legacy source snapshot");}
    no_replace_directory(stage/"store",destination);report.activated=true;
    fs::remove_all(recovery,cleanup_error);if(!cleanup_error){fs::remove(stage/"MIGRATION",cleanup_error);if(!cleanup_error)fs::remove(stage,cleanup_error);}
    return {};
  }catch(const Failure& e){return {e.code,e.what()};}catch(const std::exception& e){return {Code::io_error,e.what()};}
}
}
namespace anaiero::sdk::evidence {
Result migrate_legacy_evidence(std::string_view source,std::string_view destination,const StoreOptions& options,MigrationReport& report){return storage::migrate_legacy(source,destination,options,report);}
}

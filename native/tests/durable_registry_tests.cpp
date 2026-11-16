#include "christeye/living/durable_registry.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
namespace d=christeye::living::durable;
namespace fs=std::filesystem;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL " #x << " line "<<__LINE__<<'\n';std::exit(1);}} while(false)
struct Sink{
 std::vector<std::uint64_t> items;
 static void on(void* ctx,const d::Event& event) noexcept {
   static_cast<Sink*>(ctx)->items.push_back(event.sequence);
 }
};
static void write_raw(const fs::path& path,std::string_view bytes,bool append=false){
 std::ofstream file(path,std::ios::binary|(append?std::ios::app:std::ios::trunc));
 CHECK(static_cast<bool>(file));file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
 file.close();CHECK(static_cast<bool>(file));
}
static fs::path fresh(){
 const auto path=fs::temp_directory_path()/"christeye-durable-v2-tests";
 std::error_code ec;fs::remove_all(path,ec);fs::create_directories(path);return path;
}
static void baseline(const fs::path& path){
 d::Registry reg;CHECK(reg.open(path)==d::Code::ok);
 d::Registry other;CHECK(other.open(path)==d::Code::locked);
 auto obj=reg.put("CERNA/1\norigin SIMULATED\n");CHECK(obj.code==d::Code::ok);
 CHECK(reg.put("CERNA/1\norigin SIMULATED\n").hash==obj.hash);
 Sink sink;CHECK(reg.subscribe(Sink::on,&sink)==d::Code::ok);
 auto first=reg.publish("RNA_OBSERVATION","b-17",obj.hash,"SIMULATED",4,"sim-clock");
 CHECK(first.code==d::Code::ok&&sink.items.size()==1);
 CHECK(reg.publish("RNA_OBSERVATION","b-17",obj.hash,"SIMULATED",4,"sim-clock").hash==first.hash);
 auto second=reg.publish("LEARNING_RESULT","b-17",obj.hash,"SIMULATED",5,"sim-clock");
 CHECK(second.code==d::Code::ok&&sink.items.size()==2);
 CHECK(reg.audit_disk());
 CHECK(reg.acknowledge("reader-a",1,first.hash)==d::Code::ok);
 CHECK(reg.read_cursor("reader-a").sequence==1);
 CHECK(reg.acknowledge("reader-a",0,d::kZeroDigest)==d::Code::invalid_cursor);
 CHECK(reg.acknowledge("reader-a",2,first.hash)==d::Code::invalid_cursor);
 CHECK(reg.acknowledge("../bad",2,second.hash)==d::Code::invalid_cursor);
 Sink resumed;CHECK(reg.resume(Sink::on,&resumed,"reader-a")==d::Code::ok);
 CHECK(resumed.items.size()==1&&resumed.items[0]==2);
 auto third=reg.publish("SUPPORT_OBSERVATION","b-18",obj.hash,"IMPORTED_UNVERIFIED",6,"import-clock");
 CHECK(third.code==d::Code::ok && resumed.items.size()==2 && resumed.items[1]==3);
 CHECK(reg.acknowledge("reader-a",3,third.hash)==d::Code::ok);
 CHECK(reg.audit_disk());
 reg.close(); CHECK(other.open(path)==d::Code::ok);CHECK(other.event_count()==3);
 CHECK(other.read_cursor("reader-a").sequence==3);
 Sink after_restart;
 CHECK(other.resume(Sink::on,&after_restart,"reader-a")==d::Code::ok);
 CHECK(after_restart.items.empty()); // nothing after acknowledged cursor
 CHECK(other.audit_disk());
}
struct ExamplePlan {
  enum class Err {ok,bad};
  struct Artifact {std::string relative_path,sha256;};
  std::vector<Artifact> files{};
  Err validate() const {return files.size()==1?Err::ok:Err::bad;}
  Err verify_assets(const fs::path& root) const {
    for(const auto& item:files){std::string bytes;
      if(!d::read_bounded(root/item.relative_path,d::kMaxObjectBytes,bytes)||
         christeye::living::plans::sha256_hex(bytes)!=item.sha256)return Err::bad;}
    return Err::ok;
  }
  const auto& artifacts() const {return files;}
  std::string serialize(bool) const {return "CEVPLAN/1\nname verified-test-plan\n";}
  std::string name() const {return "verified-test-plan";}
};
static void plan_commit(const fs::path& path){
 d::Registry r;CHECK(r.open(path)==d::Code::ok);
 const auto root=path/"plan-fixtures";
 fs::create_directories(root);
 write_raw(root/"crystal.txt","CECRYSTAL/1\nSIMULATED\n");
 ExamplePlan fixture{{{"crystal.txt",christeye::living::plans::sha256_hex("CECRYSTAL/1\nSIMULATED\n")}}};
 const auto result=r.register_plan(fixture,root);
 CHECK(result.code==d::Code::ok && r.audit_disk());
 CHECK(r.events().back().kind=="PLAN_REGISTERED");
 std::string content;CHECK(r.get(result.hash,content));
 CHECK(content==fixture.serialize(true));
 CHECK(r.register_plan(fixture,root).hash==result.hash);
 CHECK(r.events().back().sequence==r.event_count());
 fixture.files.front().sha256=std::string(64,'1');
 CHECK(r.register_plan(fixture,root).code==d::Code::invalid_argument);
}
static void simulated_crashes(const fs::path& path){
 d::Registry r;CHECK(r.open(path)==d::Code::ok);
 auto obj=r.put("CELEARN/1\nnext digital lesson\n");CHECK(obj.code==d::Code::ok);
 const auto before=r.event_count();
 r.inject_next_publish(d::FaultSite::after_journal_sync);
 CHECK(r.publish("LEARNING_RESULT","b-22",obj.hash,"SIMULATED",15,"sim-clock").code==d::Code::injected_fault);
 CHECK(!r.is_open());r.close();
 CHECK(r.open(path)==d::Code::ok);CHECK(r.event_count()==before);
 CHECK(r.audit_disk());
 r.inject_next_publish(d::FaultSite::after_head_temp_sync);
 CHECK(r.publish("LEARNING_RESULT","b-22",obj.hash,"SIMULATED",15,"sim-clock").code==d::Code::injected_fault);
 r.close();CHECK(r.open(path)==d::Code::ok);CHECK(r.event_count()==before);
 CHECK(r.audit_disk());
 r.inject_next_publish(d::FaultSite::after_head_commit);
 CHECK(r.publish("LEARNING_RESULT","b-22",obj.hash,"SIMULATED",15,"sim-clock").code==d::Code::injected_fault);
 r.close();CHECK(r.open(path)==d::Code::ok);CHECK(r.event_count()==before+1);
 CHECK(r.audit_disk());
 const auto id=r.events().back().hash;
 CHECK(r.publish("LEARNING_RESULT","b-22",obj.hash,"SIMULATED",15,"sim-clock").hash==id);
 CHECK(r.event_count()==before+1);
}
static void disk_audit(const fs::path& path){
 d::Registry r;CHECK(r.open(path)==d::Code::ok);CHECK(r.audit_disk());
 write_raw(path/"events.log","bad-uncommitted-bytes",true);
 CHECK(!r.audit_disk());
 r.close();CHECK(r.open(path)==d::Code::ok);CHECK(r.audit_disk());
 const auto cursor=path/"cursors"/"reader-a.cursor";
 write_raw(cursor,"tamper");
 CHECK(r.read_cursor("reader-a").code==d::Code::invalid_cursor);
 r.close();
 // A malformed signed HEAD must block opening; never guess a commit point.
 const auto head=path/"HEAD";
 std::string original_head;
 CHECK(d::read_bounded(head,1024,original_head));
 write_raw(head,"CEWHEAD/1\noffset 999\nsequence 1\nlast bad\nseal bad\n");
 CHECK(r.open(path)==d::Code::bad_head);
 write_raw(head,original_head);
 CHECK(r.open(path)==d::Code::ok);r.close();
 // Never discard a committed byte, even when an attacker damages the journal.
 const auto journal=path/"events.log";
 std::fstream corrupt(journal,std::ios::binary|std::ios::in|std::ios::out);
 CHECK(static_cast<bool>(corrupt));corrupt.seekp(20);corrupt.put('x');corrupt.close();
 CHECK(r.open(path)==d::Code::corrupt_journal);
}
static void v1_migration(){
 const auto dir=fresh()/"legacy";fs::create_directories(dir/"objects");
 const std::string raw="CECRYSTAL/1\nsynthetic\n";
 const auto hash=christeye::living::plans::sha256_hex(raw);
 write_raw(dir/"objects"/hash,raw);
 d::Event e{1,std::string(d::kZeroDigest),"CRYSTAL_EVIDENCE","b-17",hash,
            "SIMULATED",0,"sim-clock","legacy-simulator",{}};
 e.hash=d::event_hash(e);
 write_raw(dir/"events.log","CEWREG/1\n"+d::serialize_event(e));
 d::Registry r;CHECK(r.open(dir)==d::Code::ok);CHECK(r.event_count()==1);
 CHECK(fs::exists(dir/"HEAD"));CHECK(r.audit_disk());
 r.close();
 const auto partial=fs::temp_directory_path()/"christeye-durable-v2-malformed-legacy";
 std::error_code ec;fs::remove_all(partial,ec);fs::create_directories(partial/"objects");
 write_raw(partial/"events.log","CEWREG/1\npartial");
 d::Registry rejected;CHECK(rejected.open(partial)==d::Code::corrupt_journal);
 CHECK(!fs::exists(partial/"HEAD"));
 rejected.close();
 fs::remove_all(partial,ec);
}
int main(){
 CHECK(christeye::living::plans::sha256_hex("abc")==
       "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
 const auto path=fresh();baseline(path);plan_commit(path);simulated_crashes(path);disk_audit(path);v1_migration();
 std::error_code ec;fs::remove_all(path,ec);
 std::cout<<"SW2.1 durable registry recovery, locking, disk audit, and cursor tests passed\n";
}

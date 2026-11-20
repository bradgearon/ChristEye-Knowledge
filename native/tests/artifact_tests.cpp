#include "anaiero/sdk/engine_files.hpp"
#include "christeye/living/durable_io.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace anaiero::sdk;
namespace fs=std::filesystem;
namespace os=christeye::living::durable::os;
namespace {
void expect(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
void require(Result result,const std::string& why){if(!result)throw std::runtime_error(why+": "+std::string(code_name(result.code))+" "+result.message);}
std::string utf8(const fs::path& path){const auto value=path.generic_u8string();return {reinterpret_cast<const char*>(value.data()),value.size()};}
struct Temporary {fs::path path;Temporary(){path=fs::temp_directory_path()/("anaiero-artifact-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(path);}~Temporary(){std::error_code error;fs::remove_all(path,error);}};
struct Paths {
  fs::path target,intent,stage,lock;std::string identity;
  Paths(const fs::path& parent,std::string filename){target=fs::canonical(parent)/filename;
#ifdef _WIN32
    for(auto& c:filename)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
#endif
    identity=utf8(target.parent_path())+"/"+filename;const auto stem=target.parent_path()/(".anaiero-artifact-"+sha256(filename));intent=stem;intent+=".intent";stage=stem;stage+=".stage";lock=stem;lock+=".lock";
  }
};
void write(const fs::path& path,std::string_view bytes){std::ofstream output(path,std::ios::binary|std::ios::trunc);output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));expect(static_cast<bool>(output),"write crash fixture");}
std::string read(const fs::path& path){std::ifstream input(path,std::ios::binary);expect(static_cast<bool>(input),"read fixture");return {std::istreambuf_iterator<char>(input),{}};}
Bytes intent(const Paths& paths,std::string_view expected){const auto body=pack({"ANAIERO-ARTIFACT-INTENT/1",paths.identity,sha256(expected),std::to_string(expected.size())});return pack({body,sha256(body)});}
Result publish(const Paths& paths,std::string_view bytes){return anaiero::engines::write_new_artifact(utf8(paths.target),bytes);}
void ordinary_and_conflicting_outputs(){
  Temporary temp;Paths paths(temp.path,"artifact.bin");const Bytes bytes("binary\0payload\n",15);require(publish(paths,bytes),"publish binary artifact");expect(read(paths.target)==bytes,"published exact binary bytes");expect(!fs::exists(paths.intent)&&!fs::exists(paths.stage),"successful publication cleans owned work");require(publish(paths,bytes),"matching existing artifact retry");expect(publish(paths,"different").code==Code::conflict&&read(paths.target)==bytes,"different existing artifact is preserved");
  Paths empty(temp.path,"empty.bin");require(publish(empty,""),"publish empty artifact");expect(fs::file_size(empty.target)==0,"empty artifact has exact size");Paths directory(temp.path,"directory");fs::create_directories(directory.target);expect(publish(directory,"bytes").code==Code::conflict&&fs::is_directory(directory.target),"directory target is preserved");
}
void durable_stage_recovery(){
  Temporary temp;const std::string bytes="complete artifact bytes across a restart";
  Paths complete(temp.path,"complete.bin");write(complete.intent,intent(complete,bytes));write(complete.stage,bytes);require(publish(complete,bytes),"adopt complete owned staging after crash");expect(read(complete.target)==bytes&&!fs::exists(complete.stage)&&!fs::exists(complete.intent),"complete staged bytes activated once");
  Paths partial(temp.path,"partial.bin");write(partial.intent,intent(partial,bytes));write(partial.stage,bytes.substr(0,9));require(publish(partial,bytes),"recover exact owned partial prefix");expect(read(partial.target)==bytes&&!fs::exists(partial.stage)&&!fs::exists(partial.intent),"partial owned stage completed");
  Paths intent_only(temp.path,"intent-only.bin");write(intent_only.intent,intent(intent_only,bytes));require(publish(intent_only,bytes),"resume committed intent before staging creation");expect(read(intent_only.target)==bytes,"intent-only crash recovered");
  Paths visible(temp.path,"visible.bin");write(visible.intent,intent(visible,bytes));write(visible.target,bytes);require(publish(visible,bytes),"finish publication-before-directory-barrier retry");expect(read(visible.target)==bytes&&!fs::exists(visible.intent),"existing complete target re-flushed and intent cleaned");
}
void preserve_unknown_work(){
  Temporary temp;const std::string bytes="requested bytes";
  Paths unowned(temp.path,"unowned.bin");write(unowned.stage,bytes.substr(0,4));expect(publish(unowned,bytes).code==Code::conflict&&!fs::exists(unowned.intent)&&!fs::exists(unowned.target)&&read(unowned.stage)==bytes.substr(0,4),"partial unowned staging is not adopted or removed");
  Paths different(temp.path,"different.bin");const auto original_intent=intent(different,"another operation");write(different.intent,original_intent);write(different.stage,"another operation");expect(publish(different,bytes).code==Code::conflict&&read(different.intent)==original_intent&&read(different.stage)=="another operation"&&!fs::exists(different.target),"different intent ownership is preserved");require(publish(different,"another operation"),"original matching request can finish its own staging");
  Paths corrupt(temp.path,"corrupt.bin");const auto owned=intent(corrupt,bytes);write(corrupt.intent,owned);write(corrupt.stage,"alien");expect(publish(corrupt,bytes).code==Code::conflict&&read(corrupt.stage)=="alien"&&read(corrupt.intent)==owned&&!fs::exists(corrupt.target),"mismatched bytes under a matching intent are preserved");
  Paths oversized(temp.path,"oversized.bin");write(oversized.intent,intent(oversized,bytes));write(oversized.stage,bytes+"extra");expect(publish(oversized,bytes).code==Code::conflict&&read(oversized.stage)==bytes+"extra","oversized staging is preserved");
  Paths invalid(temp.path,"invalid-intent.bin");write(invalid.intent,"truncated intent");write(invalid.stage,bytes);expect(publish(invalid,bytes).code==Code::corrupt&&read(invalid.intent)=="truncated intent"&&read(invalid.stage)==bytes,"invalid intent is preserved");
  Paths already(temp.path,"already.bin");write(already.target,bytes);write(already.stage,"unrelated staging");require(publish(already,bytes),"matching target with unrelated stage");expect(read(already.stage)=="unrelated staging"&&!fs::exists(already.intent),"matching existing target cannot authorize deleting unrelated staging");
}
void ownership_and_bounds(){
  Temporary temp;Paths held(temp.path,"locked.bin"),other(temp.path,"other.bin");os::File lock;expect(lock.open(held.lock)&&lock.lock_exclusive(),"hold writer lock fixture");expect(publish(held,"value").code==Code::busy&&!fs::exists(held.target)&&!fs::exists(held.intent),"same target rejects concurrent writer without effects");require(publish(other,"independent"),"independent target is not blocked");
#ifdef _WIN32
  auto alias=held.target.parent_path()/"LOCKED.BIN";expect(anaiero::engines::write_new_artifact(utf8(alias),"value").code==Code::busy,"Windows case alias shares the target writer lock");
#else
  const auto alias=temp.path/"alias";fs::create_directory_symlink(temp.path,alias);expect(anaiero::engines::write_new_artifact(utf8(alias/"locked.bin"),"value").code==Code::busy,"parent alias shares the target writer lock");
#endif
  lock.close();require(publish(held,"value"),"writer can retry after lock release");
  Paths large(temp.path,"large.bin");expect(publish(large,std::string(8U*1024U*1024U+1U,'x')).code==Code::capacity&&!fs::exists(large.target)&&!fs::exists(large.intent),"artifact byte admission bound");
  write(large.target,std::string(9U*1024U*1024U,'x'));expect(publish(large,"small").code==Code::conflict&&fs::file_size(large.target)==9U*1024U*1024U,"oversized existing file is rejected without replacement");
  const auto nul=utf8(temp.path/"nul.bin")+std::string("\0suffix",7);expect(anaiero::engines::write_new_artifact(nul,"x").code==Code::invalid,"embedded NUL path rejected");
#ifndef _WIN32
  Paths symlink(temp.path,"symlink.bin");fs::create_symlink(held.target,symlink.target);expect(publish(symlink,"replacement").code==Code::conflict&&read(held.target)=="value"&&fs::is_symlink(symlink.target),"symlink target is never overwritten");
  Paths lock_link(temp.path,"lock-link.bin");fs::create_symlink(held.target,lock_link.lock);expect(publish(lock_link,"replacement").code==Code::conflict&&read(held.target)=="value","symlink lock is never opened as a writable artifact lock");
#endif
}
}
int main(){try{ordinary_and_conflicting_outputs();durable_stage_recovery();preserve_unknown_work();ownership_and_bounds();std::cout<<"artifact publication: bounded binary writes, durable retries, exclusive ownership, crash staging recovery and preservation passed\n";return 0;}catch(const std::exception& error){std::cerr<<"artifact publication failure: "<<error.what()<<'\n';return 1;}}

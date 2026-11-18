#include "anaiero/sdk/native_mesh.hpp"
// The TLS adapter selects Winsock 2 before IPC includes the Windows SDK.
#include "anaiero/infrastructure/native_tls.hpp"
#include "anaiero/infrastructure/native_ipc.hpp"
#include <openssl/pem.h>
#include <charconv>
#include <condition_variable>
#include <csignal>
#include <deque>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>

namespace sdk=anaiero::sdk;
namespace mesh=sdk::mesh;
namespace ev=sdk::evidence;
namespace fs=std::filesystem;
namespace tls=anaiero::mesh::tls;
namespace {
volatile std::sig_atomic_t interrupted=0;
void interrupt(int){interrupted=1;}
bool integer(std::string_view text,std::uint64_t& value){const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);return !text.empty()&&parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&std::to_string(value)==text;}
fs::path path(std::string_view text){return fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(text.data()),text.size()));}
class StoreExecutor final:public sdk::host::IStoreTaskExecutor {
  struct Job {sdk::host::StoreTask task;sdk::host::StoreTaskCompletion completion;};
  std::unique_ptr<ev::ICommitStore> store_;std::mutex mutex_;std::condition_variable changed_;std::deque<Job> jobs_;std::thread worker_;bool stopping_=false;
public:
  sdk::Result open(std::string_view directory,bool initialize=false,std::uint64_t maximum=1024ULL*1024ULL*1024ULL){
    std::error_code error;const auto root=path(directory)/"ROOT";
    if(initialize){if(fs::exists(path(directory),error)&&(!fs::is_directory(path(directory),error)||!fs::is_empty(path(directory),error)))return {sdk::Code::conflict,"mesh store initialization requires a new empty directory"};}
    else if(!fs::is_regular_file(fs::symlink_status(root,error))||error)return {sdk::Code::not_found,"canonical store ROOT not found"};
    ev::StoreOptions options;options.maximum_bytes=maximum;if(auto result=ev::open_native_store(directory,options,store_);!result)return result;
    const auto identity=store_->info().store_id;
    if(!initialize)for(unsigned attempt=0;attempt<4;++attempt){
      ev::StoredRecord record;auto found=store_->latest("operator.policy","capacity",record);
      if(found.code==sdk::Code::not_found)break;
      if(!found)return found;
      std::vector<std::string> fields;std::uint64_t limit=0;
      if(record.record.tombstone||!sdk::unpack(record.record.payload,fields,2,1024)||fields.size()!=2||fields[0]!="store-budget/1"||!integer(fields[1],limit)||limit<65536)return {sdk::Code::corrupt,"invalid persisted capacity policy"};
      if(limit==options.maximum_bytes)break;
      if(attempt==3)return {sdk::Code::busy,"capacity policy repeatedly changed"};
      store_.reset();options.maximum_bytes=limit;if(!fs::is_regular_file(fs::symlink_status(root,error))||error)return {sdk::Code::conflict,"canonical root disappeared"};
      if(auto result=ev::open_native_store(directory,options,store_);!result)return result;
      if(store_->info().store_id!=identity)return {sdk::Code::conflict,"canonical store identity changed"};
    }
    if(store_->info().operation_epoch)return {sdk::Code::unsupported,"built-in mesh requires receipt epoch zero; preserve retained operation receipts"};
    if(initialize){ev::CommitBatch batch;batch.operation_id="mesh-store-capacity";batch.request_digest=sdk::sha256(std::to_string(maximum));batch.records.push_back({"operator.policy","capacity",sdk::pack({"store-budget/1",std::to_string(maximum)}),{},false});batch.records.push_back({"operator.runtime","canonical-mesh","canonical-mesh/2",{},false});ev::CommitReceipt receipt;if(auto result=store_->commit(batch,receipt);!result.committed())return result;}
    worker_=std::thread([this]{for(;;){Job job;{std::unique_lock lock(mutex_);changed_.wait(lock,[&]{return stopping_||!jobs_.empty();});if(stopping_&&jobs_.empty())return;job=std::move(jobs_.front());jobs_.pop_front();}sdk::Result result;try{result=job.task(*store_);}catch(...){result={sdk::Code::io_error,"mesh storage task exception"};}job.completion(std::move(result));}});
    return {};
  }
  sdk::Result submit(sdk::host::StoreTask task,sdk::host::StoreTaskCompletion completion) override {
    {std::lock_guard lock(mutex_);if(stopping_||!worker_.joinable())return {sdk::Code::busy,"mesh storage worker unavailable"};if(jobs_.size()>=8)return {sdk::Code::capacity,"mesh storage queue full"};jobs_.push_back({std::move(task),std::move(completion)});}changed_.notify_one();return {};
  }
  ~StoreExecutor(){ {std::lock_guard lock(mutex_);stopping_=true;}changed_.notify_all();if(worker_.joinable())worker_.join();}
};
int finish(const sdk::Result& result){if(!result.committed()){std::cerr<<sdk::code_name(result.code)<<": "<<result.message<<'\n';return 1;}return 0;}
std::string command(int begin,int argc,char** argv){std::string text;for(int i=begin;i<argc;++i){if(!text.empty())text+=' ';text+=argv[i];}return text;}
void usage(){std::cerr<<
  "anaiero mesh-live init STORE [MAXIMUM_BYTES]\n"
  "anaiero mesh-live certificate CERTIFICATE_PEM\n"
  "anaiero mesh-live local STORE scope SOURCE EPOCH\n"
  "anaiero mesh-live local STORE peer PRINCIPAL CERT_SHA GEN EXPECTED EXPIRY active|suspended|revoked OPERATION\n"
  "anaiero mesh-live local STORE grant PRINCIPAL CONTEXT_HEX GEN EXPECTED send|receive|both|closed bootstrap|ordinary OPERATION [ARTIFACT_SHA...]\n"
  "anaiero mesh-live admin ENDPOINT COMMAND...\n"
  "anaiero mesh-live result ENDPOINT TICKET\n"
  "anaiero mesh-live follow|artifact|bootstrap STORE CONFIG LOCAL_PRINCIPAL REMOTE_PRINCIPAL SERVER_SHA SOURCE EPOCH SUBSCRIPTION GENERATION LEASE KIND VALUE\n"
  "  KIND VALUE is through SEQUENCE (0 continuous), digest SHA, or next GENERATION.\n"
  "  CONFIG is ANAIEROMESH/2 with client certificate/key/authority and remote address/port.\n"
  "  Shared-session server: anaiero host start ENDPOINT STORE PERIOD_MS [paused] --mesh CONFIG\n";}
}
int main(int argc,char** argv){
  if(argc<2){usage();return 2;}
  try {
    const std::string action=argv[1];
    if(action=="certificate"&&argc==3){
      using Bio=std::unique_ptr<BIO,decltype(&BIO_free)>;using Certificate=std::unique_ptr<X509,decltype(&X509_free)>;
      Bio bio(BIO_new_file(argv[2],"rb"),BIO_free);if(!bio)return finish({sdk::Code::io_error,"cannot read certificate"});
      Certificate certificate(PEM_read_bio_X509(bio.get(),nullptr,nullptr,nullptr),X509_free);std::string digest;std::uint64_t expiry=0;
      if(!tls::detail::certificate_identity(certificate.get(),digest,expiry))return finish({sdk::Code::invalid,"invalid certificate"});
      std::cout<<"CERTIFICATE "<<digest<<" EXPIRES "<<expiry<<'\n';return 0;
    }
    if((action=="admin"||action=="result")&&argc>=4){
      std::string reply;const auto request=(action=="admin"?"MESH ":"MESH_RESULT ")+command(3,argc,argv);
      if(!anaiero::host::NativeIpcClient::request(argv[2],request,reply,5000))return finish({sdk::Code::io_error,"native host IPC request failed"});
      std::cout<<reply;
      if(reply.starts_with("ERR"))return 1;
      if(action=="result"&&reply.find("\"complete\":true")!=std::string::npos&&reply.find("\"code\":\"ok\"")==std::string::npos&&reply.find("\"code\":\"committed_degraded\"")==std::string::npos)return 1;
      return 0;
    }
    if(action=="init"&&(argc==3||argc==4)){
      std::uint64_t maximum=1024ULL*1024ULL*1024ULL;if(argc==4&&(!integer(argv[3],maximum)||maximum<65536))return 2;
      StoreExecutor executor;const auto result=executor.open(argv[2],true,maximum);if(result)std::cout<<"INITIALIZED canonical relay store\n";return finish(result);
    }
    if(action=="local"&&argc>=4){StoreExecutor executor;if(auto result=executor.open(argv[2]);!result)return finish(result);sdk::Bytes output;
      const auto result=mesh::administer_native(executor,command(3,argc,argv),output);if(result.committed())std::cout<<output;return finish(result);}
    if((action=="follow"||action=="artifact"||action=="bootstrap")&&argc==14){
      StoreExecutor executor;if(auto result=executor.open(argv[2]);!result)return finish(result);
      mesh::NativeServerOptions config;if(auto result=mesh::read_native_server_options(argv[3],config);!result)return finish(result);
      mesh::NativeSubscriberOptions options;options.credentials=config.credentials;options.address=config.address;options.port=config.port;options.local_principal=argv[4];options.remote_principal=argv[5];options.server_certificate_sha256=argv[6];
      std::uint64_t epoch=0,generation=0,lease=0;if(!integer(argv[8],epoch)||!integer(argv[10],generation)||!integer(argv[11],lease))return 2;
      options.subscription={argv[9],argv[4],{argv[7],epoch},generation,lease,sdk::lifecycle::SubscriptionPhase::active,{},{},false};
      // argv[12] explicitly names the value kind, avoiding positional ambiguity
      // between finite catch-up, an artifact digest and a bootstrap generation.
      if(action=="follow"){
        if(std::string_view(argv[12])!="through"||!integer(argv[13],options.through_sequence))return 2;
        mesh::NativeSubscriber subscriber(executor,options);if(auto result=subscriber.start();!result)return finish(result);
        std::signal(SIGINT,interrupt);std::signal(SIGTERM,interrupt);
        sdk::Result result;
        do {result=subscriber.wait(1000);}while(result.code==sdk::Code::deadline&&!interrupted);
        if(interrupted){subscriber.stop();result={};}
        const auto status=subscriber.status();std::cout<<"RECEIVED "<<status.cursor.sequence<<" "<<status.cursor.payload_sha256<<" PAGES "<<status.committed_pages<<" RECONNECTS "<<status.reconnects<<'\n';return finish(result);
      }
      mesh::ReceiveReceipt receipt;sdk::Result result;
      if(action=="artifact"){if(std::string_view(argv[12])!="digest")return 2;result=mesh::fetch_native_artifact(executor,options,argv[13],receipt);}
      else {std::uint64_t next=0;if(std::string_view(argv[12])!="next"||!integer(argv[13],next))return 2;result=mesh::install_native_bootstrap(executor,options,next,receipt);}
      if(result.committed())std::cout<<(action=="bootstrap"?"BOOTSTRAP_INSTALLED ":"ARTIFACT_COMMITTED ")<<receipt.receipt_sha256<<" GENERATION "<<receipt.store_generation<<'\n';
      return finish(result);
    }
    usage();return 2;
  }catch(const std::exception& error){std::cerr<<"io_error: "<<error.what()<<'\n';return 1;}
}

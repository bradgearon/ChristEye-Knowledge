#include "anaiero/infrastructure/native_tls.hpp"
#include "anaiero/application/native_mesh_client.hpp"
#include "anaiero/infrastructure/native_mesh_host.hpp"
#include "anaiero/infrastructure/native_session_mesh_export.hpp"
#include "anaiero/infrastructure/native_host_lifecycle.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
namespace {
namespace mesh=anaiero::mesh;
namespace app=anaiero::application;
namespace db=christeye::living::durable;
template<class T> bool number(std::string_view text,T& value){
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
  return !text.empty()&&parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size();
}
bool scope(std::string_view text,mesh::Scope& value){
  std::array<std::string_view,4> fields{};
  for(std::size_t i=0;i<fields.size();++i){const auto colon=text.find(':');
    if((i+1<fields.size())==(colon==std::string_view::npos))return false;
    fields[i]=text.substr(0,colon);if(i+1<fields.size())text.remove_prefix(colon+1);}
  return number(fields[0],value.source)&&number(fields[1],value.epoch)&&number(fields[2],value.clock)&&
    number(fields[3],value.colony)&&mesh::valid_scope(value);
}
bool write_new(const std::filesystem::path& path,std::string_view bytes){
  std::error_code error;
  if(bytes.empty()||std::filesystem::exists(path,error)||error)return false;
  if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path(),error);
  return !error&&db::os::atomic_write(path,bytes);
}
void help(){
  std::cout<<"Anaiero native authenticated evidence mesh\n\n"
    <<"ce_mesh configure FILE NAME PRINCIPAL CERT KEY CA LISTEN_ADDRESS PORT CONNECT_ADDRESS PORT SERVER_PIN_OR_none\n"
    <<"ce_mesh seal-config DRAFT NEW_FILE\nce_mesh seal-policy DRAFT NEW_FILE\n"
    <<"ce_mesh enroll CONFIG STATE PRINCIPAL CERT_SHA256 EXPIRES_UNIX grant...\n"
    <<"  grant: publish=SOURCE:EPOCH:CLOCK:COLONY | subscribe=SOURCE:EPOCH:CLOCK:COLONY | artifact=SHA256\n"
    <<"ce_mesh policy CONFIG STATE\nce_mesh revoke CONFIG STATE PRINCIPAL EXPECTED_REVISION\n"
    <<"ce_mesh import-policy CONFIG STATE POLICY_FILE EXPECTED_REVISION\n"
    <<"ce_mesh import-session CONFIG STATE SESSION_CONFIG SESSION_STATE\n"
    <<"ce_mesh artifact-add CONFIG STATE FILE\n"
    <<"ce_mesh serve CONFIG STATE LOCAL_ADMIN_ENDPOINT\nce_mesh serve-stdio CONFIG STATE\n"
    <<"ce_mesh query LOCAL_ADMIN_ENDPOINT COMMAND\n"
    <<"  local admin: STATE | TRACE | REVOKE PRINCIPAL REVISION | IMPORT \"FILE\" REVISION | RELOAD | STOP\n"
    <<"ce_mesh sync CONFIG STATE SUBSCRIPTION SOURCE:EPOCH:CLOCK:COLONY\n"
    <<"ce_mesh push CONFIG STATE SUBSCRIPTION SOURCE:EPOCH:CLOCK:COLONY\n"
    <<"ce_mesh fetch-artifact CONFIG STATE SHA256\n"
    <<"ce_mesh inspect CONFIG STATE\nce_mesh maintain CONFIG STATE\n";
}
int configure(int argc,char** argv){
  if(argc!=13)return 2;
  mesh::files::MeshConfiguration value;value.name=argv[3];value.principal=argv[4];
  value.certificate_file=std::filesystem::absolute(argv[5]).string();
  value.private_key_file=std::filesystem::absolute(argv[6]).string();
  value.ca_file=std::filesystem::absolute(argv[7]).string();
  value.listen_address=argv[8];value.connect_address=argv[10];
  if(!number(argv[9],value.listen_port)||!number(argv[11],value.connect_port))return 2;
  if(std::string_view(argv[12])!="none")value.server_pin=argv[12];
  if(!write_new(argv[2],mesh::files::serialize_configuration(value))){std::cerr<<"Invalid configuration or output already exists\n";return 1;}
  std::cout<<"{\"configured\":true}\n";return 0;
}
int seal(int argc,char** argv,bool policy){
  if(argc!=4)return 2;
  std::string bytes,error;
  if(!db::read_bounded(argv[2],policy?mesh::files::kMaxPolicyBytes:mesh::files::kMaxConfigurationBytes,bytes))return 1;
  const auto marker=bytes.rfind("\nseal ");if(marker!=std::string::npos)bytes.resize(marker+1);
  if(bytes.empty()||bytes.back()!='\n')return 2;
  bytes+="seal "+christeye::living::plans::sha256_hex(bytes)+'\n';
  mesh::EnrollmentPolicy enrollment;mesh::files::MeshConfiguration configuration;
  if(!(policy?mesh::files::parse_policy(bytes,enrollment,error):mesh::files::parse_configuration(bytes,configuration,error))){std::cerr<<error<<'\n';return 1;}
  if(!write_new(argv[3],bytes)){std::cerr<<"Output exists or cannot be written\n";return 1;}
  std::cout<<"{\"sealed\":true}\n";return 0;
}
int enroll(int argc,char** argv,app::NativeMeshApplication& application){
  if(argc<8)return 2;
  mesh::Enrollment value;value.principal=argv[4];value.certificate_sha256=argv[5];value.generation=1;
  mesh::files::SystemUnixClock clock;
  if(!number(argv[6],value.expires_unix_s)||value.expires_unix_s<=clock.unix_seconds())return 2;
  for(int i=7;i<argc;++i){const std::string_view grant=argv[i];const auto equals=grant.find('=');
    if(equals==std::string_view::npos)return 2;
    const auto kind=grant.substr(0,equals),argument=grant.substr(equals+1);
    if(kind=="artifact"){
      if(value.artifact_count==value.artifact_sha256.size()||!mesh::mesh_digest(argument))return 2;
      value.artifact_sha256[value.artifact_count++]=argument;value.capabilities|=mesh::capability_bit(mesh::MeshCapability::artifact_read);
    }else{
      mesh::Scope parsed;if(!scope(argument,parsed))return 2;
      if(kind=="publish"){
        if(value.publication_count==value.publications.size())return 2;
        value.publications[value.publication_count++]=parsed;value.capabilities|=mesh::capability_bit(mesh::MeshCapability::publish);
      }else if(kind=="subscribe"){
        if(value.subscription_count==value.subscriptions.size())return 2;
        value.subscriptions[value.subscription_count++]=parsed;value.capabilities|=mesh::capability_bit(mesh::MeshCapability::subscribe);
      }else return 2;
    }
  }
  const auto& policy=application.authorization().snapshot();
  for(std::size_t i=0;i<policy.count;++i)if(policy.peers[i].principal==value.principal){
    value.generation=policy.peers[i].generation;
    if(value!=policy.peers[i]){if(value.generation==UINT64_MAX)return 1;++value.generation;}
  }
  const auto status=application.authorization().upsert(value,policy.revision);
  if(status!=mesh::EnrollmentCode::ok){std::cerr<<"Enrollment rejected: "<<static_cast<unsigned>(status)<<'\n';return 1;}
  std::cout<<application.state_json();return 0;
}
int serve(int argc,char** argv,app::NativeMeshApplication& application,bool stdio){
  if(argc!=(stdio?4:5))return 2;
  const auto& config=application.configuration();if(!config.listen_port)return 2;
  mesh::tls::Context context;
  auto status=context.open({config.certificate_path.string(),config.private_key_path.string(),config.ca_path.string()},mesh::tls::Role::server);
  if(status!=mesh::TransportCode::ok){std::cerr<<context.last_error()<<'\n';return 1;}
  mesh::NativeMeshHost host(application,context,{config.listen_address,config.listen_port},config.deadline_ms);
  status=host.start();if(status!=mesh::TransportCode::ok){std::cerr<<mesh::transport_code_name(status)<<'\n';return 1;}
  if(stdio){
    // Explicit local control on the caller-owned console/pipe, useful for
    // native process supervisors. No network request reaches this channel.
    while(true){std::string command;char character=0;bool ended=false;
      while(std::cin.get(character)){if(character=='\n'){ended=true;break;}
        if(command.size()==anaiero::host::kMaxRequestBytes){host.stop();return 1;}command+=character;}
      if(!ended)break;
      const auto reply=host.administration().handle(command);
      std::cout.write(reply.bytes.data(),static_cast<std::streamsize>(reply.length));std::cout.flush();
      if(reply.stop)break;
    }
    host.stop();return 0;
  }
  anaiero::host::NativeIpcServer admin(argv[4]);anaiero::host::NativeLifecycleOptions options;
  options.on_stop=[](void* context)noexcept{static_cast<mesh::NativeMeshHost*>(context)->request_stop();};options.stop_context=&host;
  const auto result=anaiero::host::NativeHostLifecycle::run(admin,host.administration(),options);
  host.stop();return result==anaiero::host::Code::ok||result==anaiero::host::Code::interrupted?0:1;
}
}
int main(int argc,char** argv){
  if(argc<2||std::string_view(argv[1])=="help"||std::string_view(argv[1])=="--help"){help();return argc<2?2:0;}
  const std::string_view mode=argv[1];
  if(mode=="configure")return configure(argc,argv);
  if(mode=="seal-config"||mode=="seal-policy")return seal(argc,argv,mode=="seal-policy");
  if(mode=="query"){
    if(argc!=4)return 2;
    std::string response;
    if(!anaiero::host::NativeIpcClient::request(argv[2],argv[3],response))return 1;
    std::cout<<response;return response.starts_with("ERROR")?1:0;
  }
  if(argc<4)return 2;
  auto application=std::make_unique<app::NativeMeshApplication>();
  if(!application->open(argv[2],argv[3])){std::cerr<<application->error()<<'\n';return 1;}
  if(mode=="enroll")return enroll(argc,argv,*application);
  if(mode=="inspect"&&argc==4){std::cout<<application->state_json();return 0;}
  if(mode=="policy"&&argc==4){std::cout<<mesh::files::serialize_policy(application->authorization().snapshot());return 0;}
  if(mode=="revoke"&&argc==6){std::uint64_t expected=0;if(!number(argv[5],expected))return 2;
    const auto result=application->authorization().revoke(argv[4],expected);
    if(result!=mesh::EnrollmentCode::ok)return 1;
    std::cout<<application->state_json();return 0;}
  if(mode=="import-policy"&&argc==6){std::uint64_t expected=0;if(!number(argv[5],expected))return 2;
    mesh::EnrollmentPolicy policy;std::string error;
    if(!mesh::files::load_policy(argv[4],policy,error)){std::cerr<<error<<'\n';return 1;}
    const auto result=application->authorization().replace(policy,expected);
    if(result!=mesh::EnrollmentCode::ok)return 1;
    std::cout<<application->state_json();return 0;}
  if(mode=="maintain"&&argc==4){if(!application->maintain())return 1;std::cout<<application->state_json();return 0;}
  if(mode=="artifact-add"&&argc==5){std::string bytes;
    if(!db::read_bounded(argv[4],mesh::kMaxMeshArtifactBytes,bytes))return 1;
    const mesh::ArtifactReceipt receipt{christeye::living::plans::sha256_hex(bytes),bytes.size()};
    if(application->artifact_store().commit(receipt,bytes)!=mesh::ArtifactCode::ok)return 1;
    std::cout<<"{\"sha256\":\""<<receipt.sha256<<"\",\"bytes\":"<<receipt.bytes<<"}\n";return 0;}
  if(mode=="serve"||mode=="serve-stdio")return serve(argc,argv,*application,mode=="serve-stdio");
  if(mode=="import-session"&&argc==6){
    const auto& config=application->configuration();mesh::tls::Context context;
    if(context.open({config.certificate_path.string(),config.private_key_path.string(),config.ca_path.string()},mesh::tls::Role::client)!=mesh::TransportCode::ok)return 1;
    app::LocalMeshIdentity identity(context);mesh::VerifiedMeshSession self;
    if(application->authorization().authenticate(identity,self)!=mesh::EnrollmentCode::ok||self.authorized_peer().principal!=config.principal){
      std::cerr<<"Enroll this node's own certificate and publication scopes before local import\n";return 1;}
    std::string error;std::uint64_t count=0;
    const bool imported=mesh::export_native_session(argv[4],argv[5],[&](const anaiero::runtime::Observation& observation){
      return application->receive(self,observation,mesh::IngressKind::local_import).code==mesh::ReplayCode::ok;
    },error,count);
    if(!imported){std::cerr<<error<<"; processed "<<count<<'\n';return 1;}
    std::cout<<"{\"imported_or_already_present\":"<<count<<"}\n";return 0;
  }
  if((mode=="sync"||mode=="push")&&argc==6){
    mesh::Scope selected;if(!scope(argv[5],selected))return 2;
    app::NativeMeshClient client(*application);app::MeshTransferStats stats;
    if(!client.connect()||!(mode=="sync"?client.synchronize(argv[4],selected,stats):client.push(argv[4],selected,stats))){std::cerr<<client.error()<<'\n';return 1;}
    std::cout<<"{\"records\":"<<stats.records<<",\"duplicates\":"<<stats.duplicates<<",\"pages\":"<<stats.pages<<"}\n";return 0;
  }
  if(mode=="fetch-artifact"&&argc==5){app::NativeMeshClient client(*application);app::MeshTransferStats stats;
    if(!client.connect()||!client.fetch_artifact(argv[4],stats)){std::cerr<<client.error()<<'\n';return 1;}
    std::cout<<"{\"sha256\":\""<<argv[4]<<"\",\"bytes\":"<<stats.bytes<<",\"verified\":true}\n";return 0;
  }
  help();return 2;
}

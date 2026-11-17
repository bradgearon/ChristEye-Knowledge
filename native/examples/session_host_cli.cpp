#include "anaiero/application/native_session_application.hpp"
#include "anaiero/infrastructure/session_host_worker.hpp"
#include "anaiero/infrastructure/native_host_lifecycle.hpp"
#include "anaiero/services/session_command_service.hpp"
#include <charconv>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string_view>
int run_session_host(int argc,char**argv){
  namespace app=anaiero::application;
  namespace host=anaiero::host;
  namespace session=anaiero::session;
  const std::string_view mode=argv[1];
  const bool oneshot=mode=="session";
  if((oneshot&&(argc<4||argc>5))||(!oneshot&&argc!=5))return 2;
  auto application=std::make_unique<app::NativeSessionApplication>();
  const char* config=argv[oneshot?2:3];const char* state=argv[oneshot?3:4];
  if(!application->open(config,state)){
    std::cerr<<"anaiero session: "<<application->error()<<'\n';return 1;
  }
  if(oneshot){
    if(!application->settle()){std::cerr<<application->service().state_json();return 1;}
    if(argc==5){
      std::uint32_t cycles=0;const std::string_view value=argv[4];
      const auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),cycles);
      if(ec!=std::errc{}||end!=value.data()+value.size()||!cycles||cycles>1024)return 2;
      if(!application->service().execute({session::Operation::step,0,0,0,cycles},nullptr)||
         !application->settle()){
        std::cerr<<application->service().state_json();return 1;
      }
    }
    std::cout<<application->service().state_json();return 0;
  }
  session::SessionHostWorker worker(application->service(),application->wakeup(),
                                    application->configuration().period_ms);
  session::SessionCommandService commands(worker);
  host::NativeIpcServer server(argv[2]);
  host::NativeLifecycleOptions options{};
  options.windows_service=mode=="service";
  options.on_stop=[](void* context)noexcept{static_cast<session::SessionHostWorker*>(context)->shutdown();};
  options.stop_context=&worker;
  const auto result=host::NativeHostLifecycle::run(server,commands,options);
  worker.shutdown();
  return result==host::Code::ok||result==host::Code::interrupted?0:1;
}

#include "module_detail.hpp"
#include <cstring>
#include <utility>

namespace anaiero::sdk {
namespace {
std::int32_t code(Result result)noexcept{return static_cast<std::int32_t>(result.code);}
std::int32_t write_bytes(std::string_view bytes,AnaieroBufferV1* output) {
  if(!output||(!output->data&&output->capacity))return code({Code::invalid,"output buffer"});
  output->written=static_cast<std::uint64_t>(bytes.size());
  if(bytes.size()>output->capacity)return code({Code::capacity,"output buffer"});
  if(!bytes.empty())std::memcpy(output->data,bytes.data(),bytes.size());
  return 0;
}
bool input_bytes(AnaieroBytesV1 value,std::string_view& output) {
  if(value.size>module_detail::maximum_wire||(!value.data&&value.size))return false;
  output=std::string_view(reinterpret_cast<const char*>(value.data),static_cast<std::size_t>(value.size));return true;
}
template<class Function> std::int32_t guarded(Function&& function)noexcept {
  try{return function();}catch(...){return code({Code::io_error,"module callback threw"});}
}
}
struct ModuleApiAdapter::Impl {
  EngineDescriptor descriptor;std::shared_ptr<IEngineFactory> factory;AnaieroModuleApiV1 table{};
  Impl(EngineDescriptor d,std::shared_ptr<IEngineFactory> f):descriptor(std::move(d)),factory(std::move(f)) {
    table.struct_size=sizeof(AnaieroModuleApiV1);table.abi_version=ANAIERO_MODULE_ABI_VERSION;table.context=this;
    table.describe=[](void* context,AnaieroBufferV1* output)->std::int32_t {
      return guarded([&]{return write_bytes(encode_descriptor(static_cast<Impl*>(context)->descriptor),output);});
    };
    table.create=[](void* context,AnaieroBytesV1 value,void** output)->std::int32_t {
      return guarded([&]()->std::int32_t {
        if(!output)return code({Code::invalid,"create output"});
        *output=nullptr;std::string_view input;
        if(!input_bytes(value,input))return code({Code::capacity,"creation input"});
        EngineIdentity identity;Envelope configuration;EngineState restored;bool has_restore=false;
        auto result=module_detail::decode_creation(input,identity,configuration,restored,has_restore);if(!result)return code(result);
        auto& self=*static_cast<Impl*>(context);std::unique_ptr<IEngine> engine;
        if(!self.factory)return code({Code::invalid,"module factory"});
        result=self.factory->create(identity,configuration,has_restore?&restored:nullptr,engine);if(!result)return code(result);
        if(!engine||engine->identity()!=identity||engine->state().configuration!=configuration||
            !valid_state(engine->state(),self.descriptor.limits)||(has_restore&&engine->state()!=restored)||
            (engine->trials()!=nullptr)!=self.descriptor.isolated_trials||
            (engine->changes()!=nullptr)!=self.descriptor.change_preparation)
          return code({Code::corrupt,"module capability/state contract"});
        *output=engine.release();return 0;
      });
    };
    table.destroy=[](void*,void* engine){try{delete static_cast<IEngine*>(engine);}catch(...){}};
    table.state=[](void*,void* engine,AnaieroBufferV1* output)->std::int32_t {
      return guarded([&]{return engine?write_bytes(encode_state(static_cast<IEngine*>(engine)->state()),output):code({Code::invalid,"state handle"});});
    };
    table.prepare=[](void*,void* engine,AnaieroBytesV1 value,AnaieroBufferV1* output)->std::int32_t {
      return guarded([&]()->std::int32_t {
        std::string_view input;if(!engine||!input_bytes(value,input))return code({Code::invalid,"prepare handle/input"});
        StepRequest request;auto result=decode_request(input,request);if(!result)return code(result);
        PreparedTransition transition;result=static_cast<IEngine*>(engine)->prepare_step(request,transition);
        return result?write_bytes(encode_transition(transition),output):code(result);
      });
    };
    table.install=[](void*,void* engine,AnaieroBytesV1 value)->std::int32_t {
      return guarded([&]()->std::int32_t {
        std::string_view input;if(!engine||!input_bytes(value,input))return code({Code::invalid,"install handle/input"});
        EngineState state;auto result=decode_state(input,state);return result?code(static_cast<IEngine*>(engine)->install(state)):code(result);
      });
    };
    table.restore=[](void*,void* engine,AnaieroBytesV1 value)->std::int32_t {
      return guarded([&]()->std::int32_t {
        std::string_view input;if(!engine||!input_bytes(value,input))return code({Code::invalid,"restore handle/input"});
        EngineState state;auto result=decode_state(input,state);return result?code(static_cast<IEngine*>(engine)->restore(state)):code(result);
      });
    };
    if(descriptor.isolated_trials)table.fork=[](void*,void* engine,void** output)->std::int32_t {
      return guarded([&]()->std::int32_t {
        if(!engine||!output)return code({Code::invalid,"fork handle/output"});
        *output=nullptr;auto* trials=static_cast<IEngine*>(engine)->trials();if(!trials)return code({Code::unsupported,"fork"});
        std::unique_ptr<IEngine> isolated;auto result=trials->fork(isolated);if(!result)return code(result);
        if(!isolated||isolated->identity()!=static_cast<IEngine*>(engine)->identity()||isolated->state()!=static_cast<IEngine*>(engine)->state())
          return code({Code::corrupt,"fork source mismatch"});
        *output=isolated.release();return 0;
      });
    };
    if(descriptor.change_preparation)table.change=[](void*,void* engine,AnaieroBytesV1 value,AnaieroBufferV1* output)->std::int32_t {
      return guarded([&]()->std::int32_t {
        std::string_view input;if(!engine||!input_bytes(value,input))return code({Code::invalid,"change handle/input"});
        auto* changes=static_cast<IEngine*>(engine)->changes();if(!changes)return code({Code::unsupported,"change"});
        StepRequest request;auto result=decode_request(input,request);if(!result)return code(result);
        PreparedTransition transition;result=changes->prepare_change(request,transition);
        return result?write_bytes(encode_transition(transition),output):code(result);
      });
    };
    table.cancel=[](void*,void* engine){if(engine)static_cast<IEngine*>(engine)->request_cancel();};
  }
};
ModuleApiAdapter::ModuleApiAdapter(EngineDescriptor descriptor,std::shared_ptr<IEngineFactory> factory):
  impl_(std::make_unique<Impl>(std::move(descriptor),std::move(factory))){}
ModuleApiAdapter::~ModuleApiAdapter()=default;
const AnaieroModuleApiV1* ModuleApiAdapter::api()const noexcept{return &impl_->table;}
}

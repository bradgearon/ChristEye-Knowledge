#include "anaiero/sdk/lifecycle.hpp"
#include "christeye/living/durable_io.hpp"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <system_error>

namespace anaiero::sdk::lifecycle {
namespace {
namespace fs=std::filesystem;
namespace os=christeye::living::durable::os;
Result error(Code code,std::string message){return {code,std::move(message)};}
std::string path_text(const fs::path& path){const auto bytes=path.generic_u8string();return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};}
fs::path native_path(std::string_view value){return fs::path(std::u8string(reinterpret_cast<const char8_t*>(value.data()),value.size()));}
class NativeArchive final:public IArchiveStore {
  fs::path directory_;
  std::string identifier_;
  ArchiveOptions options_;
  os::File lock_;
  std::mutex mutex_;
  std::uint64_t bytes_=0;
  Result read(const fs::path& path,std::string_view digest,Bytes& out) {
    std::error_code ec;
    if(fs::is_symlink(path,ec))return error(Code::unauthorized,"archive object must not be a symlink");
    if(!fs::exists(path,ec))return error(Code::archive_unavailable,"archived object is unavailable");
    if(ec||!fs::is_regular_file(path,ec))return error(Code::io_error,"archive object is not a regular file");
    const auto size=fs::file_size(path,ec);
    if(ec)return error(Code::io_error,"cannot read archive object size");
    if(size>options_.maximum_object_bytes)return error(Code::capacity,"archive object exceeds the configured bound");
    std::ifstream stream(path,std::ios::binary);if(!stream)return error(Code::io_error,"cannot open archived object");
    out.resize(static_cast<std::size_t>(size));
    if(size&&!stream.read(out.data(),static_cast<std::streamsize>(size)))return error(Code::io_error,"archived object read was incomplete");
    if(sha256(out)!=digest)return error(Code::corrupt,"archived object digest does not match");
    return {};
  }
public:
  NativeArchive(fs::path directory,ArchiveOptions options):directory_(std::move(directory)),options_(options){}
  Result open(){
    if(!options_.maximum_bytes||!options_.maximum_object_bytes||options_.maximum_object_bytes>options_.maximum_bytes)
      return error(Code::invalid,"archive quotas are invalid");
    std::error_code ec;
    if(fs::is_symlink(directory_,ec))return error(Code::unauthorized,"archive root must not be a symlink");
    fs::create_directories(directory_,ec);if(ec)return error(Code::io_error,"cannot create archive directory");
    directory_=fs::weakly_canonical(directory_,ec);if(ec)return error(Code::io_error,"cannot resolve archive directory");
    identifier_=path_text(directory_);
    if(fs::is_symlink(directory_/"writer.lock",ec))return error(Code::unauthorized,"archive writer lock must not be a symlink");
    if(!lock_.open(directory_/"writer.lock")||!lock_.lock_exclusive())return error(Code::busy,"archive already has a writer");
    for(fs::directory_iterator it(directory_,ec),end;!ec&&it!=end;it.increment(ec)){
      if(it->is_symlink(ec))return error(Code::unauthorized,"archive entries must not be symlinks");
      if(!it->is_regular_file(ec))return error(Code::corrupt,"unexpected archive entry");
      const auto name=it->path().filename().string();
      if(name=="writer.lock")continue;
      if(name.ends_with(".tmp")){
        // A flushed object without final publication is never an archive receipt.
        fs::remove(it->path(),ec);if(ec)return error(Code::io_error,"cannot remove abandoned archive staging");
        continue;
      }
      if(name.size()!=69||name.substr(64)!=".blob"||!valid_digest(name.substr(0,64)))
        return error(Code::corrupt,"unknown file in managed archive");
      const auto size=it->file_size(ec);if(ec)return error(Code::io_error,"cannot measure archive usage");
      if(size>options_.maximum_object_bytes||bytes_>options_.maximum_bytes||size>options_.maximum_bytes-bytes_)
        return error(Code::capacity,"existing archive exceeds its quota");
      bytes_+=size;
    }
    return ec?error(Code::io_error,"cannot enumerate archive"):Result{};
  }
  std::string_view identifier() const noexcept override{return identifier_;}
  Result put(std::string_view digest,std::string_view bytes) override {
    std::lock_guard guard(mutex_);
    try{
      if(!valid_digest(digest)||sha256(bytes)!=digest)return error(Code::invalid,"archive object digest mismatch");
      if(bytes.size()>options_.maximum_object_bytes)return error(Code::capacity,"archive object exceeds its quota");
      const auto path=directory_/(std::string(digest)+".blob");std::error_code ec;
      if(fs::exists(path,ec)){
        Bytes existing;const auto result=read(path,digest,existing);if(!result)return result;
        return existing==bytes?Result{}:error(Code::corrupt,"immutable archive object differs");
      }
      if(ec)return error(Code::io_error,"cannot inspect archive destination");
      if(bytes_>options_.maximum_bytes||bytes.size()>options_.maximum_bytes-bytes_)
        return error(Code::capacity,"archive quota is exhausted");
      const auto space=fs::space(directory_,ec);
      if(ec||space.available<bytes.size())return error(Code::capacity,"archive has insufficient free disk space");
      if(!os::atomic_write(path,bytes))return error(Code::io_error,"archive object publication failed");
      bytes_+=static_cast<std::uint64_t>(bytes.size());
      Bytes verified;return read(path,digest,verified);
    }catch(const std::exception& exception){return error(Code::io_error,exception.what());}
  }
  Result get(std::string_view digest,Bytes& bytes) override {
    std::lock_guard guard(mutex_);
    try{
      if(!valid_digest(digest))return error(Code::invalid,"invalid archive object digest");
      return read(directory_/(std::string(digest)+".blob"),digest,bytes);
    }catch(const std::exception& exception){return error(Code::io_error,exception.what());}
  }
};
}
Result open_native_archive(std::string_view directory,const ArchiveOptions& options,std::unique_ptr<IArchiveStore>& out){
  out.reset();
  try{
    if(directory.empty())return {Code::invalid,"archive directory is empty"};
    auto archive=std::make_unique<NativeArchive>(native_path(directory),options);
    const auto result=archive->open();if(result)out=std::move(archive);return result;
  }catch(const std::exception& exception){return {Code::io_error,exception.what()};}
}
} // namespace anaiero::sdk::lifecycle

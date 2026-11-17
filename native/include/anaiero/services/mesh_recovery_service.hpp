#pragma once
#include "anaiero/contracts/mesh_ingress.hpp"
#include "anaiero/contracts/epoch_observation.hpp"
#include "anaiero/services/epoch_observation_delivery_service.hpp"
namespace anaiero::mesh {
// Cross-ledger recovery verifies bounded checkpoint tips and the active tail
// before any observation reaches the Projector. Cold-history audit is explicit.
class MeshRecoveryService final {
  const runtime::IObservationEpochCodec& codec_;
  const IMeshReceiptVerifier& receipts_;
  class Reader final:public runtime::IObservationEpochReplay {
    const runtime::IObservationEpochCodec& codec_;
    const IMeshReceiptVerifier& receipts_;
    runtime::IObservationEpochReplay* next_;
  public:
    Reader(const runtime::IObservationEpochCodec& codec,const IMeshReceiptVerifier& receipts,
      runtime::IObservationEpochReplay* next)noexcept:codec_(codec),receipts_(receipts),next_(next){}
    bool accept(std::span<const std::byte> bytes)noexcept override {
      runtime::Observation observation;
      return codec_.decode(bytes,observation)&&receipts_.verify_committed(observation)==ReplayCode::ok&&
        (!next_||next_->accept(bytes));
    }
    bool supports_checkpoint()const noexcept override{return next_&&next_->supports_checkpoint();}
    bool restore(const runtime::ObservationReplayCheckpoint& saved)noexcept override {
      if(!next_||saved.count>saved.last.size())return false;
      for(std::size_t i=0;i<saved.count;++i)if(receipts_.verify_committed(saved.last[i])!=ReplayCode::ok)return false;
      return next_->restore(saved);
    }
  };
  class Journal final:public runtime::IObservationEpochJournal {
    runtime::IObservationEpochJournal& source_;
    const runtime::IObservationEpochCodec& codec_;
    const IMeshReceiptVerifier& receipts_;
  public:
    Journal(runtime::IObservationEpochJournal& source,const runtime::IObservationEpochCodec& codec,
      const IMeshReceiptVerifier& receipts)noexcept:source_(source),codec_(codec),receipts_(receipts){}
    bool commit(const runtime::Observation&,std::span<const std::byte>)noexcept override{return false;}
    bool replay(runtime::IObservationEpochReplay& next)noexcept override {
      Reader verified(codec_,receipts_,&next);return source_.replay(verified);
    }
  };
public:
  MeshRecoveryService(const runtime::IObservationEpochCodec& codec,const IMeshReceiptVerifier& receipts)noexcept:
    codec_(codec),receipts_(receipts){}
  bool recover(runtime::IObservationEpochJournal& source,runtime::IObservationEpochProjection& projection,
    runtime::IObservationEpochCodec& codec)noexcept {
    Journal verified(source,codec_,receipts_);
    runtime::EpochObservationDeliveryService recovery(codec,verified,projection);return recovery.recover();
  }
  bool audit(runtime::IObservationEpochJournal& full_history)noexcept {
    Reader reader(codec_,receipts_,nullptr);return full_history.replay(reader);
  }
};
} // namespace anaiero::mesh

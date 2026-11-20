#include "anaiero/sdk/storage_tools.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace sdk = anaiero::sdk;
namespace ev = anaiero::sdk::evidence;

namespace {

void expect(bool satisfied, std::string_view message) {
  if (!satisfied) throw std::runtime_error(std::string(message));
}

void require(const sdk::Result& result, std::string_view action) {
  if (!result) {
    throw std::runtime_error(std::string(action) + ": " +
                             std::string(sdk::code_name(result.code)) + " " + result.message);
  }
}

class TestDirectory {
public:
  explicit TestDirectory(std::string_view name) {
    auto token = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    path = fs::temp_directory_path() / ("anaiero-packed-" + std::string(name) + "-" + token);
    expect(fs::create_directory(path), "test store must be a new directory");
  }
  ~TestDirectory() {
    std::error_code ignored;
    fs::remove_all(path, ignored);
  }
  fs::path path;
};

using Store = std::unique_ptr<ev::ICommitStore>;

Store open(const fs::path& directory, ev::IndexLayout layout = ev::IndexLayout::automatic) {
  ev::StoreOptions options;
  options.index_layout = layout;
  Store store;
  require(ev::open_native_store(directory.string(), options, store), "open store");
  return store;
}

ev::CommitBatch request(std::string id) {
  ev::CommitBatch batch;
  batch.operation_id = std::move(id);
  batch.request_digest = sdk::sha256(batch.operation_id);
  return batch;
}

ev::CommitReceipt commit(ev::ICommitStore& store, const ev::CommitBatch& batch) {
  ev::CommitReceipt receipt;
  require(store.commit(batch, receipt), batch.operation_id);
  return receipt;
}

ev::CompactionPlan review(ev::ICommitStore& store, std::string operation,
                          ev::IndexLayout layout) {
  ev::CompactionOptions options;
  options.operation_id = std::move(operation);
  options.expected_generation = store.generation();
  options.expected_root = store.info().root_digest;
  options.target_index_layout = layout;
  ev::CompactionPlan plan;
  require(ev::preview_native_compaction(store, options, plan), "review compaction");
  return plan;
}

void compact(ev::ICommitStore& store, const ev::CompactionPlan& plan) {
  ev::CompactionReport report;
  require(ev::execute_native_compaction(store, plan, report), "execute reviewed compaction");
  expect(report.activated && report.cleanup_complete, "compaction must durably finish");
}

void audit(ev::ICommitStore& store) {
  ev::AuditReport report;
  require(store.audit(report), "audit complete canonical and derived histories");
}

void exact_retry(ev::ICommitStore& store, const ev::CommitBatch& original,
                 const ev::CommitReceipt& first) {
  const auto generation = store.generation();
  ev::CommitReceipt stored;
  require(store.lookup_operation(original.operation_id, stored), "find original receipt");
  const auto retried = commit(store, original);
  expect(retried.duplicate, "retry acknowledged an existing operation");
  for (const auto* receipt : {static_cast<const ev::CommitReceipt*>(&stored), &retried}) {
    expect(receipt->operation_id == first.operation_id &&
           receipt->request_digest == first.request_digest &&
           receipt->batch_digest == first.batch_digest &&
           receipt->root_digest == first.root_digest &&
           receipt->generation == first.generation &&
           receipt->admitted_unix_s == first.admitted_unix_s,
           "historical receipt changed across repair or layout switch");
  }
  expect(store.generation() == generation, "an exact retry must not advance history");
}

void check_layout(ev::ICommitStore& store, ev::IndexLayout layout) {
  const auto info = store.info();
  const std::string expected = layout == ev::IndexLayout::packed_pages
                                 ? "native-packed-pages/1" : "native-sealed-avl";
  expect(info.index_ready && info.writable && info.index_backend == expected,
         "reopened store must have the expected healthy index layout");
}

void legacy_fixture(const fs::path& input) {
  TestDirectory temporary("legacy");
  const auto directory = temporary.path / "store";
  fs::copy(input, directory, fs::copy_options::recursive);
  auto store = open(directory);
  check_layout(*store, ev::IndexLayout::single_node);
  expect(store->info().store_id == "8fdab8c57962b162064f856a3e62adce1a1f901f07a80ad2a28e056d752e987a",
         "expected store written by the pre-packed runtime");
  auto original = request("old-flat-receipt");
  const std::string bytes = "fixture payload made by the pre-packed runtime";
  const auto digest = sdk::sha256(bytes);
  original.blobs.push_back({digest, bytes, {}});
  original.records.push_back({"legacy", "row", "old committed value", {digest}, false});
  ev::CommitReceipt first;
  require(store->lookup_operation(original.operation_id, first), "read pre-packed receipt");
  expect(first.generation == 1 && first.admitted_unix_s == 1690000000 &&
         first.root_digest == "b905fd236c5ead64c66fc713ad319b5e654b41461e47688064e504804dee3946" &&
         first.batch_digest == "3508f3a5433cfd3ffa2bdd3f34206756cc2007b1147dad15fded4ffc5183de0d",
         "pre-packed receipt must match its historical witness");
  exact_retry(*store, original, first);

  for (auto target : {ev::IndexLayout::packed_pages, ev::IndexLayout::single_node}) {
    auto plan = review(*store, target == ev::IndexLayout::packed_pages ? "to-packed" : "to-single", target);
    auto altered = plan;
    altered.options.target_index_layout = target == ev::IndexLayout::packed_pages
                                         ? ev::IndexLayout::single_node : ev::IndexLayout::packed_pages;
    ev::CompactionReport rejected;
    expect(!ev::execute_native_compaction(*store, altered, rejected),
           "reviewed layout may not change at execution");
    compact(*store, plan);
    store.reset();
    store = open(directory);
    check_layout(*store, target);
    expect(store->info().index_ready &&
           ev::execute_native_compaction(*store, altered, rejected).code == sdk::Code::conflict,
           "a completed review must reject a retry that names the opposite layout");
    exact_retry(*store, original, first);
    sdk::Bytes retained;
    require(store->get_blob(digest, retained), "read pre-packed payload");
    expect(store->info().root_digest == first.root_digest && retained == bytes,
           "the original canonical root and payload must survive both switches");
    audit(*store);
  }
}

struct Probe {
  ev::CommitBatch pinned = request("pin-protected-history");
  ev::CommitReceipt pinned_receipt;
  sdk::Digest pinned_digest;
  sdk::Digest old_payload;
  sdk::Digest live_payload;
};

Probe populate(ev::ICommitStore& store, unsigned batches) {
  Probe state;
  const std::string protected_bytes = "protected canonical evidence";
  state.pinned_digest = sdk::sha256(protected_bytes);
  state.pinned.blobs.push_back({state.pinned_digest, protected_bytes, {}});
  state.pinned.records.push_back({"pins", "retained", "keep", {state.pinned_digest}, false});
  state.pinned_receipt = commit(store, state.pinned);

  auto expire = request("expire-first-observation");
  for (unsigned sequence = 1; sequence <= 2; ++sequence) {
    auto observation = request("observe-" + std::to_string(sequence));
    const auto payload = std::string("observation-payload-") + std::to_string(sequence);
    const auto receipt = std::string("observation-receipt-") + std::to_string(sequence);
    const auto payload_digest = sdk::sha256(payload);
    const auto receipt_digest = sdk::sha256(receipt);
    observation.blobs = {{payload_digest, payload, {}}, {receipt_digest, receipt, {}}};
    observation.observations.push_back({"test-source", 1, sequence, payload_digest, receipt_digest});
    commit(store, observation);
    if (sequence == 1) {
      state.old_payload = payload_digest;
      expire.expire_blobs = {payload_digest, receipt_digest};
    } else {
      state.live_payload = payload_digest;
    }
  }
  commit(store, expire);
  for (const auto& digest : expire.expire_blobs) {
    require(store.reclaim_expired_blob(digest, store.generation()), "reclaim expired fixture history");
  }
  for (unsigned group = 0; group < batches; ++group) {
    auto rows = request("rows-" + std::to_string(group));
    for (unsigned ordinal = 0; ordinal < 48; ++ordinal) {
      const auto key = std::to_string(100000U + 48U * group + ordinal);
      rows.records.push_back({"rows", key, "value-" + key, {}, false});
    }
    commit(store, rows);
  }
  return state;
}

void verify_authority(ev::ICommitStore& store, const Probe& state) {
  exact_retry(store, state.pinned, state.pinned_receipt);
  auto reject = request("try-to-expire-pinned");
  reject.expire_blobs = {state.pinned_digest};
  ev::CommitReceipt receipt;
  expect(store.commit(reject, receipt).code == sdk::Code::conflict,
         "retained pin must prevent new expiry after a switch");
  ev::ObservationClaim claim;
  expect(store.lookup_observation("test-source", 1, 1, claim).code == sdk::Code::history_expired,
         "the compacted observation floor must survive the switch");
  require(store.lookup_observation("test-source", 1, 2, claim), "find retained observation");
  expect(claim.payload_digest == state.live_payload, "retained observation changed");
  ev::BlobInfo expired;
  require(store.blob_info(state.old_payload, expired), "find expired blob identity");
  expect(!expired.present && expired.expired_generation != 0, "expired data was resurrected");
  audit(store);
}

std::vector<fs::path> index_files(const fs::path& path) {
  std::vector<fs::path> files;
  for (const auto& entry : fs::recursive_directory_iterator(path / "index")) {
    if (entry.is_regular_file()) files.push_back(entry.path());
  }
  return files;
}

void corrupt_page(const fs::path& path) {
  std::fstream page(path, std::ios::in | std::ios::out | std::ios::binary);
  expect(page.good(), "open a derived page for the corruption fixture");
  page.seekg(-1, std::ios::end);
  char byte = 0;
  page.get(byte);
  page.seekp(-1, std::ios::end);
  page.put(static_cast<char>(byte ^ 1));
  page.close();
  expect(page.good(), "change one byte while preserving file length");
}

void repair_damaged_pages() {
  TestDirectory temporary("repair");
  auto store = open(temporary.path);
  const auto state = populate(*store, 16);
  const auto single_files = index_files(temporary.path).size();
  compact(*store, review(*store, "larger-packed-build", ev::IndexLayout::packed_pages));
  const auto compacted_files = index_files(temporary.path);
  expect(compacted_files.size() > 1 && compacted_files.size() < single_files,
         "packed multi-page snapshot should use fewer files than this single-node fixture");
  auto suffix = request("suffix-after-packed-base");
  suffix.records.push_back({"suffix", "tail", "present after repair", {}, false});
  const auto suffix_receipt = commit(*store, suffix);
  const auto invariant = store->info();
  for (const bool remove_root : {false, true}) {
    const auto root_name = store->info().index_root + ".page";
    const auto pages = index_files(temporary.path);
    auto target = std::find_if(pages.begin(), pages.end(), [&](const fs::path& path) {
      return path.extension() == ".page" && (path.filename().string() == root_name) == remove_root;
    });
    expect(target != pages.end(), "an active root and at least one child page are required");
    store.reset();
    if (remove_root) {
      expect(fs::remove(*target), "remove named root page for the missing-file fixture");
    } else {
      corrupt_page(*target);
    }
    store = open(temporary.path);
    if (store->info().index_ready) {
      ev::AuditReport diagnostic;
      const auto result = store->audit(diagnostic);
      expect(!result && result.code != sdk::Code::not_found,
             "incomplete or corrupt pages must not audit as valid or imply absence");
    }
    require(store->repair_index(), "repair packed data from canonical base and suffix");
    check_layout(*store, ev::IndexLayout::packed_pages);
    expect(store->info().root_digest == invariant.root_digest &&
           store->generation() == invariant.generation, "derived repair changed canonical history");
    exact_retry(*store, suffix, suffix_receipt);
    verify_authority(*store, state);
  }
  ev::reset_native_index_statistics(*store);
  ev::StoredRecord midpoint;
  require(store->latest("rows", "100384", midpoint), "find middle record after repair");
  expect(midpoint.record.payload == "value-100384" &&
         ev::native_index_statistics(*store).nodes_read < 64, "point lookup must stay bounded");
  std::vector<ev::StoredRecord> rows;
  bool more = false;
  require(store->scan_latest("rows", "", "100381", 7, rows, more), "range over repaired pages");
  expect(rows.size() == 7 && more && rows.front().record.key == "100382" &&
         rows.back().record.key == "100388", "range must retain ordering and exclusive restart key");
}

void interrupted_switches() {
  for (auto cut : {ev::FaultPoint::compaction_after_base, ev::FaultPoint::compaction_during_index_build,
                   ev::FaultPoint::compaction_after_index, ev::FaultPoint::compaction_after_root,
                   ev::FaultPoint::compaction_during_cleanup}) {
    TestDirectory temporary("switch");
    auto store = open(temporary.path);
    const auto state = populate(*store, 2);
    const auto before = store->info();
    const auto plan = review(*store, "switch-after-cut", ev::IndexLayout::packed_pages);
    ev::set_native_store_fault(*store, cut);
    ev::CompactionReport interrupted;
    const auto result = ev::execute_native_compaction(*store, plan, interrupted);
    const bool published = cut == ev::FaultPoint::compaction_after_root ||
                           cut == ev::FaultPoint::compaction_during_cleanup;
    expect(result.code == (published ? sdk::Code::committed_degraded : sdk::Code::io_error),
           "fault result must identify whether the target base was published");
    store.reset();
    store = open(temporary.path);
    ev::CompactionReport resumed;
    require(ev::resume_native_compaction(*store, resumed), "reconcile interrupted switch");
    check_layout(*store, published ? ev::IndexLayout::packed_pages : ev::IndexLayout::single_node);
    expect(store->info().root_digest == before.root_digest && store->generation() == before.generation,
           "every interrupted switch must retain the original terminal identity");
    exact_retry(*store, state.pinned, state.pinned_receipt);
    compact(*store, plan);
    verify_authority(*store, state);
    expect(!fs::exists(temporary.path / "COMPACTION") &&
           store->info().pending_reclamation_bytes == 0, "resumed switch must settle cleanup");
  }
}

void empty_packed_base() {
  TestDirectory temporary("empty");
  auto store = open(temporary.path);
  const auto old = request("only-old-operation");
  commit(*store, old);
  ev::CompactionOptions options;
  options.operation_id = "rotate-to-empty-packed";
  options.target_index_layout = ev::IndexLayout::packed_pages;
  options.expire_operation_receipts = true;
  ev::CompactionPlan plan;
  require(ev::preview_native_compaction(*store, options, plan), "review empty target");
  compact(*store, plan);
  store.reset();
  store = open(temporary.path);
  check_layout(*store, ev::IndexLayout::packed_pages);
  expect(store->info().index_root.empty() && store->info().operation_epoch == 1,
         "empty packed index must preserve the activated operation epoch");
  ev::CommitReceipt ignored;
  expect(store->commit(old, ignored).code == sdk::Code::history_expired,
         "empty packed index must not admit an expired operation ID");
  auto first = request(ev::qualify_operation_id(1, "first-new-epoch"));
  first.records.push_back({"state", "new", "first row after empty snapshot", {}, false});
  const auto first_receipt = commit(*store, first);
  exact_retry(*store, first, first_receipt);
  audit(*store);
}

void interrupted_packed_commit() {
  for (auto cut : {ev::FaultPoint::before_index, ev::FaultPoint::after_index}) {
    TestDirectory temporary("append");
    auto store = open(temporary.path, ev::IndexLayout::packed_pages);
    check_layout(*store, ev::IndexLayout::packed_pages);
    auto item = request("packed-append-cut");
    item.records.push_back({"packed", "head", "exact payload", {}, false});
    ev::set_native_store_fault(*store, cut);
    ev::CommitReceipt interrupted;
    expect(store->commit(item, interrupted).code == sdk::Code::committed_degraded,
           "published canonical suffix reports degraded index publication");
    store.reset();
    store = open(temporary.path);
    if (!store->info().index_ready) require(store->repair_index(), "recover interrupted packed suffix");
    check_layout(*store, ev::IndexLayout::packed_pages);
    ev::CommitReceipt persisted;
    require(store->lookup_operation(item.operation_id, persisted), "recover packed suffix receipt");
    exact_retry(*store, item, persisted);
    expect(store->generation() == 1, "append cut must publish only one canonical transition");
    audit(*store);
  }
}
// This mode is invoked by a separate CMake driver. Leave the store open at
// process termination so only the durable files are available to the verifier.
void process_cut(const fs::path& directory, bool interrupt) {
  auto store = open(directory);
  if (interrupt) {
    const auto plan = review(*store, "process-switch", ev::IndexLayout::packed_pages);
    ev::set_native_store_fault(*store, ev::FaultPoint::compaction_after_root);
    ev::CompactionReport result;
    const auto stopped = ev::execute_native_compaction(*store, plan, result);
    expect(stopped.code == sdk::Code::committed_degraded && result.activated &&
           !result.cleanup_complete, "producer must stop between ROOT and coverage publication");
    std::_Exit(77);
  }
  ev::CompactionReport resumed;
  require(ev::resume_native_compaction(*store, resumed), "recover in an independent process");
  check_layout(*store, ev::IndexLayout::packed_pages);
  ev::CommitReceipt first;
  require(store->lookup_operation("old-flat-receipt", first), "recover original receipt");
  expect(first.root_digest == "b905fd236c5ead64c66fc713ad319b5e654b41461e47688064e504804dee3946" &&
         store->generation() == 1 && first.generation == 1 && !fs::exists(directory / "COMPACTION"),
         "recovery must keep published layout, original root and one accepted operation");
  auto suffix = request("after-independent-reopen");
  suffix.records.push_back({"legacy", "row", "second value", {}, false});
  suffix.guards.push_back({"legacy", "row", 1, {}});
  const auto receipt = commit(*store, suffix);
  exact_retry(*store, suffix, receipt);
  audit(*store);
}
} // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 3) {
      const std::string_view mode = argv[1];
      expect(mode == "--cut" || mode == "--recover", "unknown recovery test mode");
      process_cut(argv[2], mode == "--cut");
      return 0;
    }
    expect(argc == 2, "expected closed historical store fixture directory");
    legacy_fixture(argv[1]);
    empty_packed_base();
    interrupted_packed_commit();
    interrupted_switches();
    repair_damaged_pages();
    std::cout << "Packed layout migration, commit/switch cuts, protected history and damaged-page repairs passed\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << exception.what() << '\n';
    return 1;
  }
}

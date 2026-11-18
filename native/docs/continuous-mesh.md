# Continuous canonical mesh

The optional native mesh exchanges committed evidence directly from the same
canonical store used by a running modular session. The session host, mesh
policy and delivery services share one serialized storage lane. A committed
engine transition signals an availability watermark; caught-up subscribers
wait for commits, socket activity or lifecycle deadlines instead of polling
historical storage.

This guide uses **`CEVMESH/2`** with TLS ALPN **`anaiero-mesh/2`**. The earlier
`CEVMESH/1` service and its `PERSISTED` meaning remain separate. Native Windows
and Linux are supported. The mesh build requires OpenSSL 3; the ordinary
controller remains independent of that optional dependency.

## Programs and ownership

| Public command | Direct native executable | Purpose |
| --- | --- | --- |
| `anaiero session …` | `ce_modular_session …` | Initialize, inspect and maintain the canonical engine session |
| `anaiero host …` | `ce_modular_host …` | Run the session and optional authenticated listener |
| `anaiero mesh-live …` | `ce_modular_mesh …` | Enroll peers, grant exact scopes, follow evidence and transfer artifacts/checkpoints |

On Windows the executable filenames end in `.exe`. The direct programs take
exactly the arguments following their public command family; for example,
`anaiero mesh-live certificate a.crt` is `ce_modular_mesh certificate a.crt`.
Build with `-DANAIERO_ENABLE_MESH=ON`. See the
[native mesh workflow](../../.github/workflows/native-mesh.yml) for the native
Windows OpenSSL toolchain and the
[host deployment guide](../../deployment/native/README.md) for supervision,
installation and package compatibility.

One process owns each store's native writer lock. `local STORE …` and the
standalone `follow`, `artifact` and `bootstrap` commands acquire that ownership.
Use live `admin ENDPOINT …` for a store already owned by `ce_modular_host`.
Stop a standalone receiver before opening its store for another local command.
The SDK can also attach `NativeSubscriber` to a running host's
`IStoreTaskExecutor`; the command-line host currently configures a listener,
not an outbound subscriber inside that same process.

## Credentials and exact source context

Prepare approved CA-issued server/client leaf certificates and their private
keys. The [existing two-node credential example](../../examples/mesh/README.md)
contains Bash and native PowerShell certificate preparation using OpenSSL.
Its `a.crt`, `a.key`, `b.crt`, `b.key` and `ca.crt` files can be used here. The
canonical enrollment and configuration commands below apply to this service.
Private keys remain credential files; they are not evidence artifacts.

Inspect each leaf certificate with the native command:

```text
anaiero mesh-live certificate /absolute/credentials/a.crt
anaiero mesh-live certificate /absolute/credentials/b.crt
```

The result is `CERTIFICATE SHA256 EXPIRES UNIX_SECONDS`. Pins are the lowercase,
colon-free SHA-256 of the exact leaf certificate. Enrollment expiry and
subscription leases also use absolute Unix seconds. Choose future deadlines
within the intended operating interval and certificate validity. Both the
certificate and local enrollment remain authorization requirements.

A source grant contains the complete encoded engine identity: source UID and
epoch, clock, engine/model/module identity and behavior context. Obtain its
lowercase hex encoding from the source store using `scope`; do not construct a
grant by guessing the model or copying only the source name.

For A's evidence to flow to B:

| Policy stored at | Peer principal | Certificate | Scope direction |
| --- | --- | --- | --- |
| A | `node-b` | B's client certificate | `send` A's exact source context |
| B | `node-a` | A's server certificate | `receive` the identical source context |

The receiver also pins A's certificate on the connection. CA validation alone
does not grant a source or artifact. A receive grant cannot take over a locally
owned source UID. Local engine ownership and authenticated relay ownership use
a common durable authority fence; use distinct local source UIDs when both
nodes also run their own engines.

## Two-node setup

Run the following from the repository root. These examples use Bash variable
syntax and an existing credential directory. Choose fresh store directories.
On Windows use a short endpoint name such as `anaiero-canonical-a` in place of
the Unix socket pathname, and assign the corresponding PowerShell variables
and native executable paths.

```bash
mesh_credentials="/absolute/path/to/credentials"
mesh_demo="$PWD/canonical-mesh-demo"
mkdir -p "$mesh_demo"
mesh_a_store="$mesh_demo/a-state"
mesh_b_store="$mesh_demo/b-state"
mesh_a_endpoint="$mesh_demo/a-control.sock"
mesh_server_config="$mesh_demo/a.mesh"
mesh_client_config="$mesh_demo/b.mesh"

read -r mesh_a_label mesh_a_pin mesh_a_expiry_label mesh_a_cert_expiry < <(
  anaiero mesh-live certificate "$mesh_credentials/a.crt")
read -r mesh_b_label mesh_b_pin mesh_b_expiry_label mesh_b_cert_expiry < <(
  anaiero mesh-live certificate "$mesh_credentials/b.crt")
mesh_now="$(date -u +%s)"
mesh_expiry="$((mesh_now + 3600))"
mesh_lease="$((mesh_now + 1800))"

anaiero session init examples/runtime/modular-native.cevsession "$mesh_a_store"
anaiero mesh-live init "$mesh_b_store" 1073741824
mesh_context="$(anaiero mesh-live local "$mesh_a_store" scope source-environment 1)"

anaiero mesh-live local "$mesh_a_store" peer node-b "$mesh_b_pin" 1 0 "$mesh_expiry" active enroll-b-1
anaiero mesh-live local "$mesh_b_store" peer node-a "$mesh_a_pin" 1 0 "$mesh_expiry" active enroll-a-1
anaiero mesh-live local "$mesh_a_store" grant node-b "$mesh_context" 1 0 send bootstrap grant-send-1
anaiero mesh-live local "$mesh_b_store" grant node-a "$mesh_context" 1 0 receive bootstrap grant-receive-1
```

The supplied modular session has `source-environment` at epoch `1`. For another
session, use its actual source UID and epoch. `session init` retains the model
assets and attaches the engines, so this source context exists before the host
starts. `mesh-live init` creates a canonical relay store with a persisted byte
budget and mesh runtime marker; it refuses an existing nonempty directory.

In `peer` and `grant`, the two numbers after the certificate/context are
`GENERATION EXPECTED_GENERATION`. Creation uses `1 0`; each policy change
advances exactly once, for example `2 1`. A stable operation ID identifies the
specific change. Repeat that ID only for the same intended operation.

### Configuration files

Write the server and client configuration files:

```bash
cat > "$mesh_server_config" <<EOF_SERVER
ANAIEROMESH/2
certificate=$mesh_credentials/a.crt
private_key=$mesh_credentials/a.key
authority=$mesh_credentials/ca.crt
address=127.0.0.1
port=40451
clients=8
handshakes=4
handshake_timeout_ms=5000
io_timeout_ms=5000
EOF_SERVER

cat > "$mesh_client_config" <<EOF_CLIENT
ANAIEROMESH/2
certificate=$mesh_credentials/b.crt
private_key=$mesh_credentials/b.key
authority=$mesh_credentials/ca.crt
address=127.0.0.1
port=40451
EOF_CLIENT
```

For separate machines, use the listener's numeric IPv4/IPv6 address and the
matching reachable remote address. Use absolute credential paths; they resolve
from the process working directory, not from the configuration file location.
The file is UTF-8 without a byte-order mark, starts with the exact version line,
and uses `key=value` without surrounding whitespace or shell quotes. A path's
spaces are part of its value. Blank lines and lines beginning with `#` are
accepted; duplicate or unknown settings are rejected. PowerShell can write
UTF-8 without BOM with `System.Text.UTF8Encoding($false)`.

The parser accepts only the nine keys shown above. Server `port=0` asks the OS
for an available port; obtain it with `MESH_STATUS` and put that nonzero port in
the client configuration. Client commands read credential, address and port
settings from this file; their connection and I/O timeouts remain the SDK
subscriber defaults. Server-only client/handshake settings do not configure a
standalone subscriber's retry policy.

### Start the shared session and listener

In A's terminal:

```bash
anaiero host start "$mesh_a_endpoint" "$mesh_a_store" 100 paused --mesh "$mesh_server_config"
```

This is a foreground process. `paused` initially disables automatic cycles
while allowing control and mesh work; any previously committed cycle intent
still completes. From another terminal with the same variables:

```bash
anaiero host query "$mesh_a_endpoint" PING
anaiero host query "$mesh_a_endpoint" MESH_STATUS
anaiero host query "$mesh_a_endpoint" THROUGH 2
anaiero host query "$mesh_a_endpoint" STATUS
```

Wait until `STATUS` reports the requested completed cycle count and no pending
cycle. `THROUGH` takes an absolute cycle target; its immediate ticket is queue
admission, not proof that all cycles have finished. `RUN` enables repeated
cycles and `PAUSE` disables automatic scheduling. The 100 ms period is host
scheduling time and does not alter the engines' declared logical clocks.

### Follow and resume

B can first catch up through at least one original observation:

```bash
anaiero mesh-live follow "$mesh_b_store" "$mesh_client_config" node-b node-a "$mesh_a_pin" source-environment 1 env-feed 1 "$mesh_lease" through 1
```

The finite target is an observation sequence, not a session cycle. A complete
page can end beyond that target. Success prints `RECEIVED SEQUENCE SHA PAGES N
RECONNECTS N`. To continue until stopped, use the same subscription with
`through 0`:

```bash
anaiero mesh-live follow "$mesh_b_store" "$mesh_client_config" node-b node-a "$mesh_a_pin" source-environment 1 env-feed 1 "$mesh_lease" through 0
```

The receiver commits its relay evidence and exact cursor before ACK. The sender
then durably records that exact offered boundary. Repeating the same generation
reconciles the receiver's persisted cursor and the sender's ACK after a broken
connection; a stale or mismatched ACK cannot skip a page. Transient transport
failures use bounded reconnect backoff. Authority, corruption, incompatible
history and bootstrap failures block the subscriber for explicit resolution.
Use Ctrl+C to stop a standalone continuous receiver and release its writer.

Reusing a live subscription does not implicitly renew an expired lease. Use the
lifecycle administration rules in [modular lifecycle](modular-lifecycle.md) or
an explicit new bootstrap generation when continuity requires it. While B is
stopped, inspect its last durable receive result:

```bash
anaiero mesh-live local "$mesh_b_store" received node-a env-feed source-environment 1
```

## Live administration and completion

The same `scope`, `peer`, `grant`, `received` and `cancel-bootstrap` verbs are
available through the host's local IPC:

```text
anaiero mesh-live admin ENDPOINT scope SOURCE EPOCH
anaiero mesh-live admin ENDPOINT peer PRINCIPAL CERT_SHA GEN EXPECTED EXPIRY active|suspended|revoked OPERATION
anaiero mesh-live admin ENDPOINT grant PRINCIPAL CONTEXT_HEX GEN EXPECTED send|receive|both|closed bootstrap|ordinary OPERATION [ARTIFACT_SHA...]
anaiero mesh-live admin ENDPOINT received PRINCIPAL SUBSCRIPTION SOURCE EPOCH
anaiero mesh-live result ENDPOINT TICKET
```

`admin` returns `{"ticket":N,"queued":true,"durable":false}` after bounded
admission. Use `result` until `complete:true`, then inspect `code`, `durable`
and `response`. Only completed committed mutations have `durable:true`;
a successful read such as `scope` has `durable:false`. The recent result window
holds at most 32 entries and can evict completed tickets. After restart or
eviction, reconcile a mutation using its original stable operation identity;
the transient ticket itself is not a durable receipt.

For example, revoke B at A using peer generation `2`, expected generation `1`
and a new operation ID, then inspect the returned ticket:

```bash
anaiero mesh-live admin "$mesh_a_endpoint" peer node-b "$mesh_b_pin" 2 1 "$mesh_expiry" revoked revoke-b-2
```

Revocation, grant changes, credential generation and expiry are checked again
before an outbound page/chunk is handed to transport, and before canonical
receive/ACK mutations. Bytes already handed to transport cannot be recalled.
A host clock behind the durable admission-time floor fails closed. Enrollment
and grants are local operator actions; the remote TLS protocol has no admin
verb or remote engine-control verb.

Certificate rotation requires reviewed peer generations and exact new pins.
Replace the configured credential files through the deployment's credential
procedure and restart the listener/client using them; this implementation does
not reload TLS credential files automatically. Rotation does not transfer an
existing source's ownership to a different principal.

## Authorized artifacts

An artifact must already exist in A's canonical blob store and its exact SHA-256
must appear in both relevant source grants. To update the example grants,
advance them from `1 0` to `2 1`, retain the full context/directions, and append
the digest. Apply A's change through live `admin` and B's through `local` while
B is stopped. Include every artifact that should remain authorized: the list
replaces that grant's prior artifact list.

The mesh command has no artifact-import verb. Existing module artifacts can be
inspected with `anaiero session module-packages STORE` while its host is
stopped, or an SDK producer can commit another immutable object and its
protection root. Transfer a granted object with:

```text
anaiero mesh-live artifact B_STORE B_CONFIG node-b node-a A_CERT_SHA SOURCE EPOCH SUBSCRIPTION GENERATION LEASE digest ARTIFACT_SHA
anaiero session export-artifact B_STORE ARTIFACT_SHA NEW_OUTPUT_FILE
```

Chunks are bounded and reauthorized. The receiver verifies the complete digest
before committing the artifact and its receipt. Success prints
`ARTIFACT_COMMITTED RECEIPT_SHA GENERATION STORE_GENERATION`. In this initial
profile, assembling a finite artifact uses a bounded buffer up to 8 MiB.

## Explicit bootstrap after a history floor

Bootstrap requires `bootstrap` permission in both exact source grants and the
next subscription generation. It installs the original source checkpoint plus
a durable relay cursor atomically; it does not invent received observations or
acknowledge delivery of expired original events.

First stop B's follower. Pause A, wait for the PAUSE ticket to complete and for
`STATUS` to show no pending cycle. Keep the source quiescent through offer,
installation and acceptance: a changed source tip/checkpoint makes the transfer
conflict explicitly. For the existing `env-feed` generation `1`:

```bash
anaiero host query "$mesh_a_endpoint" PAUSE
# Inspect RESULT for the returned ticket, then STATUS before continuing.
anaiero mesh-live bootstrap "$mesh_b_store" "$mesh_client_config" node-b node-a "$mesh_a_pin" source-environment 1 env-feed 1 "$mesh_lease" next 2
anaiero mesh-live follow "$mesh_b_store" "$mesh_client_config" node-b node-a "$mesh_a_pin" source-environment 1 env-feed 2 "$mesh_lease" through 0
```

Resume A with `RUN` from another terminal after bootstrap acceptance. A completed
bootstrap prints `BOOTSTRAP_INSTALLED RECEIPT_SHA GENERATION STORE_GENERATION`.
If the final acceptance reply was lost, repeat the identical bootstrap request;
the receiver's durable installation is reconciled with the sender rather than
advancing again. Installation alone is distinct from a completed network
acceptance exchange.

A bootstrap reader pin protects its checkpoint until acceptance or explicit
release. After cancelling the relevant transport, trusted local maintenance
can release a known abandoned offer with:

```text
anaiero mesh-live admin ENDPOINT cancel-bootstrap OFFER_ID OPERATION
```

Lease expiry alone does not assert that a reader released its resources. The
current CLI does not provide an offer-listing command; use a retained offer ID
from SDK inspection rather than inventing one.

## Limits, retention and recovery

| Resource | Default native profile |
| --- | --- |
| Authenticated connections | 8; configuration permits 1–32 |
| Concurrent unauthenticated handshakes | 4; configuration permits 1–16 |
| Server handshake / I/O timeout | 5,000 ms each; valid range 1–60,000 ms |
| Delivery page | At most 16 observations and 24,576 bytes |
| Outstanding delivery window | One exact page per connection |
| Artifact | At most 8 MiB; at most 32 granted digests per scope |
| Standalone storage task queue | 8 waiting jobs |
| Live admin results | 32 recent tickets; bounded command/reply payloads |
| Standalone reconnect backoff | 100 ms to 5,000 ms |
| Default relay store quota | 1 GiB accounted bytes, with ordinary maintenance reserve |

The SDK exposes additional explicit page/scheduler bounds; they are not extra
configuration-file keys. Scheduling charges bytes and read/verification work,
with shared principal/source budgets and a reserved bounded control path.
A slow handshake does not occupy the sole accept path; a peer withholding an
ACK owns only its bounded outstanding page. These are bounded operating rules,
not an unlimited-throughput or hard filesystem-latency guarantee.

Relay observations, receive receipts, projections, artifact receipts and
bootstrap installation records are canonical retained evidence. Their latest
records protect required payloads. This batch does not automatically expire
remote relay payloads or historical identity records. Finite quota therefore
stops admission unless the operator provides supported retention/capacity
policy. Sender retention still respects its lifecycle subscriptions and pins.

[Default canonical compaction](canonical-evidence.md) is supported and retains
operation receipts. Built-in mesh operation requires receipt epoch **zero** and
rejects explicitly expired operation namespaces. Do not relabel an uncertain
old mesh operation into a fresh epoch. After a backup restore, keep quarantine
until current policy is explicitly reauthorized. Renew peer authority and
recreate grants with a new operation and generation bound to the current store
incarnation; replaying an old grant operation cannot restore current authority.
Rebootstrap subscription continuity where required.

| Result | Operator meaning |
| --- | --- |
| `unauthorized` / `expired` | Current certificate, peer, grant, incarnation, time floor or lease does not authorize the operation |
| `conflict` | Revision/generation/context/cursor changed, or the request disagrees with its original durable identity |
| `capacity` | A declared queue, page, artifact, active directory, working-space or storage budget was exhausted |
| `history_expired` / `rebootstrap_required` | Original replay cannot continue from that cursor; inspect the floor and authorize explicit bootstrap |
| `archive_required` / `archive_unavailable` | Archive retrieval is needed or unavailable; neither means an empty successful history page |
| `committed_degraded` | The canonical effect committed but recovery/derived work remains; reconcile its durable identity before retrying |
| `corrupt` | Verification failed; repair a derived index only from intact canonical authority |
| `unsupported` | Protocol/profile or explicit operation-epoch mode is incompatible with this implementation |

For shutdown use `anaiero host query ENDPOINT STOP` and allow accepted work to
drain before offline audit, repair, compaction or restore. The cached
`MESH_STATUS` reports active handshakes/clients, queued work, availability,
delivered/acknowledged pages, byte charges and owner wakeups without scanning
the store. `anaiero session audit STORE` inspects canonical evidence while the
host is stopped.

TLS receipts authenticate the relay connection and preserve its attribution.
They do not establish an independently signed original author. Discovery,
QUIC, transferable author signatures, remote experiments and arbitrary engine
control are outside this protocol. Native integration tests exercise shared
session commits, parallel handshakes, slow ACKs, ALPN rejection, artifact
verification, bootstrap/retry, revocation and local IPC; sustained release
soaks and device power-loss qualification remain separately declared gates.

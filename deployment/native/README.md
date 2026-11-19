# Native host deployment

## Canonical modular host

For the compiled SDK sessions in CE-SW-38, use **`ce_modular_host`**. Initialize
the canonical session once, then run its native host directly under the
supervisor:

```sh
anaiero session init examples/runtime/modular-native.cevsession /absolute/state/modular
ce_modular_host start /absolute/runtime/control.sock /absolute/state/modular 100
anaiero host query /absolute/runtime/control.sock STATUS
anaiero host query /absolute/runtime/control.sock PAUSE
anaiero host query /absolute/runtime/control.sock THROUGH 20
anaiero host query /absolute/runtime/control.sock ENGINE_PAUSE environment pause-environment-1
anaiero host query /absolute/runtime/control.sock STOP
```

The period is wall monotonic milliseconds between completed cycles; it never
changes an engine's logical clock. Missed deadlines do not accumulate catch-up
work. `paused` after the period starts with automatic scheduling disabled.
An already committed cycle intent is completed even in paused mode. `RUN` and
`PAUSE` affect automatic scheduling; `THROUGH` requests an absolute cycle
target, with at most 10,000 new cycles in one request. A process restart retains
committed cycle progress and intent, but does not preserve the host's transient
run/through request. The service command defines its startup mode.

Local admission returns a ticket with `durable:false`. `RESULT ticket` reports
completion for the bounded recent ticket window. Engine controls require a
stable operation ID and reconcile through canonical evidence; repeating the
same operation cannot apply the transition twice. A run/pause/through ticket
records scheduling acceptance, not completion of all requested cycles.
`STATUS [engine-offset]` reads an immutable cached page of up to eight engines
while storage or model work is active. It reports pending cycles, canonical
generation, accounted quota, queue counts and owner wakeups. It does not expose
checkpoint payloads through the 16 KiB local frame.

Shutdown stops new admission, completes the active durable cycle and accepted
storage tasks, and returns explicit cancellation for queued local controls.
The stop path has reserved signaling capacity even when both work queues are
full. If storage has already failed, shutdown leaves the exact pending intent
for explicit recovery instead of reporting a completed cycle. The serial
domain owner and storage worker are separate from the IPC listener. A slow
storage task cannot turn a cached status request into a filesystem scan.

The `anaiero-modular.service` example uses systemd user service notification and
the signal drain path. It expects an initialized store at
`~/.local/state/anaiero/modular`. The Windows helper
`manage-modular-service.ps1` registers the real `ce_modular_host.exe` process as
`AnaieroModular`, requires an explicit account, runs a quiescent compatibility
check, and uses manual startup. Its Remove action removes the registration and
preserves evidence. Same-user local IPC means the client must run as that
service account.

### Package and upgrade verification

The [reviewed package lifecycle](package-lifecycle.md) now supplies native
`package-review`, `package-activate`, `package-rollback-review`, `package-status`
and `check-selected` commands. Selection is an atomic canonical transition;
rollback checks the previous compatible package against the current store and
preserves accepted work. A reviewed compatibility-binding migration admits
different exact builds only when their complete runtime/state contract witnesses
match. The managed systemd/SCM helpers switch stopped registrations, recover
interrupted switches and preserve canonical state on uninstall. Explicit
package witness budgets, pins and reviewed reclamation bound retained payloads.
Use the linked package guide for the exact native commands and supervisor steps.

Build a complete native installation before packaging. The packaging recipe
uses CMake and the installed native host; it does not require Python:

```sh
cmake -DANAIERO_BUILD_DIR=/absolute/build \
  -DANAIERO_PACKAGE_DIR=/absolute/releases/anaiero-candidate \
  -DANAIERO_CONFIG=Release \
  -DANAIERO_PROFILE=native-canonical-functional \
  -P native/cmake/BuildNativePackage.cmake
cmake -DANAIERO_PACKAGE_DIR=/absolute/releases/anaiero-candidate \
  -P native/cmake/VerifyNativePackage.cmake
```

The manifest inventories every installed file by relative path, SHA-256 and
byte count, and records the exact compiled builtin identity and profile.
Verification rejects missing, altered, extra and symlink entries, then executes
the installed host's identity check. A manifest digest is package integrity,
not an independently authenticated publisher signature. System libraries and
optional OpenSSL remain dependencies of the declared native installation;
this recipe does not claim to bundle an arbitrary operating-system runtime.
An optional `ANAIERO_ARCHIVE` creates a tar.gz transport artifact without
overwriting an existing destination.

Install the candidate beside the current package. Stop the old host, review and
activate its exact package plan, then use the selected executable in the
supervisor command. Optional verification with
`-DANAIERO_STORE_DIR=/absolute/state/modular` also checks retained-state
compatibility. Old precontract builds require unchanged compiled identity.
New packages can migrate the exact build binding without changing logical
engine identities or checkpoint bytes; a changed engine/storage/session
implementation remains unsupported without a separately qualified semantic
migration. Managed selection retains host/manifest/build witnesses in the
canonical store. Unmanaged hosts retain their original independent pin.

Preserve the previous package and a consistent recovery copy. After new
canonical writes have been acknowledged, reverting an old backup would lose
the suffix and is not an upgrade rollback. Keep the new evidence available to
a compatible binary or migrate it explicitly. Uninstalling a service or
package does not remove canonical evidence.

## Earlier runtime host

The installed `ce_colony_runtime` backend is the long-running process. Use it
directly under an operating-system supervisor so service control and signals
reach the process that owns the session. `anaiero` remains the public command
dispatcher for interactive use.

## Linux user service

The example `anaiero.service` runs under the current user. Put installed native
binaries in `~/.local/lib/anaiero/bin`, the session manifest in
`~/.config/anaiero/session.cevsession`, and session state in
`~/.local/state/anaiero/session`. Create these directories and
`~/.local/share/anaiero`, or edit the unit paths to match the installation.
Use absolute paths for engine model/configuration files in the manifest.

Copy the unit into `~/.config/systemd/user/anaiero.service`, then run:

```sh
systemctl --user daemon-reload
systemctl --user start anaiero.service
systemctl --user status anaiero.service
anaiero runtime query "$XDG_RUNTIME_DIR/anaiero/control.sock" STATUS
systemctl --user stop anaiero.service
```

Enable automatic startup only when desired with
`systemctl --user enable anaiero.service`. The host sends `READY=1` after the
local endpoint is listening and sends `STOPPING=1` before its shutdown hook.
SIGINT and SIGTERM wake the listener through a self-pipe. The shutdown hook runs
on the host owner, where it can drain and join the session worker.

## Windows SCM service

The host implements a real SCM dispatcher and stop/shutdown control handler.
The service binary must be **`ce_colony_runtime.exe service ...`**; registering
`anaiero.exe` would register a dispatcher that launches the actual host as a
child process.

In an elevated PowerShell window, use the installation helper with an explicit
service account:

```powershell
$account = Get-Credential
.\manage-service.ps1 -Action Install `
  -Executable 'C:\Anaiero\bin\ce_colony_runtime.exe' `
  -Config 'C:\Anaiero\session.cevsession' `
  -StateDirectory 'C:\Anaiero\state' `
  -Endpoint 'anaiero-control' -Credential $account
.\manage-service.ps1 -Action Start
anaiero runtime query anaiero-control STATUS
.\manage-service.ps1 -Action Stop
```

The named-pipe client must run as the same account as the service. Select the
intended account, give it Windows' **Log on as a service** right, and give it
read access to installed files and write access to the state directory.
Installation uses manual startup and never defaults to LocalSystem. The helper
supports explicit removal with `-Action Remove`; it does not delete state.

Console mode handles Ctrl+C through the same stop path. SCM reports start,
running, stop-pending and stopped states. The application stop hook executes
outside the SCM control callback and can finish accepted work before the
service reports stopped.

## Local transport contract

- Same-user identity is checked before dispatch: Linux `SO_PEERCRED`, Windows
  access-token SID comparison. Remote named-pipe clients are rejected.
- Commands are one newline-terminated line of up to 4 KiB; replies are bounded
  to 16 KiB including the final newline. Larger state queries must paginate.
- Each accepted request has an absolute I/O deadline; individual bytes do not
  renew it. Client calls also have one deadline across connection/write/read.
- A malformed, abandoned or stalled client closes its own connection. The
  listener remains available. Stop cancellation wakes both idle accept and
  incomplete client reads.
- Windows clients acknowledge a complete reply before pipe disconnect. This
  avoids an unbounded `FlushFileBuffers` and protects buffered reply delivery.
  Missing acknowledgments time out within the same request deadline.
- A Windows server holds one first-instance pipe handle for its lifetime. A
  Linux server holds a same-owner endpoint lock, probes an existing socket and
  only removes a same-owner stale socket whose inode has not changed; an
  active endpoint is left in place. The `.lock` inode remains after shutdown
  so concurrent restarts cannot acquire different inodes for the same name.

The IPC host accepts one connection at a time. Its command service must enqueue
engine/optimization work and return cached state without performing long solver
or storage work in the transport owner. A transport deadline cannot preempt
arbitrary C++ business logic executing inside `ICommandService::handle`.

`NativeHostLifecycle::run` is the composition entry point. It does not install
services, create accounts or modify operating-system service permissions.

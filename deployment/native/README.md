# Native host deployment

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

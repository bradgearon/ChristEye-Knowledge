# Reviewed native package activation and rollback

Anaiero records its selected native installation in the **existing canonical
session store**. Package activation is a durable selection transition, with
the same commit/root/receipt and exclusive writer rules as session work. It
does not replace, copy, truncate or restore the store. Keep complete package
directories beside one another and stop the host before reviewing a change.

The candidate must run on the same native platform. Packages with the same
compiled identity can be relocated as before. **Different exact build IDs
require a reviewed compatibility-binding migration.** Both packages must
carry source witnesses and their versioned compatibility digests must match.
This admits a real host presentation update or qualification-only change;
it does not admit a changed engine, runtime/storage codec, lifecycle rule,
toolchain or unsupported state schema. Old precontract packages remain
same-build-only: a version label or matching schema name is insufficient.

The whole-build manifest and exact executable digest remain evidence of what
ran. A separate witness covers the native production implementation, engine
dependencies, state-bearing session/mesh/host CLI, build recipes and declared
compiler/configuration/platform settings. The validated, compiled release
display literal and qualification sources are outside that state contract;
the C++ adapter consuming that literal is itself pinned.
Resolved native sources and local headers are hashed; unknown/nonliteral
project includes fail configuration. Platform system headers/toolchain and
linked dependencies still need a declared deployment environment. A native
probe attests both digests; raw build manifests are retained byte-for-byte,
including Windows line endings. Only the identity comparison normalizes CRLF
to LF as specified by the build manifest.

## Build and inspect a candidate

Build all installation targets, then create a fresh package:

```text
cmake "-DANAIERO_BUILD_DIR=/absolute/build" "-DANAIERO_PACKAGE_DIR=/absolute/releases/candidate" "-DANAIERO_CONFIG=Release" -P native/cmake/BuildNativePackage.cmake
/absolute/releases/candidate/bin/ce_modular_host package-check /absolute/releases/candidate
```

On Windows use `ce_modular_host.exe` and native absolute paths. CMake's package
verification invokes the same native inventory verifier used at activation.
The verifier rejects unknown or duplicate manifest members, unsafe/duplicate/
unsorted paths, missing/altered/extra files, symlinks and special filesystem
entries. It streams file hashes through a fixed buffer. It also launches a
bounded, non-mutating candidate probe and compares the host's actual compiled
identity with the manifest. A failure to load the installed native executable
or its platform dependencies therefore rejects admission.

Admission bounds are 4 MiB of manifest data, 8,192 installed files, 512 MiB per
file, 8 GiB of inventoried file bytes and 8 MiB for the retained host executable.
These are finite decoder/admission limits, not a measured recommended package
size. Package directories must remain immutable while verified and selected.
Local same-user modification outside these commands is outside their ownership
contract; hashes do not authenticate an independent publisher or defend against
an administrator replacing both the binaries and their authority.

## First selection and a subsequent upgrade

Initialize the session using this build's normal `session init` command. Stop
any host that owns the store. Review and activate the current installation once
so a future candidate has an explicit rollback predecessor:

```text
ce_modular_host package-review /absolute/state/session /absolute/releases/current select-current /absolute/reviews/select-current.plan
ce_modular_host package-activate /absolute/state/session /absolute/reviews/select-current.plan
ce_modular_host package-status /absolute/state/session
```

The review-file parent must already exist. Plan publication never overwrites a
different existing artifact. The plan binds the store ID, exact generation and
root, predecessor selection revision/digest, candidate path, exact manifest
digest, host executable digest and compiled engine identity. The review command
holds the store writer lock while checking retained state and package bytes.
An active host prevents review/activation. An unfinished durable cycle also
prevents a new change; recover and finish that cycle first.

For the candidate, stop the host and run:

```text
ce_modular_host package-review /absolute/state/session /absolute/releases/candidate deploy-candidate /absolute/reviews/deploy-candidate.plan
ce_modular_host package-activate /absolute/state/session /absolute/reviews/deploy-candidate.plan
ce_modular_host package-status /absolute/state/session
```

Activation rechecks the installed bytes and executing candidate probe while
holding the store lock. Any accepted canonical work since review makes that
plan stale. Create a new review using a new operation ID; never edit a plan to
silence the conflict. The selected manifest, host executable and canonical
receipt are retained in the same atomic transition. Existing package records
keep their artifact roots even after a later package is selected. This consumes
the finite store quota and intentionally cannot silently delete rollback
evidence to make a new installation fit. The bounded package policy and its
explicit retirement procedure are described below.

For a cross-build transition the review also binds the exact origin and target
source witnesses. The service checks all scheduled identities (including
unmaterialized engines) and retained lifecycle source identities (including
retired sources), rejecting a pending durable cycle or an unwitnessed origin.
Admission is bounded to 4,096 retained source records and 16 exact build
identities in this version. The binding, its protected source blobs and the
new package selection publish in one canonical transaction. A retry never
partially rewrites checkpoints. Engine state, source identity, prior observation
keys and operation IDs remain unchanged. Both the native session CLI and the
continuous host validate the binding before constructing a retained engine.
An installer cannot supply an arbitrary alias to the public engine catalog.

Use the `selected_executable` returned by activation/status in the stopped
supervisor's command, preserving its endpoint, store, period and mesh arguments.
Run that binary's `check-selected STORE`, then start it. For systemd, change the
unit's `ExecStart`, reload the unit and start it. For Windows SCM, update the
stopped service's executable command using its existing service account and
settings, then start it. These commands do not modify supervisor registrations
or credentials. The supervisor helpers below perform this stopped switch when
explicitly invoked. They do not provide transparent live binary replacement.

Once selection exists, both `start` and `service` reject a host from another
package path or a package whose inventory changed. This makes an unchanged
old supervisor command fail explicitly instead of quietly running an
unselected installation. `check STORE` still provides ordinary retained-state
compatibility checking for a candidate; `check-selected STORE` additionally
checks this exact executable against selection. Sessions without a deployment
selection retain their existing direct host-start behavior.

This startup fence applies to hosts implementing this deployment protocol.
Earlier binaries do not understand the selection record and cannot be assumed
to enforce it. They are outside this managed activation/rollback matrix. Do not
run an older unqualified host against a managed store by bypassing the stopped
supervisor workflow.

## Reconcile an interrupted activation

Repeat the **same** activation command with the original plan. If the transition
did not commit, it revalidates the unchanged source root and completes once. If
it committed, the original operation receipt is returned with `duplicate:true`.
Its root and generation remain the original committed values. A later package
selection is never overwritten by retrying an old activation; `still_selected`
reports whether that original plan still owns the current selection.

`package-status STORE` is the current authority for selection. It reports both
the activation generation and the store's current generation. Ordinary work
can increase the latter without changing the selected package. There is no
separate mutable selection symlink or pointer file that can disagree with the
canonical commit after a process interruption.

## Roll back the package while preserving new work

Stop the host, then review the recorded previous package against the **current**
session root:

```text
ce_modular_host package-rollback-review /absolute/state/session rollback-candidate /absolute/reviews/rollback-candidate.plan
ce_modular_host package-activate /absolute/state/session /absolute/reviews/rollback-candidate.plan
ce_modular_host package-status /absolute/state/session
```

This uses the same admission and atomic activation rules as an upgrade. The
previous installation must still exist and pass exact inventory/native probe
checks. A damaged or missing *current* installation does not block rollback to
an intact target: run the commands from that intact compatible package. The
current installation's exact host and manifest witness is reverified in
canonical storage, preserving the old identity without requiring broken files
to execute. Any current state incompatible with the target rejects rollback. A successful
rollback increases the current canonical generation and preserves all accepted
session work; it never restores an earlier store image. Update the stopped
supervisor command to the newly selected executable and check/start it as above.

The retained host bytes and inventory support audit/export; they do not replace
all files in a complete installed directory or the operating system/OpenSSL
dependencies. Keep both installation directories. Removing a service registration
does not remove canonical evidence. Package retirement is explicit; no
uninstall action silently removes evidence or the actual release directories.

### Supervised installation, reconciliation and removal

Use `manage-modular-service.sh` for a managed Linux **system** unit or
`manage-package-service.ps1` for Windows SCM. The older example user unit and
basic Windows installation script remain available for unmanaged installations.
Both new helpers support `Render` without changing registration. For example:

```sh
sudo bash manage-modular-service.sh Install --name AnaieroModular --user anaiero \
  --controller /opt/anaiero/a/bin/ce_modular_host --state /var/lib/anaiero/session --period 100
sudo bash manage-modular-service.sh Start --name AnaieroModular
sudo bash manage-modular-service.sh Stop --name AnaieroModular
# Review the stopped store using package-review, then apply the immutable plan:
sudo bash manage-modular-service.sh Switch --name AnaieroModular --plan /var/lib/anaiero/reviews/update.plan
sudo bash manage-modular-service.sh Start --name AnaieroModular
```

```powershell
$account = Get-Credential
.\manage-package-service.ps1 Install -Name AnaieroModular -Credential $account -Executable 'C:\Anaiero\a\bin\ce_modular_host.exe' -StateDirectory 'C:\Anaiero\state'
.\manage-package-service.ps1 Start -Name AnaieroModular
.\manage-package-service.ps1 Stop -Name AnaieroModular
.\manage-package-service.ps1 Switch -Name AnaieroModular -Plan 'C:\Anaiero\reviews\update.plan'
.\manage-package-service.ps1 Start -Name AnaieroModular
```

Install verifies a previously selected package and registers it stopped;
select the initial package with the native review/activate commands **before**
running Install. Supply an existing service account with package read access,
store write access and the platform's service permissions. Settings retain the
endpoint, period, state, mesh configuration and account. Switch changes only
the selected executable. If activation committed but supervisor reconfiguration
did not finish, keep the service stopped and run `Reconcile`. The helper reads
canonical selection, runs the selected binary's `check-selected` and reconciles
the registered command. `Start` rejects stale registration. If the previously
configured executable is damaged, pass an intact compatible `--controller` or
`-Executable`. `Remove` stops/drains and removes the managed registration and
its helper settings, preserving canonical state and both installation trees.

The native `run-through STORE ABSOLUTE_CYCLE` command is a bounded operational
check for a stopped session. It runs the real host scheduler with the selected
binary and accepts at most 1,024 additional cycles. It intentionally writes
session work; use `check-selected` for a read-only check.

### Bound retained package witnesses

`package-retention-status STORE` shows the retained manifest identities,
count policy, byte policy, and unique bytes rooted by package and active binding
witnesses. The default is 16 packages and 128 MiB. Configure 2–256 packages
and 64 KiB–8 GiB with `package-retention-policy STORE COUNT BYTES OPERATION`.
The canonical store quota, per-blob and batch limits still apply. Setting a
budget below current usage rejects; new activation rejects before exceeding
the declared budget. Limits cover retained witness objects, not total store
allocation, external installation directories, or permanent compact identity
and operation records. The latter still consume finite store quota.

```text
ce_modular_host package-pin STORE MANIFEST_SHA256 on OPERATION
ce_modular_host package-pin STORE MANIFEST_SHA256 off ANOTHER_OPERATION
ce_modular_host package-retention-review STORE OPERATION PLAN_FILE SUPERSEDED_MANIFEST_SHA256
ce_modular_host package-retention-activate STORE PLAN_FILE
ce_modular_host package-retention-reclaim STORE OPERATION
```

Review selects 1–8 exact superseded manifest identities. It binds the store
root/generation and exact object/byte set. Current, immediate previous and
operator-pinned packages reject. Shared host/build bytes and active binding
proofs stay protected; other canonical references can also reject unsafe
expiry. Activation tombstones the package witness and expires only eligible
objects. Repeat the original operation to reconcile an interrupted unlink or
pending byte accounting. Old activation receipts remain idempotent even after
retirement, while attempting a new activation of an explicitly retired package
identity returns history expiry. Automatic filesystem deletion of release
directories is outside this command. Pre-existing unmanaged host pins remain
independent protected roots; a managed host does not create another such pin.

## Compatibility and evidence matrix

| Change | Supported result |
|---|---|
| Same engine identity, exact native package in another directory | Reviewed selection; new host path becomes authoritative |
| Changed installed non-code assets with a freshly generated valid manifest and same compiled identity | Reviewed selection; old manifest and host remain retained |
| Current-compatible previous package after additional accepted cycles | Rollback selects it against the current store; all cycles remain |
| Different exact build, matching complete state-contract witness, both implementing binding/1 | Reviewed atomic binding migration; retained logical engine identities and state remain unchanged |
| Changed engine, storage/session codec, lifecycle semantics, compiler/platform contract or native OS | Reject; no semantic conversion or arbitrary ABI downgrade is claimed |
| Earlier package without binding/1 witness | Same-build selection only; no cross-build bridge inferred |
| Busy host, pending durable cycle, changed review root or tampered installation | Admission rejects; no selection acknowledgement |
| Old activation retried after a newer selection | Original receipt returned; newer selection remains unchanged |
| Replacing the entire store with an old external backup | Outside this workflow; no external nonrollbackable witness is claimed |
| OS crash, device power loss, arbitrary binary/ABI upgrade matrix | Requires separately retained qualification evidence |

The canonical deployment tests inject cuts after objects, transaction, root,
before index and after index publication, reopen and reconcile, and require one
selection transition. They also check stale/tampered plans, protected rollback
artifacts, incompatible identity and an old retry after rollback. Installed
package tests execute the native commands, reject changed bytes after review,
switch executable admission and preserve two accepted cycles through rollback.
The compatibility suite also tests exact source proofs, actual changed
engine/compiler/storage/session witnesses, retired and scheduled sources, five
binding publication cuts, byte/count admission, protected pins and interrupted
reclaim. `native/tests/native_package_upgrade_tests.cmake` builds two actual
native packages from the same snapshot with one host display change. It checks
distinct executable/build hashes, identical compatibility witness, actual
post-upgrade work, reverse binding and audit. The separate isolated hosted
`native_supervisor_lifecycle.cmake` fixture exercises SCM/systemd; a completed
runner report is required to claim measured platform execution.
These are software publication and functional native tests, not an achieved
long-duration deployment or physical power-loss qualification.

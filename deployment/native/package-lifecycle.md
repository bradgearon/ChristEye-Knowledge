# Reviewed native package activation and rollback

Anaiero records its selected native installation in the **existing canonical
session store**. Package activation is a durable selection transition, with
the same commit/root/receipt and exclusive writer rules as session work. It
does not replace, copy, truncate or restore the store. Keep complete package
directories beside one another and stop the host before reviewing a change.

The supported package envelope is deliberately precise: the candidate must
run on the current native platform and declare the **same compiled engine
identity** as the retained session. This supports moving a verified installation
to another release directory, replacing its deployment/documentation assets,
and returning to a previously selected compatible installation. It does not
make an incompatible engine implementation into a state-compatible upgrade.
The current conservative builtin identity covers more than engine sources;
changing host/runtime/storage source can also change that identity and require
an explicit supported migration before adoption by an existing session.

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
evidence to make a new installation fit.

Use the `selected_executable` returned by activation/status in the stopped
supervisor's command, preserving its endpoint, store, period and mesh arguments.
Run that binary's `check-selected STORE`, then start it. For systemd, change the
unit's `ExecStart`, reload the unit and start it. For Windows SCM, update the
stopped service's executable command using its existing service account and
settings, then start it. These commands do not modify supervisor registrations
or credentials. They do not provide transparent live binary replacement.

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
does not remove canonical evidence. There is no automatic package/uninstall
garbage collector in this version.

## Compatibility and evidence matrix

| Change | Supported result |
|---|---|
| Same engine identity, exact native package in another directory | Reviewed selection; new host path becomes authoritative |
| Changed installed non-code assets with a freshly generated valid manifest and same compiled identity | Reviewed selection; old manifest and host remain retained |
| Current-compatible previous package after additional accepted cycles | Rollback selects it against the current store; all cycles remain |
| Different compiled engine identity or native OS | Admission rejects; an explicit supported migration is required |
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
These are software publication and functional native tests, not an achieved
long-duration deployment or physical power-loss qualification.

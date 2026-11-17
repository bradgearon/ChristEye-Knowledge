---
title: "CE-SW-28 — Native Windows/Linux Controller Host and Weighted Scheduling"
product_version: "Anaiero v0.015.x"
status: "Native local single-process host, not a production secure mesh or OS service"
---

# CE-SW-28 — Host, same-user IPC and weighted dispatch

## Implemented

- A portable controller command service owns PING, STATE/SNAPSHOT, STEP and
  STOP policy. It delegates digital stepping to an injected IDigitalCyclePort.
- NativeIpcServer/NativeIpcClient use Windows named pipes or Linux Unix-domain
  sockets and verify the connecting principal against the server identity.
  Unauthenticated requests cannot be projected or used to advance simulation.
- Controller composition uses a SIM3 Runner, Sim3Ingress, the ColonyState
  projector, an injected WeightedQueue and a digital-only step adapter.
- WeightedQueue now maintains per-(source,epoch) FIFO heads; push and take
  scan each bounded structure linearly rather than performing the earlier
  nested O(capacity^2) scan. The existing 8:5:3:2 nonfault weights, fault
  preemption, urgency and age ranking remain in place.

## Windows and Linux

The same native C++20 service command and wire-level representation are used
on both hosts. Windows requires a same-user access-token SID and rejects
remote named-pipe clients. Linux uses SO_PEERCRED, equal UID and an owner-
restricted socket. The small endpoint does not expose Windows Service Control
Manager registration or systemd service installation; it can be run headless
and later managed by a platform supervisor.

## Usage

    anaiero runtime start /tmp/anaiero-control.sock examples/connected-gardens.cev ./sim3
    anaiero runtime query /tmp/anaiero-control.sock PING
    anaiero runtime query /tmp/anaiero-control.sock STATE
    anaiero runtime query /tmp/anaiero-control.sock STEP
    anaiero runtime query /tmp/anaiero-control.sock STOP

On Windows, use a short safe pipe name such as anaiero-control as endpoint;
the same CLI resolves it to a local named pipe.

## Limits and next work

Control commands are scoped to the current OS user, not a cryptographic
remote identity. The host is intentionally single-writer and single-process.
Read/write wait bounds must be further qualified under hostile-client
conditions and production Windows/Linux platform service lifecycle.
WindStore disk fsync is not yet offloaded to a dedicated persistence worker.
External numerical solver subprocesses still need OS-specific deadlines
and termination on failure. There is no remote TLS mesh or on-device
biological intervention permission in this module.

All cell-development and digital desire values remain uncalibrated software
proxies. No real laboratory controller or actuator interface is present.

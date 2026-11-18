---
title: "ADR-002 — Lightweight Native Windows and Linux Host Parity"
status: "Accepted architecture requirement; host service adapters pending"
date: "2026-11-16"
product_version: "0.015.0"
related: "CE-SW-20, ADR-001, CE-SW-11, CE-SW-18"
---

# ADR-002: First-class native Windows and Linux runtime

## Implementation note — 2026-11-17

The original status and target table below record the architecture decision at
its introduction. Native Windows/Linux local IPC, console/service lifecycle,
owned solver processes, session recovery and optional TLS evidence replication
are now integrated through CE-SW-31–36 and tested on both platforms. Building
does not install an OS service. The proposed resource target and complete host
API table are not measurements or a claim that every listed backend exists.
Use the [implementation checkpoint](native_implementation_status.md),
[deployment guide](../../deployment/native/README.md) and
[development plan](native_development_plan.md) for actual behavior and the
remaining operational qualification.

Anaiero must be **one portable native C++20 controller** with equally supported **Windows and Linux** host implementations. Neither Linux nor Windows is a secondary host. Headless service operation does not require WSL, Docker, Cygwin, Python, Node, or a browser.

The domain, application, simulation, queue, WindPacket, evidence contracts and projector use the same code and file/wire formats on both. The composition root selects host adapters. No platform-specific headers in portable models, contracts or services.

| Host port | Linux | Windows |
|---|---|---|
| Readiness / timer / wake | epoll, eventfd, timerfd | IOCP, overlapped I/O, event objects, waitable timers |
| Local IPC | Unix-domain sockets | Named pipes |
| Peer identity | SO_PEERCRED and scoped permissions | Pipe ACLs and validated access-token impersonation |
| Supervisor | systemd | Service Control Manager (SCM) |
| Durable files | fsync, rename, supported directory sync | FlushFileBuffers, MoveFileExW write-through |
| Writer lock | flock | LockFileEx |
| Mapped view | mmap | CreateFileMappingW / MapViewOfFile |
| Monotonic clock | CLOCK_MONOTONIC | QueryPerformanceCounter or suitable Windows clock |
| Diagnostics | bounded structured file or journald | bounded structured file or Windows Event Log |
| Distribution | native ELF | native PE/EXE (MSVC) |

Identity verification and authorization are mandatory before accepting source or subscriber events. Client process IDs or pipe/socket paths alone cannot authenticate a party. Physical biological care and stimulation remain outside the digital service.

A **controller-lite** build excludes heavy UI, imaging, secure mesh and scientific importers by default. One headless process has a bounded event-loop/IPC owner, a single domain writer and a separate bounded persistence worker. No busy polling; no unbounded memory queues or synchronous storage flush on time-critical work.

Provisional target: roughly **64 MiB** for the supervisor excluding simulator heaps and optional bulk media, not a verified guarantee. Linux reports RSS/PSS; Windows reports Private Bytes/Working Set. Compare p50/p95/p99 latency, CPU wakeups, memory allocations, restart/replay and crash recovery separately on representative host hardware.

**Release gate:** shared contract and native C++20 tests run on both Windows (MSVC) and Linux (GCC/Clang). Controller functionality is incomplete until platform adapters pass equivalent IPC authentication, deadlines, stale-state, source-epoch, bounded backpressure, shutdown, reconnection and restart tests. Any OS difference requires explicit documentation and approval, never an untracked feature gap. Existing native Windows/Linux builds do not imply these future service adapters have been implemented.

The v0.018 milestone in CE-SW-20 covers **both** native controller supervisors, not Linux alone.

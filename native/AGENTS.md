# Native Anaiero development

## Delivery preference

The user requests substantial, complete changesets, approximately **five times
the earlier incremental PR scope**. Group about five related implementation
increments around one usable capability. Include the services, adapters,
command/runtime wiring, meaningful failure tests, Windows/Linux verification,
and documentation in that changeset. Several reviewable commits within one PR
are appropriate. Preserve this preference across continuation chats.

## Architecture

- Windows and Linux are first-class native C++20 hosts. The controller does not
  require Python, WSL, containers or an external numerical solver.
- Keep passive records separate from services. Business decisions belong in
  services; filesystem, thread, process, IPC and transport behavior belong in
  infrastructure adapters behind typed contracts.
- Provide relevant strategy ports and bounded lifecycle/observation extension
  points. Extensions must not reenter an owner or block dispatch.
- ColonyState Projector materializes readable state. Experiment selection and
  optimization remain independent services.
- Preserve bounded queues, explicit backpressure, source/epoch identity,
  durable commit-before-projection, and event-driven completion notification.
- Keep actual implemented behavior distinct from model hypotheses, test-only
  components and optional external solver integrations.

## Continuation checkpoint

Read `../docs/software/native_implementation_status.md` and the latest CE-SW
specification, then verify the actual engineering branch and CI. Older CE-SW
documents describe their original increment and may have superseded TODOs.

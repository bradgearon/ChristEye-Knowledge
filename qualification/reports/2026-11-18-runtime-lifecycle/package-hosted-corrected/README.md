# Hosted native release and supervisor checks

These checks target source `addf1fda4e4c5255a3ed47f1ab57ad6ff9bf7ce7`,
the follow-up to the [first MSVC warning-as-error failure](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44451398830/job/135315963472). That earlier Windows job stopped in the first package build on three C4456 local-name shadow warnings; it did not run the SCM step. Each pair comprises
two independently built executables with one changed presentation literal
and unchanged state semantics. The package fixture and installed supervisor
fixture use separate five-engine stores.

| Hosted job | Package and supervisor evidence | Observed outcome |
|---|---|---|
| [Ubuntu 24.04 push, job 135317893010](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44451985553/job/135317893010) | [Pair](ubuntu-24.04/package-pair-report.json), [systemd](ubuntu-24.04/supervisor-report.json), [archive provenance](ubuntu-24.04/provenance.json) | Pair preserved cycle 4 and completed 5. The installed service progressed from 27 to 53 cycles; reverse selection retained all 53. Registration removal and final audit passed. |
| [Windows Server 2022 push, job 135317893127](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44451985553/job/135317893127) | [Pair](windows-2022/package-pair-report.json), [SCM](windows-2022/supervisor-report.json), [archive provenance](windows-2022/provenance.json) | Pair preserved cycle 4 and completed 5. The real SCM service progressed from 12 to 24 cycles; rollback retained all 24. Registration removal and final audit passed. |

The independent pull-request jobs repeated both gates at merge checkout
`83af17bf27f2ac43d238b22e5dab79d600c738c0`: [Ubuntu/systemd](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44451992320/job/135317914971)
and [Windows/SCM](https://github.com/bradgearon/ChristEye-Knowledge/actions/runs/44451992320/job/135317914958).
Each job log records successful execution of both the pair and installed service steps.

Each retained report is an exact artifact member. The archive provenance
records the tested checkout, job and artifact, verified whole-archive hash
and the byte length and SHA-256 of each member. Only the pair and supervisor
reports are duplicated in this compact record. The [earlier Ubuntu proof](../package-hosted-linux/README.md)
also retains a complete source-manifest pair for the earlier source revision.
These short, isolated OS lifecycle fixtures establish the exercised release
transition, not arbitrary schema migration or long-duration reliability.

# Single-node index compatibility fixture

The compressed fixture contains a closed, one-generation native store written
using the storage library built before packed pages existed. The writer used
the public headers from commit `9e6a59338cab5c0cd162d168c3e6dd960fe86e6e`
and the corresponding Linux Release storage implementation. Its admission clock
was fixed at 1690000000. The archive contains the original ROOT, INDEX-COVERAGE,
transaction, payload and six index nodes. The transient writer lock is omitted.

The sole operation `old-flat-receipt` wrote record `legacy/row` with payload
`old committed value` and referenced the blob containing
`fixture payload made by the pre-packed runtime`. The recovery test checks
the original store ID, generation, terminal root and receipt hash before and
after opening the fixture, migrating to packed pages and switching back.

| Witness | SHA-256 |
|---|---|
| Build manifest | `074c37e15058906f84ef4d20fc5b8f49e244283bdcf3477d87aea230779517ff` |
| Linked storage archive | `04c425a1344e62bc1d8fe711dd47df8b5bc243e57822dfbcb2eb937b1ac0bf7b` |
| Fixture ZIP | `a3d7954b8c6a9001b0aee046ff9dc3e7f5b67999ef557da56608bb4a9dd316a5` |


The accompanying `pre-packed-review-9e6a593.cecompact` was written by the
pre-packed `ce_modular_session compact-review` executable against a copy of
this exact fixture. Its operation ID is `fixture-legacy-review` and its
review hash is `c5770a8ba1446a3ab66ac8a3af2728cc787205c22c0486022233622a03302b47`.

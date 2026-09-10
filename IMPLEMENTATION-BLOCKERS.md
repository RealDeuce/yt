# Implementation blockers and documentation gaps

This ledger records facts required by the C17 implementation that are not
fully specified by the completed Yankee Trader 3.6G analysis. Do not close a
gap by disassembling binaries or performing new reverse engineering in this
repository. Implementation stops at the affected boundary until the upstream
documentation supplies the missing contract.

## Resolved documentation gaps

### DOC-GAP-020: corrupt color-table adjacent reads

Resolved compositionally by native commit `8fb5636`. The published rule is
the complete contract: `CINT(color) * 4 + DS:556E` selects a four-byte MBF32
operand with 16-bit address wrapping and no bounds check. The implementation
now binds the full 64-KiB process image, performs that signed address
calculation, and copies all four bytes across `FFFF -> 0000` when necessary.
It therefore does not need a separately enumerated catalog of every possible
adjacent cell. Focused fixtures pin ordinary indices, index 8, index -1, and
a wrapped four-byte read at `FFFE`.

### DOC-GAP-032: YTCONFIG 52nd YTNAME row overflow destinations

Resolved upstream by commit `aa5675db`. The four wrapped destinations are
`DS:19B2`, `1A7E`, `1B4A`, and `1C16`; the first three stores transfer their
temporary owners into the next arrays' element zero, while the fourth treats
loaded code bytes `62 00 BE B6` as a descriptor and reaches the shared
nonreturning `BRUN:0ACC` internal fatal. No BASIC ERR/ERL route or rollback is
involved.

Affected coverage:

- the YTCONFIG alias-table scan at `YTCONFIG:0E4E..0EE5`;
- a 52nd physical four-token `YTNAME.DAT` group after array index 50; and
- the exact default-fatal or continued state produced by each of its four
  sequential string stores.

The completed configuration-editor document states that the four parallel
arrays are indexed 0 through 50, that the counter is incremented before each
group, and that physical row 52 attempts out-of-bounds writes into adjacent
state. The semantic model consequently raises an abstract `OverflowError`.
Neither source identifies the four wrapped/calculated destination addresses,
the adjacent descriptors or numeric cells they alias, the order and retained
heap ownership of successful stores before a later store fails, or the exact
ERR/ERL/default-handler projection.

Those effects are observable and the native editor's current `count > 51`
guard is only a protective boundary, not a compatibility claim. The generic
BRUN dynamic-string-array contract cannot choose caller-specific destination
addresses or adjacent ownership without the missing YTCONFIG layout. Upstream
documentation, generated evidence, and focused row-52/store-failure fixtures
must publish that carrier before the guard can be replaced. No binary
inspection or new reverse engineering was performed.

### DOC-GAP-031: Xannor sector-arrival physical transaction sequencing

Resolved upstream by commit `aa5675db`. The new physical-step carrier pins
the unconditional opening GET, the mine-loss-only reload/offset-129 PUT, the
owner-label GET, the defense-gated reload and offset-81/85 PUT, every retained
failure prefix, and the later planet unlink outside the sector-arrival child.

Affected coverage:

- the address-owned `YTMAINT:71AF` sector-mine and deployed-defense arrival
  transaction;
- its `7280`, `7390`, `73C4`, `76D2`, `775B`, and `78C8` physical sites; and
- removal of the native route owner's synthetic single sector write after
  the sector, planet, and player arrival children return.

The completed semantic model pins mine-before-defense RNG and mutations, all
result rows, the positive-owner versus Mercenary label, and the final values.
The address-owned physical catalog separately names the initial sector GET,
post-mine sector reload/PUT, positive-owner player GET, and pre-defense sector
reload/PUT. It does not state the branch conditions under which the two reload/
PUT pairs are reached, which live values are copied over each freshly loaded
FIELD image, or the exact process/FIELD/output residue when any one of those
calls fails. The presentation model records news-call prefixes but does not
include these physical calls in its call tape.

Those details are observable and cannot be reconstructed from the final typed
`XannorSectorArrival` result. In particular, the native implementation
currently retains one opening sector image and performs one write only after
the later planet and player children. Replacing that with the catalogued
transaction requires knowing whether mine and defense writes are conditional,
whether a fresh reload preserves intervening fields, and exactly where the
planet child's sector-unlink write composes relative to the sector-arrival
writes. Guessing would change partial-I/O behavior and could reintroduce stale
record bytes.

Upstream documentation, a canonical ordered physical-call carrier, generated
evidence, and focused success/failure-prefix fixtures must publish those
conditions and overlays before the native transaction can be converted. No
binary inspection or new reverse engineering was performed.

### DOC-GAP-030: destroyed-mine caller projection into `FatalWorld`

Resolved upstream by commit `aa5675db`. The canonical projection now builds
`FatalWorld` directly from the inherited projectile carrier, raw mine result,
team cache, and optional returned warp result. It distinguishes the ordinary
post-news/post-sector-GET seam from the emergency-warp seam and preserves the
documented FIELD, persistence, process, pager, and news carriers.

Affected coverage:

- every admitted sector-mine result whose `DS:18B4` destroyed predicate
  transfers from `YT:0913` to common fatal entry `YT:06C4`;
- the direct emergency-warp main W and hostile W/WT continuations when mine
  damage destroys the player without a second warp; and
- the corresponding post-emergency-warp fatal continuation when the mine
  child returns through `YT-SUB:6C2E` with destroyed still nonzero.

The completed sector-mine contract says that a destroyed handoff carries the
canonical `FatalWorld`, but the published caller composition explicitly does
not claim the caller-state join into the common-fatal child. The supplied
`project_mine_fatal_entry()` adapter only checks that an independently
provided `FatalWorld.state` equals the pager state; it does not construct that
world from the inherited gameplay carrier and `RawMineResult`. The graph
likewise retains `fatal-world` as an external continuation input.

That missing projection is observable and cannot be synthesized from the
typed mine result. It must specify the exact record store, FIELD layout/record
and raw image, process cells (including raw destroyed and current-player
roots), team cache, news bytes/open-handle state, and pager/runtime state at
both fatal seams. In particular, ordinary destroyed return has completed the
final news append and final sector GET, whereas a mine-triggered emergency
warp returns without those operations and leaves the warp child's player
FIELD/persistence state.

Upstream documentation, a canonical constructor from the inherited raw world
plus the mine/warp terminal carriers, graph wiring, generated evidence, and
focused fatal-join fixtures must publish this contract before the native
caller can enter the already implemented common-fatal child. No binary
inspection or new reverse engineering was performed.

### DOC-GAP-029: shared A8D2 local string-allocation failure contract

Resolved upstream by commit `aa5675db`. The four A8D2-owned sites are
pinned at `A8EF`, `A8F4`, `A938`, and `A943`, with exact instructions,
saved IPs, statements, ERL 40001, destinations, admitted errors, and main-
handler versus internal-fatal routing. The native staged owner implements
their documented fail-before-effect response, prompt, queue, and bold
residues; its fatal adapter carries the exact saved identity into the shared
BRUN cleanup owner.

The live `session_a8d2()` path uses that same staged transformation after
the shared raw prompt and `0357` editor. Focused fixtures pin blank/Y/N/
invalid retry semantics, queue clearing, prompt clearing, every admitted
fault, and the joined main/hostile plain/ANSI failure prefixes. Root
`YT:A8D2` and all nine qualified internal transfers are verified.
AB36/0357, compatibility-uppercase, raw presentation, main/shared routing,
fatal cleanup, framebuffer, physical, and caller behavior remain
independently owned seams.

### DOC-GAP-033: YTCONFIG row-52 `BRUN:0ACC` fatal projection

Resolved upstream by commit `7454cc90`. YTCONFIG has the exact eight-byte
module label `YTCONFIG`, no statement table and therefore no source-line
clause, and saved IP `0EE5`. The shared fatal owner now composes the exact
local-only diagnostic, prompt, normal-input drain or redirected-input CR,
CLOSE-all, function-row/cursor restoration, and DOS-zero terminal while
retaining the three committed owner transfers and corrupt fourth descriptor.
The native editor uses the physical 51-descriptor-array loader directly and
reaches that terminal instead of applying its former protective row-count
guard.

Formerly affected coverage:

- the fourth physical-group-52 alias store at `YTCONFIG:0EE2`, whose saved
  return IP is `0EE5`;
- the shared `BRUN:0ACC` internal-fatal diagnostic and cleanup; and
- replacement of the native editor's remaining live `count > 51` guard.

Commit `aa5675db` supplied the four wrapped destinations and retained mutation
prefix. Commit `7454cc90` then supplied the canonical YTCONFIG `0EE5` fixture,
extended shared-fatal model, exact output bytes, and terminal cleanup carrier.
No binary inspection or new reverse engineering was performed.

### DOC-GAP-035: complete BRUN runtime error-name table

Resolved upstream by commit `59501f98` and traversal row 327. The canonical
generated `ytsub-error-output.static.txt` now publishes all 39 named error
bytes and the exact `Unprintable error` fallback for every other byte in
`00..FF`.
ERR 75 is `Path/file access error`. The native shared lookup and exhaustive
256-byte fixture consume that published table directly; YT-INIT's RUN
preflight now admits its complete ERR 53/67/75 family.

### DOC-GAP-034: main startup `YT:0133` internal-fatal location

Resolved upstream by commit `59501f98` and the completed main-startup
initialization contract. The
compact COPY frame saves IP `0136`; the statement table selects statement
`012A`, ERL 3; and the exact padded module label is `YT      `. The native
caller adapter now carries that identity into the shared `BRUN:0ACC` owner
and its focused fixture pins the complete local diagnostic and cleanup suffix.

### DOC-GAP-036: ship-computer treasury return-carrier contradiction

Resolved upstream by commit `c37a31c0`. The scalar transcript remains a
bounded fixture ending at `YT:8672`, but it is not the state authority there.
The canonical post-body world is now explicitly carried through the real
wrapper, the fresh `YT:863A` A41C hydration and prompt, the shared AB36 entry,
and the `YT:AB70` loop head for collection, report, and no-owned returns.

The native `computer_menu()` already preserves that same complete session and
database world: both computer treasury dispatches call `command_collect()` and
then continue directly to the shared fresh-prompt owner; its hydration reloads
the current player before `session_0357()` enters the shared AB36 editor. The
verified treasury, A41C, prompt, and AB36 blocks therefore close this caller
seam without treasury-specific copies of their internal behavior.

### DOC-GAP-043: BRUN COM1/COM2 CLOSE method contract

Resolved upstream by commit `d16edb83` and implemented by native commit
`dc738d0`. The shared -4/-5 method now selects `DS:13A6/13BE`, models the
conditional text-mode 1Ah write and pre-teardown ERR24, ignores drain status
3/4/5, then clears the transmit/vector/interrupt/port roots and releases FIELD
and control ownership. Focused tests pin binary YT success, text failure,
RETRY from retained live state, both COM classes, and invalid status domains.

### DOC-GAP-042: non-startup date-helper result process cells

Resolved upstream by commit `d16edb83` and implemented by native commit
`dc738d0`. One addressed process adapter now covers all ten YT calls at
`DS:188C`, startup copy `4CCA`, early-profit copies `5E56`, later-profit copies
`60C4`, all four YTCONFIG calls at `1D4A`, all seven YTMAINT calls at `1858`,
and both RMT-INIT calls at `19F6`. The live session and utility paths store the
final raw MBF32 result before their caller continuations; focused fixtures pin
all 23 site/address mappings and byte-identical copies.

### DOC-GAP-041: AB36 active-fault inventory

Resolved upstream by commit `d16edb83` and implemented by native commit
`dc738d0`. The native shared inventory contains the published 77 AB36 rows,
75 live and two discharged-unreachable sequential-allocation rows. Every row
retains its module, operation, saved IP, statement, ERL, error kind, and live
classification; a deterministic checksum fixture pins the complete table.
The existing scheduler, editor, raw heap, framebuffer, B05D/B1F3, and error
router remain the composable behavioral owners.

### DOC-GAP-040: B1F3 fault denominator and completion state

Resolved upstream by commit `d16edb83` and implemented by native commit
`dc738d0`. The native inventory contains exactly eleven live sites: three
GOSUB stack checks, four uppercase-helper allocations, and four main-module
COPY_STR allocations. The exact module/address/saved-IP/ERL/error table is
checksum-pinned and composes with the existing pager and error-router owners.

### DOC-GAP-039: B05D LOC fault inventory

Resolved upstream by commit `d16edb83` and implemented by native commit
`dc738d0`. The native B05D inventory contains exactly 25 live cuts and omits
the deterministic `YT:B080` LOC transition. Its exact helper, allocation,
physical-adapter, and pager-GOSUB identities are checksum-pinned and compose
with the existing B05D editor/output/error carriers.

### DOC-GAP-038: PORTNAME default BRUN fatal projections

Resolved upstream by commit `d16edb83` and implemented by native commit
`dc738d0`. The native PORTNAME catalog contains all 194 reachable runtime
instructions and their exact next-offset saved IPs. A single parameterized
projection supplies padded module `PORTNAME`, no source-line clause, the
callee-provided error byte, and the selected saved IP to the shared BRUN
0AC4 fatal/cleanup owner. The complete catalog checksum and a representative
fatal join are pinned; no second caller error domain is invented.

### DOC-GAP-037: action-finalizer CINT failure identities

Resolved upstream by commit `d16edb83` and implemented by native commit
`dc738d0`. The two overflow cuts now attach ERR6 with exact identities
`A734 -> A737`, statement `A712`, and `A773 -> A776`, statement `A770`, both
ERL 40001 under main handler `B2DA`. Existing raw FIELD/cache residue is
preserved and the shared router receives the exact identity.

### DOC-GAP-023: current-sector scanner cloak-clear raw value

Resolved upstream by commit `d16edb83` and implemented by native commit
`dc738d0`. Scanner eligibility and cloak state are reread from the authoritative
process cache. An admitted reveal copies canonical `00 00 00 00` into that
cache before selector-four sound and mirrors the typed cache afterward.

### DOC-GAP-022: Earth Anti-Cloak cache-clear raw value

Resolved upstream by commit `d16edb83` and implemented by native commit
`dc738d0`. Every loop iteration rereads authoritative process-cache bytes; a
positive cloak copies canonical `00 00 00 00` before the player GET. Focused
failure tapes pin the raw clear, ordering, and retained state.

### DOC-GAP-021: scoreboard team-ID scratch write boundary

Resolved upstream by commit `d16edb83` and implemented by native commit
`dc738d0`. After each fresh player GET and score overlay, the exact four team
bytes at FIELD offset 89 are copied to `DS:4B8C` before PUT. Focused tapes pin
all writes and current-player PUT-failure residue.

### DOC-GAP-015: zero-Headquarters repair literal conflict

Resolved upstream by commit `d16edb83` and implemented by native commit
`dc738d0`. Main startup uses literal `00 40 37 8A` (MBF32 733), writes it to
configuration FIELD offset 117, performs PUT #1,1, and only then copies the
same bytes to process `DS:4BD8`. PUT failure retains the dirty FIELD repair
without publishing the process global.

### DOC-GAP-014: command-6 dependency failure projections

Resolved upstream by commit `d16edb83` and implemented by native commit
`dc738d0`. Command 6 now attaches the published OPEN, LOF, three GET, final
CLOSE, opening/log/automatic heading, pause-output, and private-wait identities.
Physical adapters supply their own admitted error byte; delegated output/wait
calls preserve their callee domain. ERR24 projects to the existing shared
statement retry and all other errors use the shared terminal route.

## Resolved documentation gaps from upstream `b2c85613`

### DOC-GAP-019: planet-updater arbitrary numeric records

Resolved upstream by commit `b2c85613` and implemented by native commit
`8275622`. Production now selects `yt_planet_updater_raw_run()` for
the live updater; after GET it applies no record-value guard, while
`yt_planet_updater_run()` remains the ordinary typed projection. The raw
state carries the documented Q, P, A, day, minute, and elapsed cells through
the process image. Focused native fixtures sweep MBF32 minus one and dirty
zero through all fourteen numeric FIELD offsets, pin the `YT-SUB2:0D63` ERR6
overflow before any LSET, and retain the typed rejection as an adapter result
rather than game behavior.

### DOC-GAP-018: plasma wait destination roots

Resolved upstream by commit `b2c85613` and implemented by native commit
`8275622`. The first and second one-second prelaunch waits use distinct
process cells `DS:5D02` and `DS:5D0E`; every half-second admitted-hop wait
reuses `DS:5D3E`. Each path copies the exact MBF32 literal before entering the
shared by-reference wait, which overwrites that cell with its deadline.

### DOC-GAP-017: radio private-pager wait destination root

Resolved upstream by commit `b2c85613` and implemented by native commit
`8275622`. Every private-pager pause copies exact MBF32 99 into `DS:53DE` and
passes that process cell to the shared by-reference wait. Repeated pauses reuse
the same cell, so later reader failures retain its last duration or deadline.

### DOC-GAP-016: Xannor-victory wait destination root

Resolved upstream by commit `b2c85613` and implemented by native commit
`8275622`. The victory suffix copies exact MBF32 99 into `DS:5CAE` and passes
that process cell to the shared by-reference wait; direct, missile, and plasma
callers therefore retain the same returned duration/deadline carrier.

## Previously resolved documentation gaps

### DOC-GAP-028: ADE0 allocation/GOSUB current-statement identities

Resolved upstream by commit `98f69fcd`. The 22 remaining main-owned ADE0
allocation/GOSUB sites and all four delegated uppercase-helper sites now
have exact instruction, saved-IP, current-statement, ERL, ERR and installed-
handler identities. The native fault registry contains all 26 mappings and
projects their exact main or shared terminal routes. The staged save,
uppercase, repeat-parse, repeat-build/notice, and semicolon/queue owners now
attach those identities at all 26 selectable BRUN heap/GOSUB cuts, including
arbitrary build and replacement-loop occurrences. Raw heap, descriptor,
frame, asynchronous continuation, and physical handler effects remain
implementation work rather than a documentation gap.

### DOC-GAP-027: sequential disk-read carry prefix and cursor conflict

Resolved upstream by commit `b37b9064`. The one shared `CBFD..CC52` refill
accepts a carry-set physical prefix of zero through 128 bytes and a terminal
external cursor. It overlays that prefix on the cleared buffer, retains the
zero tail, leaves the old total count and zero remaining count, does not
increment the refill index, and cannot consume the failed prefix. `EOF` and
regular-file `LINE INPUT #` use this same contract. The existing native typed
adapter and exhaustive refill fixtures already implement it.

### DOC-GAP-026: ADE0 repeat-overflow current-statement identities

Resolved upstream by commit `b37b9064`. Repeat `VAL` overflow at `AEBF`
saves IP `AEC2`; repeat conversion-to-SINGLE overflow at `AEC8` saves IP
`AECB`. Both belong to current statement `AE9C`, ERL 36000, main handler
`B2DA`, and ERR 6. The native fault registry now exposes both identities and
the live session attaches them to the existing nonlocal gameplay-resume
router.

### DOC-GAP-025: new-player identity GET/PUT error projection

Resolved upstream by commit `0c2f8ccf`. The selected-player GET is
`064B/064E`, retries statement `0640`, and has ERL 11120; the identity PUT is
`0683/0686`, retries statement `0678`, and has the same ERL. Both run under
main handler `B2DA`; GET admits ERR 52/57/70/75 and PUT additionally admits
ERR 61. ERR 57 retries only the named I/O statement, while every other
admitted error takes the main terminal route over the documented retained
FIELD and physical-I/O prefix.

### DOC-GAP-024: post-login repair raw FIELD sources

Resolved upstream by commit `0c2f8ccf`. The first predicate is cached current
sector below raw one—not turns below one—and its sole assignment copies
`DS:628A` byte-for-byte to player FIELD offset 57. The cargo repair copies
canonical zero `DS:62F4` to offsets 69 and 73, then independently copies live
maximum-holds `DS:19E8` to offsets 77 and 65. The two PUT identities,
ERR domains, retry statements, retained dirty FIELD images, and physical
failure prefixes are now pinned.

### DOC-GAP-013: command-5 fractional TEAM target

Resolved upstream by commit `f3025222`. Every raw nonzero current-player team
ID invokes the shared loader. Its raw inclusive 1..50 comparisons have no
integrality predicate, so 1.75 reaches BRUN random-record conversion and,
with sector offset 51, selects physical record 52. The existing canonical
native loader was correct; the stale radio-composer sentence was not.

### DOC-GAP-012: command-8 dependency failure projections

Resolved upstream by commit `f3025222`. Canonical physical adapter domains
supply partial I/O, parser/file state and BRUN error mapping; ERR 24/57 retry
re-enters the statement over that retained state, and other results enter the
documented shared handler or main ERL-40000 recovery. Command 8 preserves its
entry FIELD. Shared AB36/F8/framebuffer behavior composes normally.

### DOC-GAP-011: command-4 FIELD and dependency failure projections

Resolved upstream by commit `f3025222`. The scoreboard generator leaves the
FIELD image from its last successful cache, player-row or positive-team GET.
A failed return hydration preserves that image and live file state; a
successful current-player GET replaces it. Physical failures and retries use
the canonical adapter and router contracts, while shared input/framebuffer
state remains ordinary composition.

### DOC-GAP-010: navigation error-router identities and corrupt state

Resolved upstream by commit `f3025222`. Command-specific saved IP, retry
address, ERL, installed handler and admitted BRUN error domains are now pinned
for all route-builder and main navigation failure sites. Corrupt route indices
perform the documented 16-bit adjacent or wrapped DS write; predecessor
cycles and FIFO/movement loops retain their real back-edges rather than host
array bounds or finite-transcript normalization.

### DOC-GAP-006: command-2 dependency failure identities

Resolved upstream by commit `f3025222`. The selected-sector, friendship,
ordinary-port and Earth-report failure sites now have their saved IP, retry
statement, ERL, active handler and complete admitted error domains connected
to the existing main/shared routers. Canonical physical adapters supply the
accepted prefix and retained file/FIELD state at each concrete observation.

### DOC-GAP-009: navigation shared AB36 and framebuffer composition

Affected coverage:

- command-10 start and destination editors;
- command-3 destination and confirmation editors;
- route-display `POS(0)` state; and
- inherited local framebuffer/cursor results.

Resolved by the user's authoritative clarification. Navigation injects its
canonical global state at each real shared-editor boundary; the existing AB36
machinery then owns per-poll time refresh, input precedence, queue/typeahead,
F8 resumption, carrier and terminal outcomes. Route display carries the
inherited local column into the existing `POS(0)`/wrap model, and local output
reduces through the canonical inherited-framebuffer transducer. None requires
a caller-specific vector for every interleaving or one fixed initial/final
screen image. These joins remain implementation and verification work. No
binary inspection or new reverse engineering was performed.

### DOC-GAP-008: command-7 inherited framebuffer composition

Affected coverage:

- ship-computer command-7 local screen cells and cursor result; and
- its local F8/editor presentation effects.

Resolved by the user's authoritative clarification. The documented semantic
local event tape, inherited cursor/framebuffer carrier, and canonical
framebuffer transducer define the result compositionally. Exact compatibility
does not require a single fixed initial framebuffer or a separately supplied
final screenshot. The native implementation must carry and reduce that state;
this is not a missing-analysis boundary. No binary inspection or new reverse
engineering was performed.

### DOC-GAP-007: command-7 shared AB36 editor composition

Affected coverage:

- ship-computer command-7 avoid-list slot editor; and
- its sector replacement editor.

Resolved by the user's authoritative clarification. As with resolved
DOC-GAP-005, command 7 supplies canonical global state at its two real AB36
entries. The shared AB36 contract already owns later time refresh, local/
serial/queue precedence, typeahead, asynchronous F8 and resumed polling,
Ctrl-R/save-repeat helper joins, carrier loss, and terminal outcomes. The
caller does not need bespoke vectors for every permutation. No binary
inspection or new reverse engineering was performed.

### DOC-GAP-005: command-2 surviving AB36 editor interleavings

Affected coverage:

- ship-computer command-2/alias-23 first selector and retry selectors; and
- the command-2 return into the fresh `YT:8639` computer prompt.

Resolved by the user's authoritative clarification. The shared AB36 contract
already parameterizes per-poll time refresh, local/serial/queue precedence,
typeahead residue, F8 delivery and resumed polling, and every timeout,
carrier-loss and terminal outcome. Command 2 does not require a separate
caller-specific vector for every permutation. It only joins its canonical
global state into that shared editor at the initial selector, the retry after
`02DB` clears the queue, and the fresh `8639` computer prompt. The upstream
`docs/runtime/computer-port-report-output.md:290` disclaimer was therefore a
documentation scoping defect, not missing behavioral analysis. No binary
inspection or new reverse engineering was performed.

### DOC-GAP-004: forced-Bribe surrender join raw/typed output conflict

Affected coverage:

- hostile Bribe forced-combat continuation;
- hostile Attack surrender-prefix composition; and
- completion claims that depend on the joined forced-Bribe combat oracle.

Resolved upstream by commit `35039c3e`. The joined fixture now carries the
actual COM1 device class `FCh` through
`compose_hostile_attack_surrender_prefix_world()`, so its raw and typed
direct-output serial states agree. This removes the erroneous additional LF:
the corrected fixture explicitly rejects `CR LF LF`. The formerly failing
focused test
`tests.test_transducer_join.TransducerJoinTests.test_hostile_bribe_forced_attack_composes_first_combat_region`
passes in isolation. This was a caller-composition/state-carrier defect, not
missing combat or fighter/shield-spill analysis.

### DOC-GAP-003: unshielded sector-mine missile RNG in joined fatal contracts

Affected coverage:

- anonymous sector-mine body and its caller continuations;
- direct fighter-kill raw FIELD/store/news/RNG composition; and
- direct fighter destroyed-to-fatal raw transaction.

The upstream contracts disagreed about the positive-missile branch of an
unshielded sector-mine batch:

- `docs/runtime/sector-mine-output.md` specifies one direct missile `RND`,
  identifies site `YT-SUB:679B`, and agrees with
  `tools/ytmine_output.py`, which consumes
  `batch<N>.missiles` once and applies
  `INT(RND * (batch * missiles)) + 1`.
- `docs/runtime/direct-fighter-fatal-cycle-output.md` instead lists
  `missile TIMER/RANDOMIZE/RND x3` in its joined RNG order.

Resolved by the user's authoritative clarification: the fatal-cycle sentence
is a prose error. An unshielded nonzero missile field consumes exactly one
direct `RND` at `YT-SUB:679B`, then computes
`INT(RND * (batch * missiles)) + 1` and caps the result to the current missile
stock. The three-step TIMER/RANDOMIZE/RND shrink helper applies to fighters,
carried mines, commodities, and empty holds, not missiles.

The retained direct-fighter fatal fixture has zero missiles, so its generated
artifact, 13-draw trace, and 1,597/1,900-byte transcripts are unchanged. The
native `mine_encounter()` follows the correct component model, and its
provider-driven missile step now pins exact-zero suppression, one-call
cardinality, capped arithmetic, and the single failure boundary explicitly.

### DOC-GAP-001: sequential character-device `PRINT` adapter

Affected coverage:

- BRUN regular-file `PRINT`/`CLOSE` and shared CLOSE-all registry
- BRUN regular-file sequential `OUTPUT OPEN`

Resolved upstream by commit `c08f1fca` in
`docs/runtime/brun-print-transducer.md`. The contract now specifies:

- one ordered one-byte physical write per logical byte, with the final pending
  byte offered by statement completion;
- accepted and dropped clear-carry results, including the compatibility-bit
  distinction;
- exact pending/index/column residue at value-write and completion failures;
- the absence of a synthetic carrier-loss edge; and
- the device-code newline distinctions used by rooted callers.

The shared projection is implemented by `yt_text_device_print()` with focused
per-write provider tests. Raw allocator/registration cleanup remains a mapped
implementation obligation, not a documentation gap.

### DOC-GAP-002: `YTMAINT` radio-compaction file transaction

Affected coverage:

- YTMAINT message compaction and newspaper rotation
- `YTRMSG.DAT` radio store/compaction

Resolved upstream by commit `c08f1fca` in
`docs/file-formats/YTRMSG.md`. The contract now specifies:

- the preliminary OUTPUT/CLOSE/KILL sequence that removes an old `temp`;
- a fresh RANDOM destination with the 4/4/4/72 FIELD layout;
- explicit source `GET` and dense destination `PUT` record numbers; and
- source `#5`, then destination `#4`, then KILL, then NAME ordering.

The documented transaction is implemented by `yt_radio_compact()`.

## Explicitly deferred work

These are authorized deferrals rather than missing-analysis gaps:

- exact `LOCAL.EXE` console/menu/DORINFO/SHELL/local-framebuffer
  compatibility; and
- platform-specific OpenDoors startup, exit, and post-OPEN wait behavior.

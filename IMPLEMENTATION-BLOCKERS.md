# Implementation blockers and documentation gaps

This ledger records facts required by the C17 implementation that are not
fully specified by the completed Yankee Trader 3.6G analysis. Do not close a
gap by disassembling binaries or performing new reverse engineering in this
repository. Implementation stops at the affected boundary until the upstream
documentation supplies the missing contract.

## Resolved documentation gaps

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

Resolved upstream by commit `aa5675db`. The four A8D2-owned sites are now
pinned at `A8EF`, `A8F4`, `A938`, and `A943`, with their exact saved IP,
statement/ERL, admitted allocation or owner-corruption outcomes, retained raw
world, and main-handler versus shared internal-fatal projections.

Affected coverage:

- the direct emergency-warp confirmation after its now-joined ADE0 and
  delegated compatibility-uppercase prefixes;
- all other callers of the shared `YT:[A8D2,A949)` Y/N/blank reader; and
- exact main/shared error routing and retained input state before answer
  selection or invalid-answer retry.

The completed caller documents and global-state registry establish A8D2's
ordinary semantic order: raw-emit mutable prompt `DS:4D3A`, run AB36/ADE0,
compatibility-uppercase the complete response, retain
`LEFT$(response,1)` in `DS:4C9A`, clear the prompt on valid return, or set
bold, clear typeahead and retry for another first byte. The direct-warp
implementation now joins the documented nonempty `ADE7` and `AE4A` cuts and
all four delegated uppercase-helper cuts through their exact main/shared
handler identities.

The documentation does not identify A8D2's own post-uppercase `LEFT$` and
destination-assignment/COPY call instructions, saved IPs, current statements,
ERLs, admitted ERR values, or installed handler projections. It also does not
pin the movable-string heap/descriptor prefix for `DS:4C9A` and `DS:4D3A`,
the returned-answer scratch, or the queue state at those failures. The direct
emergency-warp and other caller documents explicitly list these A8D2
allocation failures as remaining work. The generic BRUN string contract is
not enough to choose the missing caller sites and starting descriptors.

Upstream documentation, the canonical A8D2 world model, generated evidence,
and focused failure-prefix fixtures must publish those identities and raw
states before this boundary can proceed. No binary inspection or new reverse
engineering was performed.

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

## Open documentation gaps

### DOC-GAP-034: main startup `YT:0133` internal-fatal location

Affected coverage:

- the rooted post-event startup initialization at `YT:[0121,0206)`;
- the corrupt destination-owner result at the `YT:0133` control-CR copy; and
- the caller projection from that retained string-heap state into the shared
  `BRUN:0ACC` internal-fatal terminal.

The published startup model and generated evidence identify the reached
result as `YT:0133 / 0ACC`, retain its complete prior scalar/heap/descriptor
prefix, and correctly keep it outside the installed main BASIC error handler.
The shared fatal documentation requires the saved BASIC IP and the statement
table's greatest-offset result in order to render the exact module location
and optional source-line clause. Neither the startup document, its generated
artifact, nor its focused corrupt-owner fixture publishes those two caller
values for this cut.

The native shared fatal owner therefore cannot attach an exact `YT` caller
identity without guessing an instruction-return width and statement-table
mapping. Upstream documentation and a focused fatal-composition fixture must
publish the saved IP, resolved line/no-line result, exact diagnostic bytes,
and cleanup join for this retained `YT:0133` world. No binary inspection or
new reverse engineering was performed.

### DOC-GAP-023: current-sector scanner cloak-clear raw value

Affected coverage:

- authoritative process-array reads for the current-sector scanner's
  player-candidate loop at `YT-SUB:5E8D`; and
- the exact revealed-cloak cache mutation and residue before selector-four
  sound, later duplicate sensor slots, visible-player GETs, and dependency
  failures.

The completed `docs/runtime/current-sector-output.md` and
`docs/gameplay/command-shell.md` establish that the scanner reads the
startup-era sector and cloak arrays, emits the shimmer row for an admitted
reveal, then clears only that candidate's cloak cache before sound and later
visibility tests. The semantic `tools/ytsector_output.py` model records the
result only as numeric zero. Neither it, the generated scanner evidence, nor
the global-state registry identifies the four source bytes copied into
`DS:1A70 + 4*(candidate+52)` by this clear.

The raw value cannot safely be inferred from its numeric meaning. Completed
cache owners already use canonical `00 00 00 00`, Xannor retaliation uses
`00 00 40 00`, player death uses `00 00 7A 00`, and action-finalizer expiry
uses `00 00 A3 00`. The scanner's process reads and clear must migrate as one
unit because a duplicate mode-one sensor slot observes the completed clear;
mixing process reads with the existing typed-only clear would replay stale
cloak state.

Upstream documentation, generated raw evidence and a focused duplicate-slot
failure-prefix fixture must identify the copied source bytes. Until then the
native current-sector scanner retains its typed cache carrier. No binary
inspection or new reverse engineering was performed.

### DOC-GAP-022: Earth Anti-Cloak cache-clear raw value

Affected coverage:

- authoritative process binding of the global Earth Anti-Cloak sweep's
  positive-cloak clears at `YT-SUB:932E`; and
- exact cache residue across its target `GET`, name-conversion, output,
  sound, fade and credit-debit failure prefixes.

The completed `docs/runtime/earth-purchase-actions-output.md` and
`docs/gameplay/planet-actions.md` establish that every positive player cloak
cache entry is cleared before that player's `GET`, while zero and negative
entries remain unchanged. They identify the process cache mapping as
`DS:1A70 + 4*(record+52)`, but neither document nor the generated Earth
purchase model identifies the four raw bytes written by the clear. This
cannot safely be inferred from the numeric result: other completed cache
owners use canonical `00 00 00 00`, Xannor retaliation uses
`00 00 40 00`, player death uses `00 00 7A 00`, and action-finalizer expiry
uses `00 00 A3 00` for numerically zero values.

Upstream documentation, generated raw evidence and a focused failure-prefix
fixture must identify the copied source bytes and confirm whether every loop
iteration uses the same value. Until then the native Anti-Cloak model retains
its typed cache mutation and is not connected to the authoritative process
array. No binary inspection or new reverse engineering was performed.

### DOC-GAP-021: scoreboard team-ID scratch write boundary

Affected coverage:

- authoritative binding of the scoreboard generator's current-player team-ID
  scratch at `DS:4B8C`; and
- exact process/FIELD residue when a player score-cache GET or PUT fails.

The completed global-state registry and `docs/file-formats/YTSCORE.md` identify
`DS:4B8C` as an MBF32 value copied from player FIELD offset 89 during
`YT-SUB:3AF5..3B46`, and say that a normal return retains the last player team
ID. They do not place that copy relative to the loop's fresh player GET,
score-field overlay, and durable cache PUT. That ordering is observable: a
failed PUT either retains the preceding player's scratch or the current
player's raw team bytes. The current native generator also decodes the initial
player sweep into a host object, so choosing that stale value rather than the
fresh FIELD bytes would create a second undocumented behavior.

Upstream documentation, generated evidence, and a focused failure-prefix
fixture must identify the exact copy point and raw source before `DS:4B8C` can
be made authoritative. The separately documented sector-owner reuse of
`DS:1A40` is not blocked by this gap. No binary inspection or new reverse
engineering was performed.

### DOC-GAP-020: corrupt color-table adjacent reads

Affected coverage:

- corrupt logical-color indices whose direct `CINT(color) * 4 + base`
  calculation reads adjacent process storage.

Correction to the original entry: the completed global-state registry already
names the initialization flag at `DS:556A` and the 32-byte table at
`DS:556E`; `docs/runtime/presentation-helpers.md` supplies the eight ordinary
values. The native ordinary table is therefore process-bound and no upstream
work is needed for indices 0 through 7.

The remaining gap is the explicitly unbounded corrupt-index domain. The
registry identifies some following roots (`DS:5596/559A` string descriptors
and `DS:559E/55A2` caches), but does not identify the intervening cells at
`DS:558E/5592`, provide a complete wrapped-address/alias contract, or pin the
raw reads and resulting `CINT`/`COLOR` behavior for the reached corrupt cases.
The native implementation retains its protective range result outside 0
through 7 rather than inventing those outcomes. Upstream documentation,
generated evidence, and focused fixtures must specify that corrupt domain
before the guard can be removed. No binary inspection or new reverse
engineering was performed.

### DOC-GAP-019: planet-updater corrupt numeric record behavior

Affected coverage:

- `YT-SUB2:0A97..0FD5` planet-production updates over edited or corrupt
  planet records;
- exact raw MBF arithmetic, persistence, and failure residue for dirty-zero
  and negative numeric fields; and
- every scanner, planet-menu, computer-report, projectile, and maintenance
  caller that can pass such a record to the shared updater.

The completed `docs/gameplay/planet-economy.md` explicitly leaves overflow
and error behavior for deliberately corrupt numeric records outside its
result. The native `updater_validate_record()` currently stops with
protective `YT_RANGE` results for dirty exponent-zero values in selected
fields and for every negative nonzero field. Those guards prevent unsafe or
unsupported host arithmetic, but they are not an evidence-backed claim about
the shipped BASIC transition, ERR/ERL identity, partial process/FIELD state,
or whether a write is reached.

Upstream documentation, the canonical raw model, generated evidence, and
focused fixtures must specify these corrupt domains before the guards can be
replaced or classified as exact behavior. No binary inspection or new
reverse engineering was performed.

### DOC-GAP-018: plasma wait destination roots

Affected coverage:

- the two one-second plasma prelaunch waits;
- every half-second per-hop plasma route wait; and
- their process-image and dependency-failure residue across the plasma
  resolver.

The completed `docs/runtime/plasma-bolt-output.md` specifies the exact
SINGLE one and half-second values, call order, and that every `94FD` call
overwrites its by-reference duration cell with an absolute deadline. Neither
that document, the generated plasma evidence nor the global-state registry
names the mutable DS destination used by any of those calls. The native
implementation therefore retains typed durations at these boundaries but
cannot bind the correct process roots or claim their failure residue without
inventing addresses. Upstream documentation and generated evidence must name
the caller cells and their reuse/aliasing rules. No binary inspection or new
reverse engineering was performed.

### DOC-GAP-017: radio private-pager wait destination root

Affected coverage:

- the shared radio reader's strictly-greater-than-22 private-pager pause;
- command-6 and automatic post-login radio scans that reach that pause; and
- exact process-image residue on wait and later output/file failures.

The completed `docs/runtime/radio-message-output.md` specifies the 99-second
value, exact `94FD` polling semantics, count reset, and post-wait blank, but
does not identify the caller-owned mutable duration/deadline cell. The
global-state registry has no matching radio private-pager wait root. Existing
`ytradio.static.txt` values describe the separate radio-door 33-second wait,
not this gameplay reader's 99-second caller cell. The native reader can retain
its typed wait callback but cannot make the correct process cell authoritative
without an upstream destination address and alias/lifetime contract. No
binary inspection or new reverse engineering was performed.

### DOC-GAP-016: Xannor-victory wait destination root

Affected coverage:

- Headquarters-victory `[PAUSE]` suffix at `YT-SUB:A83E..A850`;
- authoritative process-image binding of its 99-second duration/deadline
  cell; and
- exact wait-failure residue in the joined direct, missile and plasma
  Headquarters-victory callers.

The completed `docs/runtime/xannor-victory-output.md` specifies that `A83E`
copies MBF32 99 and that `A84B` passes a caller-owned cell by reference to
the shared `94FD` wait, which replaces that same cell with the absolute
deadline. Generated `ythq-victory.static.txt` and
`ythq-victory-output.static.txt` identify `DS:A9D8` as the raw
`00 00 46 87` source value, but neither source identifies the mutable
destination root written by the copy and passed to `94FD`. The global-state
registry also has no Headquarters-victory wait root.

The native implementation can preserve the typed 99-second behavior but
cannot make the correct process cell authoritative or pin its failure
residue without inventing that destination address. Upstream documentation,
the canonical world model, generated evidence and registry must name the
destination root before this boundary can proceed. No binary inspection or
new reverse engineering was performed.

### DOC-GAP-015: zero-Headquarters repair literal conflict

Affected coverage:

- startup configuration hydration/validation at `YT-SUB:BE82..BEA0`;
- authoritative binding of process-global Headquarters cell `DS:4BD8`; and
- every later main-session consumer of the repaired Headquarters value.

The completed prose and generated raw model disagree about the value copied
when configuration offset 117 is numeric zero. The following sources say the
repair value is 85:

- `docs/runtime/startup-config-hydration.md`;
- `docs/runtime/startup-login.md`;
- `docs/gameplay/daily-maintenance.md`;
- `docs/runtime/initialization.md`;
- `docs/runtime/configuration-editor.md`;
- traversal row 10 and `ytstartup-config-hydration.static.txt`; and
- the existing typed C model and fixtures.

However, `tools/ytstartup_config_hydration.py::repair_zero_headquarters_raw`
installs literal bytes `00 40 37 8A` in both record offset 117 and
`DS:4BD8`, and `tests/test_ytstartup_config_hydration.py` explicitly asserts
those bytes. They decode as MBF32 733, not 85. The model's docstring also
says it is applying the `BE82` repair and stops before the physical PUT, so
this is not merely an unrelated shipped nonzero fixture value.

Implementation cannot make `DS:4BD8` authoritative without choosing which
of these mutually exclusive values and failure prefixes are correct. The
uncommitted Headquarters migration was reverted. Upstream documentation,
the canonical raw model, generated evidence, and regression test must be
reconciled before this boundary can proceed. No binary inspection or new
reverse engineering was performed.

### DOC-GAP-014: command-6 dependency failure projections

Affected coverage:

- ship-computer command-6 radio-log scan and private wait;
- its active-computer entry/body/fresh-prompt cycle; and
- completion claims for physical and shared-handler failures.

The completed `docs/runtime/computer-radio-log-output.md` and shared radio
reader, random-file, output, wait, prompt, input, and framebuffer contracts
fully specify the successful complete-record cycle. They compose command
typeahead, the private pager, all endpoint predicates, return hydration, the
fresh prompt, later AB36/F8 activity, and inherited framebuffer state without
requiring command-specific interleaving vectors or a fixed final image.

The command-specific document explicitly leaves unidentified the fatal
`OPEN`/`LOF`/radio-`GET`/name-`GET`/direct-output/private-wait/`CLOSE`
shared-handler suffixes, persistent `ERR 24` retries, partial random-file
records and physical device writes, and entry/return `A41C` anti-cloak
conversion or handler failures. The already-emitted prefix, successful FIELD
order, and ordinary return remain implementable, but native work must not
invent the missing BASIC `ERR`, `ERL`, retry statement, active-handler, or
partial-I/O projection at those cuts. Upstream documentation is required for
those identities. No binary inspection or new reverse engineering was
performed.

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

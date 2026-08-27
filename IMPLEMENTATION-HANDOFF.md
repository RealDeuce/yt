# Yankee Trader C17 implementation handoff

## Read this first

This document is for the fresh session that resumes work on this door. It was
written while the separate reverse-engineering effort was still advancing.
The user has said that reverse engineering will be complete when work resumes.
Do **not** continue from any frontier remembered from the conversation or from
the analysis state at the time this handoff was written. Read the completed
analysis at its current state and derive the remaining C work from that.

Door root:

```text
/bbsdev/doors/yt/ng
```

Completed Yankee Trader 3.6G analysis:

```text
/bbsdev/doors/yt/3.6G
```

OpenDoors reference manual and source:

```text
/bbsdev/doors/yt/ng/third_party/opendoors
```

Reference C/OpenDoors door:

```text
/synchronet/src/sbbs/src/doors/clans-src
```

This implementation has its own Git repository. OpenDoors is pinned as the
`third_party/opendoors` submodule; do not substitute the stale Synchronet
source copy. The completed reverse-engineering tree is external evidence, not
part of this repository and not a writable implementation workspace.

## The task

Reimplement Yankee Trader 3.6G in C17 using OpenDoors. The target is Windows,
Linux, FreeBSD, macOS, and other platforms supported by the selected
interfaces.

Exact compatibility is the default requirement:

- From the player's point of view, the native door must be indistinguishable
  from 3.6G for every normal, alternate, error, interruption, timeout,
  carrier-loss, and SysOp-triggered path documented by the analysis.
- The native utilities must retain the same SysOp functionality and
  persistent side effects. Their native interface need not imitate DOS where
  the user has explicitly relaxed that requirement, but behavior and files
  still must.
- Never decide how the game “should” behave. If the completed analysis does
  not answer a behavior needed for implementation, stop and ask the user so
  the analysis can be updated.
- Do not replace an exact child operation with a convenient aggregate result.
  Preserve operation order, intermediate persistence, output order, random
  draws, interruptible boundaries, error prefixes, and non-atomic failure
  behavior.
- Do not describe a subsystem as complete merely because its happy-path game
  rule exists in C. Completion requires comparison against the completed
  analysis and proportionate tests.

There are three intentional high-level departures from the original:

1. Use a good platform CSPRNG instead of BRUN's poor generator. Preserve the
   documented number, placement, and use of random draws; replace only the
   source of each new uniform value. `src/yt_random.c` currently draws 24
   random bits from the platform entropy provider and converts them to a
   `float` in `[0,1)`.
2. Use `float` and `double` internally, but use MBF32/MBF64 exclusively at
   legacy serialization boundaries. Do not introduce IEEE floating-point
   bytes into any compatible file.
3. Retain every reached local BASIC `PLAY` operation as an ordered logical
   presentation event, but perform no host audio operation for it. OpenDoors
   exposes no portable local ANSI-music or BASIC `PLAY` endpoint, and the user
   explicitly selected this no-op supported-platform boundary on 2026-08-26.
   Remote ANSI-music and BEL bytes remain exact and are not covered by this
   departure.

## Source authority

Use evidence in this order:

1. The completed byte-pinned 3.6G analysis and its generated artifacts.
2. Original executable observations and retained fixtures linked by that
   analysis.
3. Original documentation and change history, only with the precedence
   assigned to them by the analysis.
4. Existing C code in this door.

The C code is an implementation candidate, not an authority. Some of it was
written before exact presentation, BRUN behavior, error edges, and connected
control flow were known.

Start with these analysis indices, at their current post-completion content:

```text
/bbsdev/doors/yt/3.6G/docs/STATUS.md
/bbsdev/doors/yt/3.6G/docs/architecture/reverse-engineering-traversal.md
/bbsdev/doors/yt/3.6G/docs/architecture/player-output-coverage.md
/bbsdev/doors/yt/3.6G/docs/architecture/root-coverage.md
/bbsdev/doors/yt/3.6G/docs/architecture/global-state-registry.md
/bbsdev/doors/yt/3.6G/docs/file-formats/file-lifecycle.md
/bbsdev/doors/yt/3.6G/docs/runtime/used-brun-operations.md
```

Do not treat a high-level semantic oracle as a substitute for connected raw
world, I/O, output, and return evidence. The analysis often retains both;
use the connected component for implementation and the higher-level result as
an independent cross-check where appropriate.

## First actions after resuming

1. Read this document and the completed analysis indices above.
2. Confirm the analysis tree is at its completed state. Ignore the former
   returning-login/Xannor frontier; it was still moving when this document was
   written.
3. Run the native baseline:

   ```sh
   cd /bbsdev/doors/yt/ng
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

4. Create an implementation-coverage ledger in this door. Map every final
   analysis component, reachable edge, external effect, and executable to:

   - the C function that implements it;
   - the exact test or fixture that proves it;
   - `missing`, `candidate`, `verified`, or `explicitly deferred` status; and
   - any prerequisite shared runtime, input, output, file, or numeric work.

   Do this before making broad claims or opportunistic gameplay edits. The
   analysis coverage ledgers prove analysis ownership, not C implementation
   ownership.
5. Audit from shared foundations outward. When a common primitive is wrong,
   fix and test it before repairing every caller separately.
6. Keep exact compatibility, exploit hardening, terminal enhancements, and
   native automation as separate scopes. Do not let an enhancement silently
   alter compatibility mode.

## Current door contents

The build produces these seven programs:

```text
yt
ytmaint
ytconfig
yt-init
rmt-init
portname
local
```

The authoritative build is in `CMakeLists.txt`. It consumes the pinned
`third_party/opendoors` checkout through the upstream `OpenDoors::Static`
CMake target. `GNUmakefile` is only a convenience wrapper around that same
build graph; it is not an independent legacy integration.

The main implementation areas are:

| Area | Current files | Present state, not a completion claim |
|---|---|---|
| MBF and QuickBASIC helpers | `src/qb.c`, `src/qb.h`, `src/yt_score_format.c` | MBF32/64 conversion, INT/FIX/CINT/VAL, string normalization, generic numeric output, and a dedicated PRINT USING scoreboard formatter exist. Their edge domains still require final-analysis audit. |
| Raw records and files | `src/yt_data.c`, `src/yt_file.c`, `src/yt_text.c`, `src/yt_names.c` | 137-byte records, 86-byte radio records, DOS EOF text files, case-insensitive DOS-style path resolution, alias files, and mutation-in-place APIs exist. Typed encoders retain the original raw record and rewrite only values that compare changed, protecting unbound bytes and noncanonical zero residue in many cases. |
| Platform layer and RNG | `src/yt_platform.c`, `src/yt_random.c` | Windows entropy/process/clock code and Unix-like FreeBSD/macOS/Linux branches exist. Only the present FreeBSD build has been exercised in this handoff. |
| Initializers | `src/yt_init.c`, `src/main_yt_init.c`, `src/main_rmt_init.c` | World generation, auxiliary files, direct-versus-maintenance handoff, and RMT reset paths exist. A clean-install subprocess test now exists, but full fault ordering and every randomized byte have not thereby been proven. |
| Maintenance | `src/yt_maint.c` | Player expiry, port/planet production, radio compaction, news rotation, scoreboard, Wanderer, Xannor, mercenary, and lottery code exists. Its size and breadth are not evidence that all final analysis branches or prefixes match. |
| Configuration and utilities | `src/main_ytconfig.c`, `src/main_portname.c`, `src/main_local.c` | Broad interactive behavior exists and has limited executable-level tests. Exact transcripts, errors, and all mutation boundaries still need ledger-based verification. |
| Scoreboard | `src/yt_score.c`, `src/yt_score_format.c` | Generation and cached-score writes exist. Boundary fixtures cover many PRINT USING overflow and exponent forms. Compare the entire final score contract, including failures and partial output. |
| OpenDoors adapter | `src/yt_door.c`, `src/yt_input.c`, `src/yt_input_model.c`, `src/yt_output.c`, `src/main_yt.c` | The player session now obtains origin-tagged raw OpenDoors events through a split local/remote FIFO for the distinct B05D and AB36 selection predicates. The pure arbitration and B05D queue mutations have deterministic fixtures, but physical overflow/failure behavior, remaining input consumers, startup/shutdown, and cross-platform behavior are not closed. |
| Player session | `src/yt_session.c` | Roughly 9,600 lines cover admission, movement, hazards, combat, ports, Earth, planets, teams, missiles, plasma, radio, computer reports, and exit. Cruise/plasma owners now map their recovered opening, route/reroute, defense, mine, player, planet, news, sound, wait, and terminal paths; the action finalizer, shared port-treasury body, movement danger scanner, team audit ordering, press-any-key presentation, both profit reports, the Info panel, the silent planet-production updater, and the nearest-port layer scanner have also been reconciled structurally. The six launch-time commodity-base draws are now restored through the replacement CSPRNG boundary. Treat all of it as candidate code requiring compositional comparison with completed connected analysis. There will be no monolithic end-to-end session transcript requirement; use deterministic native component fixtures plus separately verified presentation, persistence, input, RNG, and physical-adapter boundaries. |
| Original package assets | `src/yt_assets.inc`, `src/main_assetgen.c`, `data/` | Eight distributed files are regenerated and checksum-tested. |

There are few `TODO` markers. Their absence is not evidence of closure.

## Current test baseline

The native baseline command is:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

It ran:

```text
test_qb: ok
test_data: ok
test_sound: ok
input-model tests passed
test_presentation: ok
test_assets: ok
test_score: ok
test_clean_install: ok
test_utilities: ok
```

The tests currently prove useful foundations:

- selected MBF32/MBF64 vectors and QuickBASIC numeric/string helpers;
- bounded B05D/AB36 input-source arbitration, source FIFO, B05D control-key
  queue mutations, and editor endpoint routing;
- basic raw-record preservation, CSPRNG injection, random-file I/O, and DOS
  EOF text append behavior;
- exact sizes and checksums of the eight distributed package assets;
- selected scoreboard PRINT USING boundaries, the 603-byte empty-game
  scoreboard, cached scores, and partial-file behavior;
- an actual `yt-init` subprocess in a fresh package-like directory, followed
  by the sibling `ytmaint`, with expected output markers, core file sizes,
  configuration defaults, player defaults, fixed sector/ports/planets, and
  the early-EOF destructive truncation boundary; and
- limited executable behavior for YTCONFIG, PORTNAME, standalone RMT-INIT,
  and LOCAL.

They do **not** currently prove:

- any complete remote or local player transcript;
- ANSI/plain output bytes, framebuffer results, pagination, or arbitrary
  user/file text safety;
- OpenDoors initialization, command-line parsing, carrier loss, timeout,
  chat, menu/toolbar commands, SysOp actions, or shutdown;
- caller-specific sound placement through deterministic native component fixtures (the dispatcher and 59-site static inventory now have exact focused tests/mapping);
- exact session mutation and random-draw order across gameplay branches;
- runtime error routing, partial physical I/O, interruption points, or
  non-atomic failure prefixes across the whole door;
- full `ytmaint`, YTCONFIG, RMT-INIT, PORTNAME, or LOCAL transcripts and
  fault families;
- Windows, Linux, or macOS builds; or
- mixed operation in which original and native binaries alternately mutate
  the same files.

The clean-install test intentionally checks structure and invariants rather
than a single full-world hash because the replacement RNG is intentional and
the clock is live. Add deterministic-provider/time fixtures when exact
randomized mutation order must be proved.

## Known high-priority gaps

### 1. Player session is broad but not compatibility-audited

`src/yt_session.c` contains much of the game's apparent functionality, but
many strings and presentations visibly predate the final exact analysis. It
also uses many ordinary C `%.9g`/`%.15g` formats. Those are not general
substitutes for BRUN numeric conversion or QuickBASIC PRINT USING.

Do not perform a spot review and call the session complete. Use the completed
player-output and rooted-control-flow ledgers to verify every entry, branch,
join, output byte stream, local result, state mutation, file effect, random
draw, and exit. Replace shortcuts with shared exact components where the
analysis shows shared code.

### 2. General OpenDoors output is knowingly unsuitable

`src/yt_output.c` currently sends all output through
`od_disp(..., TRUE)`. On Win32 the local display does not parse ANSI on that
path; ESC and following bytes become visible glyphs. This already makes
`YTOPEN.ANS` wrong locally. The same path is unsafe for unsanitized player,
BBS, registration, message, and external-file text.

Read `OPENDOORS-IO.md` in full before changing any I/O call. In summary:

- Intended NUL-free ANSI/AVATAR presentation should normally use
  `od_disp_emu(text, TRUE)`, with `od_no_ra_codes = TRUE` and every other
  relevant OpenDoors global set deliberately. In that configuration the
  intended terminal commands are interpreted for the Win32 local display
  while the original string is transmitted remotely.
- Use `od_disp()` with local echo only for exact counted output that is proven
  to contain no terminal-emulator commands. Its special reasons for existing
  in this door are counted or embedded-NUL data and remote-only output, not
  general presentation.
- Arbitrary data must follow the original validation/sanitization rules; do
  not invent filtering and do not treat terminal emulation as sanitization.
  A blanket switch from `od_disp()` to `od_disp_emu()` would let hostile or
  incomplete sequences in that data alter the local screen and the
  emulator's persistent parser state.
- `od_printf()`, `od_input_str()`, `od_disp_emu()`, and `od_send_file()` each
  add behavior and are not wholesale replacements.
- The installed `CP437UTF8` option must remain off for byte-exact CP437
  sessions.

The ineffective pre-`od_init()` assignment to `od_always_clear` is also
documented there. Its correct post-init value must come from completed
behavioral evidence, not preference.

### 3. Sound dispatcher and caller integration are implemented candidates

`src/yt_sound.c`, `src/yt_presentation.c`, `src/yt_output.c`, and
`src/yt_session.c` now implement the recovered dispatcher, the `X` toggle,
and every qualified live member of the 59-static-caller inventory.  The
alternate `YT:4F2D` selector-1 transfer arm is recorded as qualified dead:
the completed transfer oracle proves that the action initializes its private
flag to zero and never changes it, so only `YT:4F48` selector 4 is admitted
without asynchronous memory corruption.  This remains candidate rather than
verified whole-session behavior until caller-specific transcript/effect and
physical-endpoint fixtures are joined.

Legacy ANSI music must be sent as exact counted **remote-only** bytes.
OpenDoors does not parse it correctly for its Win32 local display:

- `od_disp(..., TRUE)` renders the sequence as glyphs;
- `od_disp_emu()` treats legacy `CSI M` as Delete Line and then renders PLAY
  bytes; and
- the terminal byte `0x0e` is Shift Out on a real VT-compatible terminal.

Local BASIC `PLAY` is retained in the ordered logical presentation timeline
but deliberately causes no host audio operation, per authorized departure 3
above.

`TERMINAL-COMPATIBILITY.md` records a possible positively detected CTerm
`CSI |` enhancement and the raw-input demultiplexer it would require. That is
an enhancement, not permission to alter compatibility mode. No fallback
policy has been selected, so do not implement one by judgment.

### 4. OpenDoors SysOp integration is incomplete

The DOS personality/custom-hotkey mechanism is not a portable implementation
of legacy F4-F10 controls and is not used by the Win32 local interface.
OpenDoors' Win32 buttons, menus, accelerators, and chat facilities should
provide the native SysOp interface.

The SysOp view need not be identical to DOS, but every action must have the
correct consequence for the remote player and game state. In particular,
OpenDoors time adjustment currently changes `od_control.user_timelimit`,
while `src/yt_session.c` enforces its own `session_deadline`; these can
diverge. Chat, exit/hangup, time changes, carrier handling, and any relevant
local presentation must be connected and tested. The exact legacy behavior
is in the completed `docs/runtime/sysop-controls.md` analysis; the OpenDoors
selection constraints are in `OPENDOORS-IO.md`.

### 5. Cross-platform OpenDoors startup and exit remain deferred

The user explicitly deferred final OpenDoors startup/exit work on platforms
other than the currently exercised one until later work. `src/main_yt.c` and
`src/yt_door.c` contain a provisional WinMain/main split, command-line
prefill/parsing, cleanup hook, `od_init()`, `od_exit()`, and final `exit()`.
Do not assume it is approved merely because it builds.

Preserve these non-negotiable facts when that phase is authorized:

- OpenDoors can call `exit()` during initialization and ordinary I/O.
- Install all cleanup state and `od_before_exit` before any call that may
  initialize OpenDoors.
- `od_parse_cmd_line()` has different Win32 and non-Win32 signatures and
  subtle ownership/ordering rules.
- It must be called exactly as required relative to `od_init()`.
- Avoid double initialization and double shutdown.
- Preserve a valid cleanup path for partially opened game state.

Until that phase, do not let unrelated compatibility work casually rewrite
these boundaries.

### 6. Error and persistence behavior needs explicit coverage

The original programs often write output, mutate a FIELD buffer, perform a
`PUT`, append a line, delete a file, or rename a file before a later
operation fails. Those prefixes are observable and frequently non-atomic.
The C code generally propagates a single `yt_error`, but that alone does not
reproduce BRUN error selection, retries, cleanup, or already-committed
effects.

Use the final file lifecycle and error-router analysis. Test injected
failures at each documented physical operation, not just a generic failure
at the outer function.

## Persistent compatibility rules

- `YTDATA.DAT` records are exactly 137 bytes. Preserve all fields not written
  by the reached operation, including bytes 133-136 and any layout-specific
  residue.
- `YTRMSG.DAT` records are exactly 86 bytes, including the documented
  maintenance compaction behavior.
- Preserve MBF exponent-zero values when the original operation does not
  rewrite them. Numeric equality is not always byte equality.
- Preserve text CRLF, DOS EOF, padding, truncation, append positioning, and
  per-line durability exactly.
- Preserve each executable's literal filename spelling and operation order.
  The native filesystem compatibility layer must provide DOS-style
  case-insensitive lookup on case-sensitive hosts without casually treating
  distinct host directory entries as interchangeable. Audit ambiguous
  case-colliding files explicitly.
- The native and original binaries must be able to alternate over the same
  files. Test both directions with documented fixtures: original-created
  files mutated natively, and native-created files consumed and mutated by
  the original binaries.
- Do not add fields to legacy records, assign meanings to unbound tails, or
  add required sidecar state for exact mode.

The earlier fresh-install failure was caused by an incorrect maintenance
rotation implementation, not by permission to skip rotation. The analysis
establishes that YTMAINT opens/closes both current and yesterday news in
append mode before deleting yesterday and renaming current. Thus missing
files are created before the destructive operations. `rotate_news()` now
models that order, and `tests/test_clean_install.c` exercises a fresh
`yt-init` handoff. Retain this regression.

## Existing design notes in this door

These documents are requirements or discussion records, not proof that their
features are implemented:

- `OPENDOORS-IO.md`: complete OpenDoors I/O function-selection study and
  current call-site audit. Treat its warnings as active until code and tests
  prove otherwise.
- `TERMINAL-COMPATIBILITY.md`: possible CTerm/SyncTERM ANSI-music capability
  detection. No policy decision has authorized it.
- `EXPLOIT-MITIGATIONS.md`: exact-mode exploit behavior and possible future
  hardened rules. Do not implement hardened behavior without an explicit
  ruleset decision.
- `NATIVE-AUTOMATION.md`: inventory of terminal scripts/macros that should
  eventually have native, terminal-independent equivalents. These are
  extensions, must use ordinary game operations and costs, and are not a
  substitute for completing exact compatibility.

The important separation is:

```text
exact 3.6G compatibility
    != exploit-hardened rules
    != terminal-safety enhancements
    != native player automation
```

Do not combine these in one unlabelled mode.

## Recommended verification order

This is an engineering dependency order, not a decision about game behavior:

1. Build the final implementation-coverage ledger from the completed
   analysis.
2. Verify and extend MBF, BRUN numeric formatting, PRINT USING, date/time,
   string, path, raw-record, and file-operation foundations.
3. Verify initializers and noninteractive maintenance with deterministic
   clock/RNG/I/O providers and byte-level fixtures, including every failure
   prefix.
4. Replace the general output shortcut with provenance-aware exact output
   paths. Make carefully configured `od_disp_emu()` the normal path for
   intended ANSI/AVATAR presentation, reserve `od_disp()` for its narrower
   counted/plain and remote-only roles, and add remote-byte plus Win32
   local-framebuffer tests.
5. Add caller-specific component/effect fixtures around the implemented sound dispatcher and its 59-site static inventory.
6. Build reusable input/session-runtime adapters for exact queue, pager,
   timeout, carrier, and SysOp consequences.
7. Audit startup/login and then each reachable session component in rooted
   control-flow order, composing shared exact children instead of creating
   caller-specific approximations.
8. Verify YTCONFIG, PORTNAME, RMT-INIT, LOCAL, and YTMAINT whole-program
   transcripts and persistent effects.
9. Complete the explicitly deferred platform-specific OpenDoors startup and
   exit work when authorized.
10. Only after exact compatibility is closed, ask for decisions needed by
    terminal enhancements, hardened rules, and native automation.

For each component, tests should cover at least:

- exact remote bytes for ANSI and plain modes;
- the correct local result, especially the Win32 80x25 display;
- accepted, rejected, empty, repeated, queued, local, and remote input;
- normal, alternate, END, carrier-loss, timeout, and SysOp-triggered exits;
- raw before/after records and auxiliary files;
- random draw order with an injected provider;
- time/date boundaries, including BRUN's recovered oddities;
- every documented partial physical I/O and error edge; and
- continuation into the real caller without replaying predecessor output or
  mutation.

## Stop conditions

Stop and ask the user when:

- the completed analysis lacks a value, branch, byte sequence, failure
  ordering, platform consequence, or external observation needed by the C
  implementation;
- two final analysis artifacts disagree;
- an apparent fix requires choosing between exact behavior and a safer,
  cleaner, or better-balanced behavior;
- terminal capability fallback, hardened-rule constants, automation
  semantics, or new persistent state requires a policy decision; or
- completing a task would require expanding into the explicitly deferred
  OpenDoors startup/exit phase without authorization.

Do not fill these gaps with conventional behavior, likely intent, host API
defaults, or personal judgment.

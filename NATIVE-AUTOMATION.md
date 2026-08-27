# Native player automation notes

## Purpose

The strategy material distributed at
`../../syncterm/yt.starflt.com` depends heavily on terminal macros, ZOC REXX
scripts, captured session logs, text-editor macros, Python post-processing,
and an Excel workbook. These tools give substantial advantages to players
whose terminal can run them.

The replacement should expose the corresponding operations through Yankee
Trader itself. A player using a basic ANSI terminal should not need ZOC,
terminal-specific key maps, local scripting, a session-log parser, or a
spreadsheet to perform established Yankee Trader workflows.

This document is an inventory and requirements note, not yet an interaction
or implementation specification. It deliberately does not choose prompts,
keys, menu placement, persistence, limits, or stopping policy where the
source material does not determine them.

## Compatibility boundary

Native automation is an extension to the recovered game, not evidence about
the behavior of the 3.6G executable. Exact 3.6G compatibility must remain a
separate selectable behavior.

An automated operation must not become a privileged database query merely
because the server already has the data. It must expose only information that
the equivalent player commands would expose, and must preserve the ordinary
cost and side effects of each constituent action:

- turns, credits, cargo, ammunition, fighters, mines, cloak, and score;
- random-number draws and their action-relative order;
- planet and port production updates;
- spy sweeps;
- danger checks, combat, black holes, mines, and emergency warp;
- radio, news, and scoreboard effects;
- player and Xannor retaliation;
- pauses, confirmations, aborts, death, carrier loss, and SysOp intervention;
- persistent writes and visibility to concurrent sessions.

For example, a native missile sweep means launching ordinary missiles one at
a time through the ordinary projectile path. It does not mean reading every
sector record and returning the same final discoveries for free.

The batch boundary is also significant in a multiplayer game. The source
scripts wait for ordinary prompts between actions, allowing other sessions
and local SysOp events to interleave. Whether native automation preserves
that interleaving and presentation pacing requires an explicit specification;
the whole batch must not silently become one atomic operation.

## Two unrelated sets of function keys

The strategy guide's F1 through F12 definitions are **player-side ZOC key
mappings**. Pressing one makes the terminal transmit a text command sequence.
They are available to a remote player only because that player's terminal was
configured to provide them.

They are unrelated to the executable's local-only F4, F5, F8, F9, and F10
event handlers documented in
`/bbsdev/doors/yt/3.6G/docs/runtime/sysop-controls.md`. Those are local SysOp
controls whose effects must also be reflected correctly to the remote player.

Implementation and tests should call the first set player shortcuts or
workflows, not “F-key controls,” to avoid confusing it with the SysOp event
system.

## Source inventory

### Distributed programs and data

| Source | External dependency | Automated purpose |
|---|---|---|
| `yt/c3script.zrx.html` | ZOC REXX | Replace computer option 3: obtain the route, decline normal autopilot, move through it, and scan every traversed sector |
| `yt/f1scan.zrx.html` | ZOC REXX | Scan adjacent sectors, visit each one, drop one fighter, scan, and return |
| `yt/3000scan.zrx.html` | ZOC REXX plus a local CSV | Work through a target-sector list, traverse and scan routes, remove observed sectors from the worklist, and save progress |
| `yt/3000-1.zrx`, `3000-2.zrx`, `3000-orig.zrx`, `1000.zrx` | ZOC REXX and session logging | Autopilot to and scan a sequential range of sectors to create a universe log |
| `yt/10scan/10-scan.zrx` | ZOC REXX and session logging | Run computer option 10 from sector 1 to every sector |
| `yt/10scan/10-scan.py` | Python and the captured option-10 output | Identify coverage targets, dead ends, long warps, and isolated sector groups; generate more ZOC scripts and lists |
| `yt/scripts.txt` | Clipboard or terminal macro | Fire one missile at a grid-ordered sequence covering the universe |
| `yt/scripts2.txt` | Clipboard or terminal macro | Run computer planet report option 9 over a grid-ordered sequence of sectors |
| `yt/fighter script.txt` | Captured port list, clipboard, terminal logging, and text search | Query every known port with option 2 and find sectors returning `No information available.` |
| `yt/ytports.py` | Python and an ANSI-free option-14 capture | Extract port sector numbers and generate the batched option-2 port scan |
| `yt/ytcalc.xls` | Excel-compatible spreadsheet | Perform combat, planet, production, bribe, and credit calculations |
| `yt/gl-logon.txt`, `yt/tohc-logon.txt` | ZOC | Navigate particular BBS logon/menu sequences |
| `yt/yt-time-t1.zrx`, `yt-time-t2.zrx`, `yt-time-t3.zrx` | ZOC | Benchmark terminal output, port-pair reporting, and path finding |

The website also publishes command strings and workflows that are not stored
as separate script files. They are included below.

### Classification

The logon scripts operate before Yankee Trader and are BBS-specific. They
cannot be made into an in-door player command. The timing scripts are
developer/SysOp benchmark tools, not gameplay conveniences. They belong in
the replacement's test or benchmark tooling rather than its player menu.

Scheduled downloading of a BBS-hosted scoreboard and newspaper is likewise a
host/web archival operation. A native historical-news feature could replace
the player need, but that would require new retained history and is not
equivalent to a session command.

The remaining items describe player gameplay, analysis, record-keeping, or
calculations and are candidates for native exposure.

## Native workflow inventory

### 1. Common player shortcuts

The strategy guide recommends the following ZOC key mappings:

| Player shortcut | Transmitted sequence | Purpose |
|---|---|---|
| F1 | `C;4;;1` | Scoreboard |
| F2 | `C;8;T;NS;1` | Today's newspaper |
| F3 | `C;8;Y;NS;1` | Yesterday's newspaper |
| F4 | `S;M` | Scan, then prompt to move |
| F5 | `L;3;;P;;;0;L;3;;P;;;0/R20` | Repeated equipment sale from a planet to its port |
| F6 | `L;2;;P;;;0;L;2;;P;;;0/R20` | Repeated organics sale from a planet to its port |
| F7 | `L;1;;P;;;0;L;1;;P;;;0/R20` | Repeated ore sale from a planet to its port |
| F8 | `L;T;C;P;;/R20` | Repeated cargo transfer from a port to a planet |
| F9 | `P;;;;;L;T;C;3;/R20` | Sell equipment to the port and transfer purchased cargo to the planet |
| F10 | `P;;;;;L;T;C;2;/R20` | Sell organics to the port and transfer purchased cargo to the planet |
| F11 | `P;;;;;L;T;C;1;/R20` | Sell ore to the port and transfer purchased cargo to the planet |
| F12 | `F;1;S;M` | Drop a one-fighter breadcrumb, scan, and prompt to move |

These are the clearest minimum set of terminal-independent conveniences.
Native entry points can invoke the same underlying actions without requiring
the player to configure function keys or memorize opaque command strings.

The guide also relies on the game's command queue, `/R` repetition, Ctrl-R
reuse, and Ctrl-X cancellation. Those are already game mechanisms, although
the shipped repeat-count behavior is buggy. A native interface still needs
documented equivalents for:

- repeating a selected workflow a chosen number of times;
- repeating the previous workflow; and
- aborting safely between constituent actions.

Whether an extension retains or corrects the original `1..10` versus
`11..20` repeat-count bug must be specified separately from exact
compatibility mode.

### 2. Route execution with automatic scanning

The replacement `c;3` ZOC script asks the game for the ordinary shortest
route, declines the ordinary autopilot, parses the displayed route, and then
moves through it one sector at a time while scanning. It scans the starting
sector after leaving the computer, then scans every parsed route sector,
including the destination.

A native scan-as-you-travel operation should therefore be based on the same
option-3 route that the player could request. Each movement and scan must
remain an ordinary action with its normal costs, hazards, output, cache
effects, and interruption points.

The following extension details are not specified by that script:

- what happens when a danger warning declines or interrupts the planned
  route;
- whether the player confirms the route once or confirms individual
  hazardous moves;
- whether the remaining route is retained after an interruption; and
- how results are summarized in addition to, or instead of, ordinary
  transcripts.

### 3. Adjacent scan and fighter breadcrumbs

`f1scan.zrx` scans the current surroundings, extracts adjacent sectors, visits
each, drops one fighter, scans, and returns to the original sector. The
strategy guide also supplies the simpler `F;1;S;M` breadcrumb shortcut and
uses adjacent one-fighter placement while back-tracing Xannor retaliation.

Native support is needed for at least:

- scan and place a chosen fighter count in each adjacent sector;
- scan each visited adjacent sector;
- return to the starting sector when an ordinary route remains available;
  and
- show which branches completed or stopped.

Dropping fighters, moving, and scanning must use their ordinary paths.
Whether return failure stops the operation, starts a new path search, or
leaves the remaining branches pending is not documented and needs a decision.

### 4. Sector worklists and coverage runs

Several scripts maintain or manufacture a list of sectors and perform the
same operation on each:

- sequentially visit and scan every numbered sector;
- visit and scan every known dead end;
- visit a sector adjacent to every dead end and scan the dead end remotely;
- inspect every known enemy planet;
- visit every owned planet for harvesting and defense maintenance;
- run planet report option 9 against every sector or selected sectors;
- query port information for every known port;
- launch one missile toward every grid target or every known dead end; and
- re-observe selected planets after a time interval.

This points to a native worklist/itinerary facility shared by navigation,
reports, and weapons rather than isolated terminal strings. A work item must
name a legitimate player operation and its inputs; it must not encode a
direct record read.

The source workflows do not establish:

- maximum list size;
- whether lists or progress survive logout;
- how a player creates, edits, imports, or names a list;
- whether a failed item is retried, skipped, or stops the run;
- whether dangerous or destructive items require per-item confirmation;
- how duplicate targets are treated; or
- how concurrent changes invalidate a prepared route.

These require specification before implementation.

### 5. Native universe mapping and topology analysis

The external mapping process currently has two forms.

The older process visits every sector, captures sensor output, removes
unrelated text with Notepad++ macros, then sorts and deduplicates in Excel to
obtain dead ends.

The newer `10-scan` process:

1. runs option `C;10;1;<sector>` for the entire sector range;
2. captures the route outputs;
3. counts sector occurrences in the routes;
4. identifies a smaller set of targets whose visit or missile paths cover
   unique locations;
5. identifies dead ends;
6. reports long-warp candidates; and
7. reports isolated groups of roughly 20 through 120 sectors with a single
   entrance.

All of those computations can be performed by the game without requiring a
particular terminal, text editor, Python, or Excel. Information disclosure
must nevertheless match the underlying option-10 reports. If option 10 lets
the player calculate the complete topology without turns, a native analyzer
may calculate it from those same results; it must not silently include data
that option 10 with the player's current state would not return.

Possible report families evidenced by the external tool are:

- dead-end sectors, separated by whether a port is present when that fact is
  legitimately known;
- minimum coverage targets;
- long-warps and the number of sectors beyond them;
- single-entrance isolated groups;
- “next-to-dead-end” sectors suitable for adjacent scanning; and
- a generated worklist for visiting, scanning, or launching ordinary
  missiles at a selected result set.

The exact graph definitions and the script's apparent fixed assumptions
about sector ranges need to be captured as test vectors before specifying
the native reports.

### 6. Missile reconnaissance and bulk launching

`scripts.txt` launches one missile at grid-ordered targets across the
universe. The ordering is intended to cover open space more efficiently than
strict numeric order. Other published sequences target known dead ends.

A native missile worklist must retain all ordinary projectile behavior,
including:

- one missile and the ordinary turn cost per launch;
- the route selected for that individual target;
- avoid-list effects;
- intermediate impacts and stopping;
- cloak and Anti-Cloak behavior;
- player and Xannor retaliation;
- news and radio output;
- black-hole rerouting; and
- termination on death, insufficient turns, insufficient missiles, carrier
  loss, or player cancellation.

The script encourages players to add discovered Xannor, friendly, and planet
sectors to option 7's avoid list and clear them later. Native set operations
for adding, removing, viewing, and temporarily applying a group of exclusions
would remove another large clipboard dependency.

This workflow intersects the exploit-mitigation questions in
`EXPLOIT-MITIGATIONS.md`. Providing equal native access does not by itself
decide whether remote projectile reconnaissance is retained in a hardened
ruleset.

### 7. Bulk planet reports

`scripts2.txt` runs computer option 9 across a grid-ordered sector sequence.
The strategy notes also keep separate option-9 lists for dead ends with and
without ports.

Native bulk planet reporting must reproduce the option-9 result for each
requested sector, including its limited, unavailable, and full-report paths.
It must preserve the option's nested side effects: the recovered 3.6G
analysis shows that this nominal report is not uniformly read-only.

Useful result grouping evidenced by the notes includes:

- planets found;
- sectors for which information was unavailable;
- no-planet results;
- changed ownership or a missing formerly known planet; and
- a list suitable for later navigation or ordinary weapon actions.

The external scripts reveal all raw per-sector results. Any condensed native
summary must not erase information necessary to distinguish those paths.

### 8. Port availability and hostile-fighter screening

The external port scan performs option 14 to obtain all port sectors, parses
the capture with `ytports.py`, runs option 2 against every port, captures the
output again, and searches for `No information available.` Such a result is
used as evidence that hostile fighters prevent port information from being
shown.

A native port sweep can perform those same option-2 queries and report:

- successful port reports;
- sectors returning the ordinary unavailable result; and
- the targets not yet investigated by travel or missile.

It must not identify what caused an unavailable result unless the ordinary
option-2 path reveals that information. The native result should retain the
original limitations: only port sectors are checked, it consumes no turns
when the underlying report consumes none, and it must not disclose a hidden
planet directly.

The notes use a related owned-planet audit: option 2 for owned planets in
port sectors and option 9 for owned planets without ports. This detects
unfriendly forces, weak defenses, useful stock, and changed planet state.
A native owned-planet audit should be composed from those same reports rather
than reading protected sector or planet fields directly.

### 9. Trading and planet servicing

The guide automates a port-pair trade with:

```text
m;117;p;;;;;m;115;p;;;;/r20
```

It also uses the F5 through F11 player shortcuts for repeated same-sector
planet/port transfers and describes visiting every owned planet to:

- harvest produced fighters and other inventory;
- sell produced cargo;
- transfer purchased cargo into production;
- equalize ground forces;
- replace missing sector-defense fighters; and
- check for mercenaries or hostile forces.

Native trade and service loops must perform each docking, trade, launch,
landing, transfer, and movement through the ordinary handler. The selected
quantity policy, price-change behavior, affordability response, stop
threshold, minimum reserved turns, and danger response are not determined by
the macros' blank-answer convention alone and need an explicit
specification.

Computer option 16 reports profitable port pairs, but players currently copy
the result into notes and manually append the number of full-hold trades
supported by current inventory. Native reports should expose the calculations
players currently perform externally:

- current per-unit projected profit;
- route or relative distance already exposed by the computer;
- the limiting inventory across both ports;
- the number of complete trips for the player's current hold count; and
- a worklist entry for ordinary travel and repeated trading.

The projection must be kept distinct from actual trading. Actual dynamic
prices, production updates, rounding, ownership receipts, and inventory
changes occur on each ordinary port visit.

### 10. Repeated observation

The guide uses repetition for several observational tasks:

- scanning repeatedly to reveal a cloaked player;
- visiting sectors adjacent to enemy planets, recording ground forces,
  waiting, then repeating the same itinerary to estimate stored credits;
- running daily owned-planet and defense audits; and
- comparing current owned-planet and fighter reports against previous notes.

Immediate repetition can be a native worklist operation. A delayed or
cross-session observation requires durable timestamps and samples that do not
exist in the legacy player record. That persistence issue must be resolved
before a native timed-observation feature is specified.

### 11. Calculators

The `ytcalc.xls` workbook contains one `Data` sheet with at least these
player-facing calculations:

- credits needed to raise planet productivity to one plasma bolt per day;
- fighters produced per day;
- plasma damage and plasma bolts needed;
- missile damage and missiles needed;
- Xannor score;
- ground-force and fighter credit requirements;
- maximum Xannor planet attack;
- mercenary bribe cost;
- fighter surrender from attacker/defender ratio;
- planet production and stored-credit estimation from ground-force growth;
  and
- total credits spent on planet development.

The website separately embeds a plasma calculator taking bolt count, target
type, and distance.

These should be available as native informational tools so combat and planet
decisions do not require Excel or a web browser. The formulas cannot be
copied blindly into a corrected ruleset: the spreadsheet and website encode
the patched executable's quadratic plasma behavior. Exact compatibility and
any corrected linear-plasma ruleset need separate calculator formulas.

Before implementation, each workbook formula should be extracted and checked
against the recovered executable. The workbook is strategy material, not
authoritative executable evidence.

### 12. Player map, notebook, and history

The strategy guide maintains external notes for:

- discovered dead ends and whether they contain ports;
- known planets, owners, defenses, inventory, production, and last visit;
- the date a planet location was publicly revealed;
- known Xannor and mercenary locations;
- known deployed fighters;
- current avoid-list membership;
- profitable port pairs and estimated remaining trades;
- candidate hiding sectors and distance from Earth;
- formerly owned planet locations needing reinspection; and
- archived daily newspapers and scoreboards.

Native maps and notes would eliminate dependence on Notepad++, terminal
scrollback, session logging, and scheduled webpage downloads. They introduce
a persistence problem, however. The documented `YTDATA.DAT` player schema has
no space for these records, and the legacy data files must remain byte
identical and usable by both binaries.

No storage choice is made here. The choices that require an explicit project
decision include:

- session-only notes;
- reports re-derived each login from currently legitimate queries;
- an optional new MBF sidecar with a documented interoperability boundary;
  or
- no persistent native notebook, while still providing native session
  automation and downloadable/plain-text reports.

It would be incorrect to reuse reserved bytes or undocumented record tails
without a separately documented format decision.

## Candidate delivery models

The inventory can be exposed through one or more of these models, but the
choice is not made by this document:

1. **Named commands.** Dedicated commands such as scan route, audit owned
   planets, scan known ports, or service planet.
2. **A native worklist menu.** The player chooses a legitimate action and a
   generated or entered set of sectors.
3. **A constrained plan builder.** The player composes supported game
   actions, repetition, and stop conditions without writing terminal text.
4. **A compatible command-language extension.** New queue syntax expresses
   the established workflows while remaining usable from any terminal.

Named commands are easiest to explain, while a shared worklist mechanism
matches most of the external tools. A general macro language raises much
larger parsing, recursion, resource-limit, and compatibility questions. These
tradeoffs need a deliberate decision rather than being inferred from the ZOC
scripts.

## Required stop and safety contract

Every mutating native workflow needs a specified result for at least:

- the player presses the native cancel key;
- Ctrl-X or the compatibility stop flag arrives;
- the carrier disappears;
- time expires or the SysOp adjusts remaining time;
- SysOp chat or other local events interrupt input;
- the player dies or emergency-warps;
- a danger scanner requests confirmation;
- a route ceases to exist;
- a target changes while the worklist is running;
- turns, credits, cargo space, or the selected inventory run out;
- a port changes its traded commodity or reaches a quantity/price boundary;
- an ordinary pause, pager, or radio message occurs; and
- file or record I/O fails partway through the list.

The result must identify completed, current, skipped, and unattempted items.
Whether pending work survives any of these events remains unspecified.

## Verification requirements

Each native workflow should be tested against an explicit ordinary-command
sequence using identical initial files and deterministic RNG:

- the final legacy data files are byte identical;
- the same turns and inventory are consumed;
- the same random draws occur in the same action-relative order;
- the same radio, news, retaliation, and combat events occur;
- interruption after every constituent action leaves the same persistent
  state;
- no report reveals information absent from the ordinary command transcript;
- remote ANSI clients receive the feature without local function-key support;
  and
- local SysOp function-key events retain their separate behavior and produce
  the correct player-visible interruption.

Read-only analyzers need fixture comparisons against the actual ZOC/Python or
spreadsheet result, followed by comparison with the recovered executable
formula. Calculators need separate fixtures for exact 3.6G and every
explicitly selected corrected ruleset.

## Open specification decisions

The following decisions are required before code is written:

- which native automation features are available in exact compatibility mode;
- command keys, menus, prompts, help text, and result formatting;
- whether automation is named workflows, worklists, a plan builder, extended
  command syntax, or a combination;
- batch interleaving and presentation pacing in concurrent games;
- default and configurable list-size or repetition limits;
- per-item confirmation and stop behavior;
- persistence of worklists, progress, discoveries, notes, and history;
- whether any new MBF sidecar is permitted by the shared-file requirement;
- the authoritative formulas for calculators under corrected rulesets; and
- which exploit-sensitive workflows remain available when a hardened ruleset
  changes the underlying action.

Until those are specified, the source scripts provide evidence of player
needs and equivalent command sequences, but not authority to invent the
native interaction.

## References

- Strategy-site mirror:
  `../../syncterm/yt.starflt.com`
- Strategy scripts and workbook:
  `../../syncterm/yt.starflt.com/yt`
- Recovered 3.6G gameplay:
  `/bbsdev/doors/yt/3.6G/docs/gameplay`
- Command queue and player input:
  `/bbsdev/doors/yt/3.6G/docs/gameplay/command-shell.md`
- Path finding, autopilot, and port-pair reports:
  `/bbsdev/doors/yt/3.6G/docs/gameplay/computer-profit.md`
- Planet and player reports:
  `/bbsdev/doors/yt/3.6G/docs/gameplay/computer-reports.md`
- Port economy:
  `/bbsdev/doors/yt/3.6G/docs/gameplay/port-economy.md`
- Local SysOp controls:
  `/bbsdev/doors/yt/3.6G/docs/runtime/sysop-controls.md`
- Exploit mitigation and corrected-ruleset boundary:
  `EXPLOIT-MITIGATIONS.md`

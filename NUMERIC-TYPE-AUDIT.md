# Floating-point state audit

This audit covers every `float` or `double` member in production named
structures after the numeric-type refactor. It classifies values by what the
shipped Yankee Trader programs can produce, not by values injected directly by
tests and not by externally edited files. The programs are the only supported
writers of their data files.

A field is eligible for a size-based integer type only when every producer is
integral and the complete reachable range has a finite bound representable by
that type. MBF32 and MBF64 remain the on-disk formats; an eligible persistent
field would still require explicit conversion at the record-codec boundary.

The disposition codes below are:

- `F`: a normal program path produces a fractional value.
- `U`: program-produced values are integral on the cited path, but valid play
  can exceed `int64_t`, so no size-based integer type covers the domain.
- `R`: the member carries or aliases a representation whose domain requires
  floating point.
- `C`: proven integral and bounded, and converted.

## Producer findings

The following witnesses decide the classifications in the inventory:

- YTCONFIG accepts decimal input through `VAL`, narrows it to MBF32, applies
  range comparisons, and stores it without `INT` or `CINT`. A maximum-holds
  input of `5.9` fits the option's input width and is accepted; the attempted
  `1000.9` witness is truncated to `1000` before `VAL` and therefore does not
  contradict this. The other documented witnesses include `123.5junk` and
  Headquarters `8.6`. See
  `../3.6G/docs/runtime/configuration-editor.md` under “Scalar editor” and
  “H: Headquarters relocation”.
- The gameplay numeric editor accepts `.`. Planet fighter transfer stores
  `S(VAL(input))` without an integral coercion, and sector-mine deployment does
  the same for every positive amount at least one. See
  `../3.6G/docs/gameplay/planet-menu.md` and
  `../3.6G/docs/runtime/main-sector-mine-output.md`.
- A fractional configured hold count makes free holds fractional. Blank
  commodity purchase selects that maximum unchanged, making a cargo quantity,
  credit debit, port receipt, and carried commodity fractional through an
  ordinary transaction.
- Port and planet updates use fractional `TIMER / 60`, fractional elapsed days,
  production-rate arithmetic, and random event factors. See
  `../3.6G/docs/gameplay/port-economy.md` and
  `../3.6G/docs/gameplay/planet-economy.md`.
- Sector-fighter input itself is floor-converted. Sector fighters nevertheless
  have a fractional producer: Mercenary planet absorption adds the planet's
  fractional fighter reserve to the destination defense. Hostile combat and
  the Mercenary scoreboard inherit that value. Xannor group sizes remain
  integral, but score-derived regeneration has no `int64_t` bound.
- Planet bank growth is integral because it applies `INT`, but it has no game
  cap. Repeated valid elapsed-time updates grow a positive balance beyond
  `INT64_MAX` while still inside MBF32. Withdrawing that balance funds the
  uncapped Earth fighter, shield, and ground-force purchases documented in
  `../3.6G/docs/gameplay/earth-store-lottery.md`. Bank-dependent planet weapon
  production likewise permits integral player missile, plasma, and
  ground-force totals beyond `INT64_MAX` after a Take All transfer.
- Navigation stores avoid-list input without `INT` or `CINT`. Route-start and
  repeat-count paths do apply integral coercion, but accept sufficiently long
  negative decimal literals to produce integral MBF values outside `int64_t`
  before their later control tests. See
  `../3.6G/docs/gameplay/navigation.md` and `src/yt_command_input.c`.
- The live time formatter constructs the text prefix from a `uint16_t whole`
  minute count. `src/yt_session_info.c` is the sole production caller that
  refreshes this text, and `session_low_time()` is the sole owner of the
  remembered value. Therefore the cache range is exactly `0..UINT16_MAX` and
  fractions supplied directly by tests are not program-reachable.

## Exhaustive member inventory

| Structure | Floating members | Disposition and producer reason |
|---|---|---|
| `commodity_trade_terms` | `displayed_hold`, `credits`, `free_holds`, `maximum` | `F`: player cargo/credits and blank-selection maximum inherit the fractional configuration/trade path. The unused decoded `selected_quantity` member was removed; the exact MBF64 raw capacity remains. |
| `hostile_persistence` | `defender_loss`, `deployed_fighters`, `ship_fighters`; `shields` | fighter fields `F` because combat starts with and caps losses to fractional player/sector fighters; shields `U`, inheriting the uncapped player shield domain. |
| `hostile_surrender` | `attacker_loss`, `defender_loss`, `deployed_fighters`, `deployed_remaining`, `ship_fighters` | `F`: all derive from fractional-capable player or deployed fighter counts, including capped loss values. |
| `hostile_tail` | `defender_loss`, `deployed_fighters`, `ship_fighters`, `headquarters`, `turns_per_day` | fighter fields `F`; configuration fields `F` through YTCONFIG. |
| `maint_state` | `player_cloak` | `R`: pointer alias to the fractional player cloak field. |
| `nearest_scan` | `base_price[3]`, `timer_seconds` | `F`: randomized market bases and the clock timer both have fractional producers. |
| `profit_report` | `base_price[3]` | `F`: copy of randomized market bases. |
| `projectile_route_state` | `origin`, `destination`, `amount` | origin `F` because Xannor retaliation aliases fractional-configurable Headquarters; destination `F` because the target editor accepts an in-range raw `VAL` without `INT`; amount `U` because its integral resource path is not bounded to `int64_t`. |
| `qb_val_result` | `value` | `R`: decoded raw `VAL` result, whose supported syntax includes fractions and magnitudes outside `int64_t`. |
| `session_combat_state` | `deployed_fighters`, `ship_fighters`; `ship_shields` | fighter fields `F`; shields `U`, mirroring the uncapped player field. |
| `session_earth_state` | `clearance_discounts[4]` | `F`: values are direct random fractions. |
| `session_navigation_state` | `avoided_sectors[30]`; `route_start_sector` | avoids `F`; route start `U` after integral `VAL` coercion without an `int64_t` magnitude bound. |
| `session_projectile_state` | `retained_counterlaunch_missiles` | `U`: the count is integral but bounded only by the player's uncapped missile total. |
| `yt_config` | `genesis_ports`, `headquarters`, `initial_credits`, `initial_fighters`, `initial_holds`, `lottery_plays`, `maximum_holds`, `retention_days`, `turns_per_day` | `F`: every numeric editor stores accepted `VAL` results without integral coercion. |
| `yt_config_menu_working` | `lottery_plays`, `maximum_holds` | `F`: working copies of fractional configuration values. |
| `yt_maintenance_planet_result` | `old_event_total`, `new_event_total`, `old_event_ground`, `new_event_ground`; `civil_war_expense` | totals/ground `F` through production and random civil-war arithmetic; expense `U`, since it is integral but derived from the uncapped bank. |
| `yt_nearest_market` | `minute`, `elapsed`, `stock[3]`, `production[3]`, `stored_minute` | `F`: clock, elapsed-time, and port-economy values. |
| `yt_planet` | `production[3]`, `stock[3]`, `fighters`, `ground_forces`, `last_minute`, `mines`, `missiles`, `plasma`; `bank` | listed economy/defense fields `F` through elapsed production, fighter transfer, or random civil-war loss; bank `U` through uncapped integral interest growth. |
| `yt_planet_economy` | `current_minute`, `elapsed`, `production[10]`, `quantity[10]`, `contribution[10]` | `F`: the arrays jointly carry fractional clock, rate, stock, defense, and contribution slots and cannot use one integral element type. |
| `yt_player` | `turns`, `fighters`, `holds`, `ore`, `organics`, `equipment`, `credits`, `score`, `cloak`, `mines`; `shields`, `missiles`, `plasma`, `ground_forces` | first group `F` through configuration, transfers, scoring, cloak, or fractional mine deployment; second group `U` through uncapped purchases/planet transfers. |
| `yt_player_cache` | `cloak[52]` | `F`: cache of fractional player cloak values. |
| `yt_port` | `stock[3]`, `production[3]`, `treasury`, `last_minute` | `F`: elapsed economy makes stock/rate/time fractional; blank fractional commodity purchases make treasury receipts fractional. |
| `yt_port_market_state` | `timer_seconds`, `base_price[3]` | `F`: fractional clock and randomized market bases. |
| `yt_projectile_damage_result` | `fighters`; `shields` | fighters `F` by capping damage to fractional fighter state; shields `U`, mirroring uncapped integral shields. |
| `yt_projectile_ground_result` | `ground` | `F`: linked-planet ground forces can be fractional after civil-war loss. |
| `yt_projectile_productivity_result` | `old_total`, `new_total` | `F`: sums of fractional planet production values. |
| `yt_repeat_transform` | `count` | `U`: `INT(VAL(...))` makes it integral, but negative input is retained before the positive-count normalization and is not bounded to `int64_t`. |
| `yt_score_player`, `yt_score_team` | `score` | `F`: score inputs include fractional player state and fractional sector fighters. |
| `yt_scoreboard` | `mercenaries`, `xannor` | Mercenaries `F` through fractional planet-fighter absorption; Xannor `U` through integral but uncapped score-derived regeneration. |
| `yt_sector` | `fighters`, `metadata`, `mines` | `F`: Mercenary planet absorption adds fractional planet fighters, Headquarters `8.6` is stored in metadata, and mine deployment accepts fractions. |
| `yt_session` | `market_bases[3]`; formerly `low_time_remembered` | market bases `F`; low-time cache `C` to `uint16_t`, covering its complete `0..65535` producer range. |

## Result

No surviving floating member satisfies both conversion requirements. The one
bounded integral state member is now `uint16_t`; the one dead decoded `double`
member is removed. Persistent record fields remain MBF32 because every one is
either fraction-capable or lacks a representable size-based integer bound, and
all existing record serialization boundaries remain unchanged.

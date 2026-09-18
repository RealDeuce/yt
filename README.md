# Yankee Trader 3.6 native reimplementation

This repository contains the C17/OpenDoors reimplementation of Yankee Trader
3.6. The runtime can reproduce pristine 3.6 or any of the six documented patch
profiles: 3.6A, 3.6C, 3.6D, 3.6E, 3.6F, and 3.6G. Compatibility evidence and
the byte-pinned 3.6G original-program analysis remain in the separate read-only
tree at `/bbsdev/doors/yt/3.6G`; the profile differences are documented in
`../PATCHED-VERSIONS.md`.

OpenDoors is a pinned Git submodule.  Clone and build with:

```sh
git clone --recurse-submodules <repository-url> yt-ng
cd yt-ng
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

For an existing checkout, initialize the dependency with:

```sh
git submodule update --init --recursive
```

The default build uses `third_party/opendoors` and links
`OpenDoors::Static`.  An exact local OpenDoors checkout can be selected with
`-DYT_OPENDOORS_SOURCE_DIR=/path/to/OpenDoors`.  An installed OpenDoors 6.3
package can instead be selected with `-DYT_USE_SYSTEM_OPENDOORS=ON`.

`GNUmakefile` is only a convenience wrapper around this CMake build.  It does
not contain a second OpenDoors integration.

## Patch profiles

`yt` and `yt-init` accept an application option through the OpenDoors command
line parser:

```sh
yt -PATCH 3.6G
yt-init -PATCH 3.6G
```

Profile names are case-insensitive but must be complete: `3.6`, `3.6A`,
`3.6C`, `3.6D`, `3.6E`, `3.6F`, or `3.6G`. The default is pristine `3.6`.
When other plain arguments are required, put them before `-PATCH`; OpenDoors
collects consecutive plain arguments following an application-defined option
as that option's value.

The selected profile controls every documented initialized-data difference,
including command keys, prices, Xannor arithmetic and awards, score weighting,
registration text, and the historical malformed string descriptors. It also
controls the universe layout created by `yt-init`: profiles 3.6 through 3.6F
use a 3,004-record sector span, while 3.6G uses 2,004. `yt` rejects a database
whose sector span does not match the selected profile; it does not migrate or
rewrite an existing universe.

# Yankee Trader 3.6G native reimplementation

This repository contains the C17/OpenDoors reimplementation of Yankee Trader
3.6G.  Compatibility evidence and the original-program analysis remain in the
separate read-only tree at `/bbsdev/doors/yt/3.6G`.

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

# Clone Command Design

## Architecture

Five layers, each building on the previous:

```
clone.c        — orchestrates init → discover refs → fetch pack → parse → checkout
checkout       — tree → working directory (reuses object_read)
packfile.c     — parse PACK binary format, resolve deltas (reuses decompress_data, object_write)
pktline.c      — git pkt-line wire format parser
http.c         — libcurl wrapper for GET refs + POST upload-pack
```

## New Files

- `src/commands/clone.c` — public clone() command
- `src/net/http.h` + `http.c` — HTTP client (libcurl)
- `src/net/pktline.h` + `pktline.c` — pkt-line parser
- `src/pack/packfile.h` + `packfile.c` — packfile parser + delta resolution

## New Dependencies

- libcurl via vcpkg (HTTP/HTTPS client)

## Implementation Steps

1. Add libcurl to vcpkg + CMake — verify compilation
2. HTTP layer — GET refs, POST upload-pack
3. Pkt-line parser — extract HEAD SHA, build want request
4. Packfile parser — header + non-delta objects
5. Delta resolution — OFS_DELTA + REF_DELTA
6. Checkout — commit → tree → working directory

## Tester Expectations

- Command: `./your_program.sh clone <url> <dir>`
- Checks: files have correct contents, commits are readable
- Test repos: github.com/codecrafters-io/git-sample-{1,2,3}

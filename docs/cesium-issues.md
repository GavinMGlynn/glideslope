# Two issues to post at https://github.com/CesiumGS/cesium-native/issues

Both were found building glideslope against Cesium Native v0.64.0 with
`-fsanitize=address,undefined -fno-sanitize-recover=all` on GCC 14.3.1
(Rocky Linux 10). Neither is glideslope-specific: any sanitized build that
streams a Cesium ion asset hits both.

**Both checked against `main` on 2026-09-21**, by reading the files on
GitHub rather than the pinned copy in `ext/`:

- `TileLoadInput::pAssetAccessor` is still declared
  `const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor;`, and
  so are its neighbours - `tile`, `contentOptions`, `asyncSystem`, `pLogger`
  and `requestHeaders` are all `const` references. The hazard is the
  struct's, not one member's.
- `QuantizedMeshLoader::readValue` still returns
  `*reinterpret_cast<const T*>(data.data() + offset);`, and it is not alone:
  `parseQuantizedMesh` reads the header and the extension fields the same
  way, and `decodeIndices` casts spans to typed pointers likewise. A fix
  wants the whole file, not the one function.

No issue matching either was found open on the tracker.

---

## Issue 1 — `TileLoadInput::pAssetAccessor` is a reference member, and `CesiumIonTilesetLoader` binds a temporary to it

**Title:** `CesiumIonTilesetLoader::loadTileContent` leaves `TileLoadInput::pAssetAccessor` dangling (stack-use-after-scope)

**Body:**

`TileLoadInput::pAssetAccessor` is declared as a reference:

```cpp
// Cesium3DTilesSelection/include/Cesium3DTilesSelection/TilesetContentLoader.h:92
const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor;
```

`CesiumIonTilesetLoader` holds a `shared_ptr` to a *derived* accessor:

```cpp
// Cesium3DTilesSelection/src/CesiumIonTilesetLoader.h:85
std::shared_ptr<CesiumAsync::CesiumIonAssetAccessor> _pIonAccessor;
```

and passes it when building the aggregated input:

```cpp
// Cesium3DTilesSelection/src/CesiumIonTilesetLoader.cpp:368
TileLoadInput aggregatedInput(
    loadInput.tile,
    loadInput.contentOptions,
    loadInput.asyncSystem,
    this->_pIonAccessor,        // shared_ptr<CesiumIonAssetAccessor>
    ...
```

Because the parameter is `const std::shared_ptr<IAssetAccessor>&` and the
argument is a `shared_ptr` to a different type, a temporary
`shared_ptr<IAssetAccessor>` is materialised for the conversion. It is bound
to the reference member and destroyed at the end of that full-expression, so
`aggregatedInput.pAssetAccessor` dangles for every later use.

The open-data path in our project passes a `shared_ptr<IAssetAccessor>`
whose type matches exactly, so no temporary is made and the bug never shows -
which is why it only appears on the ion path.

**What the sanitizer says**

```
ERROR: AddressSanitizer: stack-use-after-scope on address 0x... at pc 0x...
READ of size 8 at 0x... thread T0
    #3 requestTileContent Cesium3DTilesSelection/src/LayerJsonTerrainLoader.cpp:712
    #4 LayerJsonTerrainLoader::loadTileContent(...) LayerJsonTerrainLoader.cpp:875
    #5 CesiumIonTilesetLoader::loadTileContent(...) CesiumIonTilesetLoader.cpp:378
    #6 TilesetContentManager::loadTileContent(...) TilesetContentManager.cpp:1257
    #7 TilesetContentManager::processWorkerThreadLoadRequests(...) TilesetContentManager.cpp:1924
    #8 Tileset::loadTiles() Tileset.cpp:477
    #9 Tileset::updateViewGroupOffline(...) Tileset.cpp:335

Address ... is located in stack of thread T0 at offset 96 in frame
    CesiumIonTilesetLoader::loadTileContent(...) CesiumIonTilesetLoader.cpp:334
```

**To reproduce**

Build with `-fsanitize=address -fno-sanitize-recover=all`, construct
`Tileset(externals, /*ionAssetID=*/1, token)` for Cesium World Terrain, and
call `updateViewGroupOffline` until a tile loads.

**Suggested fix**

Make `TileLoadInput::pAssetAccessor` a value rather than a reference, or have
`CesiumIonTilesetLoader` keep a `shared_ptr<IAssetAccessor>` member alongside
the derived one and pass that. A value member is the safer of the two: the
struct's other reference members have the same hazard for any caller whose
argument needs a conversion.

---

## Issue 2 — `QuantizedMeshLoader::readValue` does misaligned loads

**Title:** `QuantizedMeshLoader::readValue` reads unaligned data through `reinterpret_cast` (undefined behaviour, caught by UBSan)

**Body:**

```cpp
// CesiumQuantizedMeshTerrain/src/QuantizedMeshLoader.cpp:166
template <class T>
T readValue(
    const std::span<const std::byte>& data,
    size_t offset,
    T defaultValue) noexcept {
  if (offset + sizeof(T) <= data.size()) {
    return *reinterpret_cast<const T*>(data.data() + offset);
  }
  return defaultValue;
}
```

`offset` is a byte offset into the tile, and the quantized-mesh format does
not align its fields, so this forms and dereferences a misaligned `const T*`.
That is undefined behaviour regardless of whether the target happens to
tolerate it, and it fires on essentially every terrain tile.

**What the sanitizer says**

```
CesiumQuantizedMeshTerrain/src/QuantizedMeshLoader.cpp:171:60: runtime error:
load of misaligned address 0x... for type 'const unsigned int',
which requires 4 byte alignment
```

With `-fno-sanitize-recover=all` this ends the process, so a sanitized build
cannot stream Cesium World Terrain at all.

**To reproduce**

Build with `-fsanitize=undefined -fno-sanitize-recover=all` and load any
quantized-mesh terrain tile.

**Suggested fix**

```cpp
T value;
std::memcpy(&value, data.data() + offset, sizeof(T));
return value;
```

which compiles to the same instruction on targets that allow unaligned access
and is well defined everywhere. **The same pattern appears throughout the
file** - `parseQuantizedMesh` reads `QuantizedMeshHeader`, `extensionID` and
`extensionLength` this way, and `decodeIndices` casts spans to typed pointers
- so this wants a sweep rather than a one-line change.

---

## Issue 3 — `SqliteCache` sets no busy timeout, so a second process writing the cache fails at once

**Title:** `SqliteCache` never calls `sqlite3_busy_timeout`, so concurrent writers get SQLITE_BUSY immediately

**Body:**

`SqliteCache` opens its database and turns on WAL:

```cpp
// CesiumAsync/src/SqliteCache.cpp:58, 214
const std::string PRAGMA_WAL_SQL = "PRAGMA journal_mode=WAL";
```

WAL lets readers and a writer work at once, which is the hard part, and it is
good that it is there. But the connection is opened with
`sqlite3_open` and **nothing ever sets a busy timeout** - there is no call to
`sqlite3_busy_timeout` or `sqlite3_busy_handler` anywhere in the file. SQLite's
default timeout is zero, so a second *writer* - a second process sharing the
cache, which WAL does not serialise - gets `SQLITE_BUSY` on its first attempt
rather than waiting, and the store fails.

**What it looks like**

Several processes sharing one cache file log, repeatedly:

```
[error] [SqliteCache.cpp:590] database is locked
[error] [SqliteCache.cpp:456] database is locked
```

Nothing is lost but the caching - the entry is simply not stored and the asset
is fetched again next time - so it is easy to miss. It costs bandwidth and
time in proportion to how much is being streamed.

**To reproduce**

Run two processes that stream the same tileset with the same
`SqliteCache` database name, and watch the log.

**Suggested fix**

After `sqlite3_open` succeeds:

```cpp
CESIUM_SQLITE(sqlite3_busy_timeout)(pConnection, 5000); // milliseconds
```

A few seconds is generous for a cache write and turns a failure into a short
wait. A caller who wants a different figure could be given one, but a default
of zero is the one value that cannot be right.


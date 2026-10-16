### Cesium ion's layer.json is read on Windows, and the weather still arrives, 2026-10-01 — tail done

**The cause was the kind of deflate, not the send and not the read loop.**
WinHTTP hands a compressed body up as it came unless
`WINHTTP_OPTION_DECOMPRESSION` is on, and ion's CDN serves `layer.json` with
`Content-Encoding: gzip` whatever the request says. The two attempts of
2026-09-21 and 22 turned the option on with `WINHTTP_DECOMPRESSION_FLAG_ALL`,
which makes WinHTTP offer `gzip, deflate`. Offered both, Open-Meteo prefers
deflate, and sends it in the zlib format (RFC 1950) - which is what HTTP's
"deflate" means. WinHTTP's deflate decoder reads only raw deflate (RFC 1951),
so every read of that body fails with **E_ABORT, 0x80004004** - the
2147500036 the first attempt recorded and then called a red herring. It was
the failure.

**The evidence**, from a probe of WinHTTP (not committed) on the Windows
development machine, fetching the forecast URL `open_meteo_url` makes:

| WinHTTP set up as | Open-Meteo answered | the read |
|---|---|---|
| no decompression | identity, 16,478 bytes | the JSON |
| `FLAG_ALL`, on the session or on the request | deflate | E_ABORT, all 8 fetches that reached the body |
| `FLAG_DEFLATE` alone | deflate | E_ABORT |
| `FLAG_GZIP` alone | gzip | the JSON |
| no decompression, `Accept-Encoding: gzip, deflate` by hand | deflate, `78 9c...` | 4,652 compressed bytes, which Python's zlib reads whole, Adler-32 and all |

A server on the loopback serving one JSON body several ways, to WinHTTP with
`FLAG_ALL`, settled what the decoder refuses: zlib format at one go, E_ABORT;
zlib format flushed in 4 KiB pieces, E_ABORT; Open-Meteo's own recorded bytes,
E_ABORT; raw deflate, read; gzip flushed in pieces, read. And ion's
`layer.json` (asset 1): with no decompression, 128,383 bytes beginning
`1f 8b`, `Content-Encoding: gzip`, not asked for; with `FLAG_GZIP`, 920,353
bytes of JSON.

**The 12002 at `WinHttpSendRequest` that the second attempt recorded is
explained only in part.** That error was the last of three tries
(`fetch_with_retries`), so the first try's failure was never printed, and on
the development machine every try with `FLAG_ALL` that reached the body
failed at the read. The development machine also sometimes waits 120 s and
fails the send with 12002 whether decompression is on or not (a tail), but a
CI test that took 16.5 s in all cannot have waited that long, so what the
third try met on CI is still not known. The fix does not depend on it.

**The fix**: `http_winhttp.cpp` turns the option on with
`WINHTTP_DECOMPRESSION_FLAG_GZIP` only. WinHTTP then offers `gzip` itself,
undoes it, and takes `content-encoding` and `content-length` off - the layer
puts back a length that counts the body handed up, as it did. A body in any
other encoding, or any body before Windows 8.1, where the option cannot be
set, keeps its `content-encoding`, which is what tells a caller (`http.hpp`
says so). The Windows 8.1 case cannot arise for the client, which needs
Windows 10 for SDL_GPU; the server and CLI are not tested on anything older
either - CI's Windows runners are Windows Server. libcurl and NSURLSession are
unchanged: both undo zlib-format deflate.

**Tests**, in `tests/unit/test_http.cpp`:

- `a_body_served_gzipped_arrives_as_it_was_before_it_was_gzipped` - a
  loopback server answers gzip whatever it was offered, as ion does: the body
  is the original, with no `content-encoding` and a `content-length` that
  counts it. No network, so CI runs it on every platform.
- `a_server_that_prefers_deflate_when_offered_it_is_still_read` - a loopback
  server answers as Open-Meteo does: zlib-format deflate when deflate is
  offered, gzip when only gzip is, nothing compressed otherwise. Whatever the
  client offers it must read. The canned bodies are 105 and 93 bytes made by
  Python's `gzip` and `zlib`. The loopback server (`OneRequestServer`) can now
  make its answer from the request's head.
- `cesium_ions_layer_json_is_fetched_and_read` - with the user's token, asks
  ion where asset 1 is and fetches its `layer.json` with the bearer token ion
  gives: status 200, no `content-encoding`, not gzip's magic, a true
  `content-length`, and JSON with `format` quantized-mesh-1.0, its `tiles`
  and more than ten levels of `available`. **Skipped without a token**, and
  no CI job has one, so CI skips it; it prints neither the URL nor an error
  that could hold the token.

**Verification.** Windows debug on the development machine
(`tools/windows_build.sh`, the token passed in through `WSLENV`): all three
pass - offered `gzip`, "fetched 920353 bytes of layer.json through WinHTTP" -
and so do `the_weather_now_at_an_airfield_is_fetched_from_aviationweather_and_open_meteo`
(it took 124 s on one run, a 120 s send timeout and then a retry - a tail
of its own) and the other HTTP tests. Linux release: the same three pass, libcurl
offering `deflate, gzip, br` and fetching the same 920,353 bytes, and the
weather test passes. **Seen to fail on Windows**: with `FLAG_ALL` back, the
deflate test fails - "offered "gzip, deflate", the answer could not be read:
... the transfer failed (WinHTTP error 2147500036)" - and the weather test
cannot fetch the forecast (it skips here, the network not being required; CI
requires it, which is how it failed there); with the option not set, the
gzip test fails ("not 105 bytes beginning with byte 31") and the ion test
fails ("no content-encoding left on it: gzip"). Each change was reverted.
**On CI** (run 36841724447, this pull request, where the network is
required): the weather test fetched the forecast and passed on all three
Windows jobs - debug, release and clang-cl - as did the two loopback tests on
every platform, macOS included; the ion test skipped everywhere, no job having
a token.

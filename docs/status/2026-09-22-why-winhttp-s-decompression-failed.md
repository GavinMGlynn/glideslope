### Why WinHTTP's decompression failed, 2026-09-22

**The first attempt turned the option on, watched every Open-Meteo fetch
fail on all three Windows jobs, turned it off, and wrote "why is not known"
beside it.** The reason was in Microsoft's documentation, and reading it
rather than guessing again found it.

**The body was read by asking how much was there.**
`WinHttpQueryDataAvailable` was called, and the loop stopped when it answered
none. Its own documentation says not to do that: *"Do not use the return
value of WinHttpQueryDataAvailable to determine whether the end of a response
has been reached, because not all servers terminate responses properly."* It
is also not the decompressed length when WinHTTP is undoing an encoding, so
turning decompression on was the thing that exposed a loop that was already
wrong. The documented pattern is fixed chunks - it advises 8 KiB or more -
read until a read returns no bytes, which is the end of a body as
end-of-file is the end of a file. That is what it does now, and it is right
whether anything is being decompressed or not.

**And `WinHttpSetOption`'s result is checked**, which it was not before. An
option that did not take is an option that quietly does nothing, and the body
would arrive compressed with nothing to say so; where it cannot be set -
it wants Windows 8.1 - `content-encoding` is left on the response, which is
what tells a caller the bytes are not what they look like.

**The number in the old note was a red herring.** "WinHTTP saying
2147500036" is 0x80004004, E_ABORT, which is not in WinHTTP's error range at
all.

**And CI said the diagnosis was wrong.** Turned on a second time with the
loop fixed, every Open-Meteo fetch failed again on all three Windows jobs -
but this time with a real WinHTTP code instead of a stale one: **12002,
`ERROR_WINHTTP_TIMEOUT`, at `WinHttpSendRequest`**. That is raised before a
single byte of the body is read, which is earlier than the read loop runs, so
the loop was never the cause. Asking for a compressed body from that host,
from those runners, times out the send; why is still not known.

The option is off again - a red tree is the stop-everything condition - and
the read-loop fix stays, because it is right on its own account whatever
WinHTTP is asked to decompress. What has been gained is a symptom worth
having: a known code at a known call, instead of "2147500036, why is not
known". The next attempt starts from the send, not the read.

**What CI can and cannot answer.** The weather half of the tail's
verification is exactly what failed, and `tests/unit/test_weather.cpp`
fetches Open-Meteo on the Windows jobs, so CI is real evidence for it - it is
what produced this diagnosis. The ion half needs a token no CI job has.

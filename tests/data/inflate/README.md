# DEFLATE decoder fixtures

Made by `tools/make_inflate_fixtures.py` with zlib 1.3.1.zlib-ng. Each is a zlib stream of the payload the
script's generator makes from the seed; `tests/unit/test_inflate.cpp` makes
the same payload and compares.

| File | Payload bytes | Seed | Stream bytes |
| --- | --- | --- | --- |
| `far_matches.zz` | 40000 | 10 | 32164 |
| `empty.zz` | 0 | 1 | 8 |
| `fixed.zz` | 20000 | 2 | 3174 |
| `dynamic.zz` | 120000 | 3 | 15907 |
| `fast.zz` | 50000 | 4 | 9279 |
| `huffman_only.zz` | 6000 | 5 | 3245 |
| `runs.zz` | 20000 | 6 | 10630 |
| `stored.zz` | 3000 | 7 | 3011 |
| `sync_flushes.zz` | 30000 | 8 | 4579 |
| `full_flushes.zz` | 30000 | 9 | 4816 |

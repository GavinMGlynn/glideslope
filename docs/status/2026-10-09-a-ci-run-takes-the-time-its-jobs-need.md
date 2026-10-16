### A CI run takes the time its jobs need, 2026-10-09 — item done

PR #150's own run (37933227320, head c22a5821), with macOS release only on
pull requests and vcpkg fetched from the runner's clone, took 46 minutes from
push to result (12:55 to 13:41 UTC). Every build restored main's ccache: macOS
release 504/567 hits (88.9%), Ubuntu debug and release 501/565 (88.7%),
Windows debug, release and clang 756-760/831 (91.0-91.5%), Rocky 896/1134
(79.0%). The other three CI items stay open, each owed what its text names.

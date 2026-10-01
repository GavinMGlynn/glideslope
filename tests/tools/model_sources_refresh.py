"""model_sources_refresh.py - tools/make_models.py --refresh, against a stand-in network.

Each case is built explicitly: a source that answers 429 or 503, one that
serves an outage page in place of a pinned file, and none that serves it.
Nothing here reaches the network.

    python3 model_sources_refresh.py <path to tools/make_models.py>
"""
import hashlib, importlib.util, io, pathlib, sys, tempfile, urllib.error

spec = importlib.util.spec_from_file_location("mm", sys.argv[1])
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

GOOD = b"the pinned bytes"
PAGE = b"<html>Disaster Recovery mode</html>"
SHA = hashlib.sha256(GOOD).hexdigest()

def http(url, code):
    return urllib.error.HTTPError(url, code, "x", {}, io.BytesIO())

answers = {}
def fake(url):
    a = answers.get(url.split("/")[2], 404)
    if isinstance(a, int):
        raise http(url, a)
    return a
m.download = fake

CASES = 4
checked = []
failures = []
def check(what, ok):
    print(("ok   " if ok else "FAIL ") + what)
    checked.append(what)
    if not ok:
        failures.append(what)

# 1. Software Heritage's 429 is not a missing file.
answers = {"archive.softwareheritage.org": 429}
try:
    m.serves(f"{m.HERITAGE}{SHA}/")
    check("a 429 from Software Heritage stops the refresh", False)
except urllib.error.HTTPError:
    check("a 429 from Software Heritage is not an HTTPError, which the walk "
          "takes for a missing file", False)
except Exception as e:
    check("a 429 from Software Heritage stops the refresh, naming it",
          "429" in str(e) and "softwareheritage" in str(e))

# 2. Files.get tries the next candidate path only on a 404.
with tempfile.TemporaryDirectory() as d:
    files = m.Files(pathlib.Path(d), refresh=True)
    files.pinned = {}
    calls = []
    def one(key, spec, path):
        calls.append(path)
        raise http("https://svn.code.sf.net/x", 503)
    files._one = one
    try:
        files.get("k", {"dir": "D"}, "Models/a.xml")
        check("a 503 is raised", False)
    except Exception:
        pass
    check("a 503 is not taken as a missing path: one candidate tried, not two",
          calls == ["Models/a.xml"])

# 3. A refresh never caches a source's page in place of a pinned file.
with tempfile.TemporaryDirectory() as d:
    files = m.Files(pathlib.Path(d), refresh=True)
    name = m.pinned_name("k", "Models/a.xml")
    release = f"{m.RELEASE}{name}"
    svn, web = m.urls_for({"dir": "D"}, "Models/a.xml")
    files.pinned = {name: (len(GOOD), SHA, [svn, release, web])}
    answers = {"svn.code.sf.net": 503, "sourceforge.net": PAGE,
               "github.com": GOOD, "archive.softwareheritage.org": 404}
    try:
        data = files._one("k", {"dir": "D"}, "Models/a.xml")
    except Exception as e:
        data = repr(e).encode()
    cached = (pathlib.Path(d) / name).read_bytes() if (pathlib.Path(d) / name).exists() else None
    check("with the Subversion server down, the web view's page is passed over "
          "and the pinned bytes come from the release", data == GOOD and cached == GOOD)

    (pathlib.Path(d) / name).unlink(missing_ok=True)
    answers = {"svn.code.sf.net": 503, "sourceforge.net": PAGE,
               "github.com": 503, "archive.softwareheritage.org": 404}
    try:
        files._one("k", {"dir": "D"}, "Models/a.xml")
        raised = False
    except Exception:
        raised = True
    check("with no source serving the pinned bytes, the refresh fails and "
          "caches nothing", raised and not (pathlib.Path(d) / name).exists())

print(f"{len(checked)} of {CASES} cases checked, {len(failures)} failed")
sys.exit(1 if failures or len(checked) != CASES else 0)

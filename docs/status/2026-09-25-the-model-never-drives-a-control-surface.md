### The model never drives a control surface, 2026-09-25 — item done

**What is missing first.**
- **The verification changed.** The plan's verification said "checked at
  configure time". The first attempt was a configure-time reading of the
  source, and review got past it 14 ways. What holds it now is the compiler,
  the preprocessor's own list of what it opened, and the linker's symbols,
  checked as the copilot is built. The plan's verification now says so.
- **It stops the copilot growing a way to a control by accident**, not
  code set on getting round it: a pointer to the aircraft handed in by a
  caller and cast, say, which review is for. Symbols of a name the check does
  not know the shape of are not seen, and nor is a type use inlined away.
- **`FlightPlan::start` places an aircraft** in the air, where the plan
  says. That is state, not a control, and it is by design: `fly-plan` starts
  there.

**What works.** Three walls (`cmake/Copilot.cmake`):
1. **The copilot is compiled from a staged copy.** Its own sources sit beside
   only the headers it may see: the plan (`sim/plan.hpp`), JSON, the runways
   file and an HTTP request. There is no other include directory.
   - The plan's types now live in `sim/plan.hpp`, built as `glideslope_plan`.
     That header includes nothing of the aircraft. `navigator.hpp`,
     `autopilot.hpp` and `lander.hpp` take them from it.
   - The copilot links `glideslope_plan` and `glideslope_platform`, and takes
     neither's include directory.
   - Each copilot header is compiled on its own in the stage as well, since
     the programs that use one compile it seeing all of `src/`.
   - A control is declared nowhere it can see, so naming one does not
     compile, however it is spelled.
2. **What it opens is what it is shown.**
   - After the build, every staged source is preprocessed again. Every file
     the compiler lists as opened (`-M`, or `/showIncludes`) must be in the
     stage or outside the source and build trees, whatever the spelling that
     opened it. It opens 1,244 files, none past the stage.
   - At configure time, its sources are refused if they hold anything that
     could compile one way in the stage and another where a program includes
     it, or reach past the stage: a preprocessor conditional, `__has_include`,
     `#pragma include_alias` or `push_macro`, an include that is not a plain
     relative path, a line splice, a digraph or trigraph, or a file other
     than a `.cpp` or `.hpp`. The rules see an include behind a comment or a
     byte-order mark.
3. **Its symbols are checked as it is built**, with `nm -C`, or `dumpbin
   /SYMBOLS` on Windows. Every name of glideslope's in what it defines or
   calls must be the copilot's own or on the list, and nothing of JSBSim's
   may appear. A simulation function declared by hand is refused, even one
   called by its mangled name, and so is anything the copilot defines in
   another part's namespace. A toolchain with neither tool refuses the
   configure. The real library has 264 such names, all allowed.

**Verified.** `the_copilot_can_see_include_and_call_nothing_that_drives_the_aircraft`
walks 172 cases:
- **Source rules:** 29 ways refused, each naming the file, and a tree of near
  misses accepted.
- **Visibility:** it compiles with the copilot target's own include path,
  and requires that path to be the stage alone.
  - A file naming everything allowed compiles.
  - Every other header under `src/` fails to include.
  - Every class, struct and enum of the simulation's other than the plan's
    fails to be named.
  - The first review's spellings fail.
- **Opening:** the check refuses a source that opened a header of the source
  tree by an absolute path, and accepts one that opened only the stage. Each
  copilot header has its own unit.
- **Symbols:** five forbidden objects are refused, each for its reason, and an
  allowed one is accepted. These run with `dumpbin` on Windows, where the call
  by a mangled name is not walked: MSVC's mangled names hold `?` and `@`,
  which no identifier can, so it cannot be written there.

**Seen to fail**, each put back:
- the copilot linking the simulation, which gave it all of `src/`;
- the symbol check accepting everything;
- `..` allowed in an include;
- no unit made for a header;
- the opening check blind to the source tree;
- JSBSim's names allowed.

The JSBSim case at first passed for the wrong reason: its probe named nothing
of glideslope's, and was refused as "nm read nothing". Every symbol case now
has to be refused for the reason it is there for. The line splice was at
first never tested at all: the test's list swallowed its backslash. It now
holds a real splice.

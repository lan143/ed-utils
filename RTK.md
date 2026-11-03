# RTK.md

`rtk` is an output-condensing proxy for shell commands, mandated by the global
`~/.claude/RTK.md` policy. Prefix repo commands with `rtk` and treat its
condensed output as the complete output. Batch related commands to save turns.

## Cheatsheet

```sh
# Browse
rtk ls
rtk tree src
rtk find . -name "*.h"

# Read files
rtk read src/Json.cpp
rtk read src/log/log.h

# Search
rtk rg 'EDUtils::' src
rtk grep 'EDUtils::' src
rtk ast-grep 'StateProducer' src

# Git
rtk git status
rtk git log --oneline -10
rtk git diff
rtk git branch
rtk diff                      # review working-tree changes

# Misc
rtk wc -l src/*.cpp
rtk json library.json

# Compile gate (the only validation in this repo)
rtk err pio run
```

## Notes specific to this repo

- There is no test suite and no linter: do **not** use `rtk test` or `rtk lint`.
  The compile check (`rtk err pio run`) is the gate.
- Build from a throwaway consumer project outside this repo with board `esp32dev`
  and `lib_deps = file:///absolute/path/to/ed-utils` + `ArduinoJson@^6`.
  See `AGENTS.md` for the full recipe.
- `rtk json library.json` is handy for reading the PlatformIO manifest.

## Recovery

If `rtk` output is empty, truncated, or garbled:

```sh
rtk recall <hash>             # re-print a condensed result from its hash
rtk proxy <cmd>               # run the raw command without condensing
```

Fall back to `rtk proxy` only when `recall` does not recover the output.

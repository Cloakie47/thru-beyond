# `thru dev toolchain install` fails on Windows: "Failed to detect OS: program not found"

## Environment

- OS: Windows 11 Home (build 10.0.26200), x64
- thru CLI: 0.3.2 (installed via `npm i -g thru`; `thru.exe` runs fine otherwise)
- Shell: PowerShell 5.1

## Steps to reproduce

1. On a Windows machine, install the CLI: `npm i -g thru`
2. Verify it works: `thru --help` (prints normal help)
3. Run: `thru dev toolchain install`

## Expected

Either the toolchain installs, or a clear message that Windows is not a
supported platform for the toolchain (with a pointer to WSL2).

## Actual

```
Fetching latest release...
Installing toolchain version: v0.3.2
Error: Failed to detect OS: program not found
```

The platform detection appears to shell out to `uname`, which does not exist
on Windows, so detection fails before any platform check can run. The same
command inside WSL2 Ubuntu on the same machine works and prints
`Detected platform: Linux-x86_64`.

## Notes

- The GitHub release for v0.3.2 ships a `thru-cli-Windows-x86_64` archive, so
  the CLI itself is Windows-supported — which makes the cryptic failure of the
  `dev` subcommand more surprising.
- If Windows toolchain support is not planned, an explicit
  "unsupported platform: use WSL2" error would save users significant time.

*Filed as https://github.com/Unto-Labs/thru/issues/34*

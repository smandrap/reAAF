# Changelog

## [0.3.1] - 2026-03-25

### Fix:

- Use original file path if no usable path is found in AAF essence, to have opportunity to relink the media.

### Docs:

- Add info about aaf format support across daws. (on GitHub docs folder)

### Tests:

- Support automated tests against LibAAF supplied AAF files and aaftool output (can be improved, but can verify
  regressions)

### Other:

- Huge under the hood changes.

## [0.3.0] - 2026-03-24

- Add preference to fully zoom project after AAF import

## [0.2.0] - 2026-03-23

- Logger Window
- A bunch of other non user facing stuff

## [0.2.0-beta4] - 2026-03-23

- Minor refactor
- CI updates

## [0.2.0-beta3] - 2026-03-22

- Internal refactor: changed some dumb ownership models
- Fix log window stack overflow crash on Windows (oopsie)

## [0.2.0-beta2] - 2026-03-19

- Remove custom color logic from import log dialog, it's still broken.

## [0.2.0-beta1] - 2026-03-19

- Add import log
- Add preference to control when/how the log dialog opens (always, on warnings/errors, never)

## [0.1.2] - 2026-03-14

- fix possible crash in case libaaf returns nullptr in clips and timecode structs

## [0.1.1] - 2026-03-14

- Very minor fixes and cleanups

## [0.1.0] - 2026-03-14

- Initial release

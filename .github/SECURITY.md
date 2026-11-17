# Security policy

## Reporting a vulnerability

Please report security problems privately, not in a public issue.

Use GitHub's private reporting: on this repository's **Security** tab, choose
**Report a vulnerability**. Only the maintainers can see the report, and we
can discuss it and work on a fix there before anything is made public.

Include what you can of:

- what the problem is and what an attacker could do with it,
- the macXenos version (Preferences > About shows it, with its build number)
  and your macOS version,
- steps or a file that reproduce it.

We aim to acknowledge a report within a week. When a fix ships, we credit the
reporter in the release notes unless you'd rather not be named.

## Supported versions

Security fixes go into the latest release. The app updates itself, so please
make sure you are on the newest version (Preferences > System > Software
update) before reporting.

## What is in scope

- **The macXenos app:** the emulator, the Blades dashboard, the native macOS
  code, and how it handles files it opens (game images, content packages,
  saves and settings).
- **The update mechanism:** updates are delivered through Sparkle from
  `https://macxenos.com/appcast.xml`, signed with our EdDSA key, and the app
  itself is signed and notarized by Apple. A way to make macXenos install an
  update we did not sign, or to bypass those checks, is in scope.
- **The build and release tooling** in this repository.

## What is out of scope

- **Problems inherited from Xenia Canary** that are not specific to the Mac.
  Please report those to [Xenia Canary](https://github.com/xenia-canary/xenia-canary)
  as well; we pick up their fixes.
- **Code inside a game.** macXenos runs the game code you give it. A crafted
  game image or content package that misbehaves inside the emulated console is
  expected; one that escapes the emulator and affects your Mac is in scope.
- Bugs with no security impact. Please file those as normal issues.
- Requests about obtaining games or bypassing copy protection, which this
  project does not support.

Thank you for helping keep macXenos and its users safe.

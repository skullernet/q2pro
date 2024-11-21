# Q2JUMP-PRO

This is a remake of the [q2pro-jump](https://github.com/TotallyMehis/q2pro-jump)
client for Quake 2 with some additional features and improvements specifically
for playing the [q2jump](http://q2jump.net) mod. The q2pro-jump was forked
from the [q2pro-speed](https://github.com/kugelrund/q2pro-speed), a custom client
focused on single-player speed runs, which itself was forked from
[q2pro](https://github.com/skullernet/q2pro), an enhanced multiplayer-oriented
Quake 2 client and server. While q2pro is still actively maintained, the
q2pro-jump and q2pro-speed clients have been unmaintained for some time and
thus have become out of date and hard to keep synchronised with the latest
upstream q2pro changes.

The main goal of this project is to reimplement the essential q2pro-jump 
features and try to keep it in sync with q2pro.

## Current status

### Implemented features

- Added [strafe_helper](https://github.com/kugelrund/strafe_helper) acceleration
  HUD
- Added the dynamic colorization of `cl_ups` when using
  `draw cl_ups <x> <y> dynamic`
- Added the additional console auto-completions for commands available in the
  q2jump mod.

These have been almost directly copied from the q2pro-jump client, with some
minor modifications to make them work with the latest q2pro codebase.

### Missing features

- Server code is currently vanilla q2pro, not q2jump.
- An introduction to the q2jump mod with a tutorial and video links would be
  nice.

## Installation

Extract the contents of the archive to your Quake 2 directory. You need the
original Quake 2 game files to play. The binaries are not signed so you must
allow them to run in your operating system.

## Building

Follow the instructions in the
[q2pro INSTALL.md](https://github.com/skullernet/q2pro/blob/master/INSTALL.md)

It should be possible to compile a version for at least Windows, Linux, and
macOS.

## Contributing

If you want to contribute, please fork this repository and create a pull
request.

Try to keep your commits small and focused on a single change. This makes
it less likely to cause merge conflicts when the upstream q2pro is updated.
If your editor reformats the whole file when saving, please disable this
feature as it makes merges not apply cleanly.

## License

This project is licensed under the GPL-2.0 license. See the LICENSE file for
details. No warranty is provided and no rights are reserved for the changes
introduced in this project. You are free to use them in any way you like as
long as you comply with the upstream q2pro license.

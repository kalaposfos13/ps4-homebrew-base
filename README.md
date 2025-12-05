# ps4-homebrew-base

My custom repo for a PS4 homebrew template, and some of my homebrew, each as their own branches.

## Building

Non-Linux: Follow the Linux building tutorial, and it will either work or not. ¯\\_(ツ)_/¯

Linux:
- Get some build tools, you'll figure it out hopefully. The default compiler is clang, but in theory other compilers can work too.
- Get the toolchain-llvm-18 (older llvm versions probably work too) artifact from the latest release/action from https://github.com/ps4emulation/OpenOrbis-PS4-Toolchain/
- Unzip it, and set the OO_PS4_TOOLCHAIN environment variable to the project root
- Run `make`

## Homebrew

- AvPlayer Example Homebrew: A program to test media playback and its emulation on various emulators, by using libSceAvPlayer.
- Eboot Hook Test: A program to test hooking into eboot functions from an external prx library. [(Source for the hook part)](https://github.com/kalaposfos13/eboot-hooks-prx)
- Uptime Printer: A small utility tool to print the console's uptime (time spent in rest mode included).
- eBootloader: A homebrew that launches other homebrew from /data/homebrew/eboot.bin, with arguments from /data/homebrew/args.txt.


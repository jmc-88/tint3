# tint3 panel [![Build Status](https://travis-ci.org/jmc-88/tint3.svg?branch=master)](https://travis-ci.org/jmc-88/tint3) [![Coverage Status](https://coveralls.io/repos/github/jmc-88/tint3/badge.svg?branch=master)](https://coveralls.io/github/jmc-88/tint3?branch=master)

This project aims to continue the development of tint2, port it to C++, make it safer against crashes, and have it use XCB instead of Xlib.

## How is this different from tint2?

It shouldn't be much different from tint2 from a user's point of view at this stage. The focus is mostly on having a cleaner, more robust codebase.

## How stable is it?

Pretty stable. I use it daily and I haven't seen issues in a while, but if you happen to find any, please [file a bug](https://github.com/jmc-88/tint3/issues).

## Is it packaged for my Linux distribution?

 * There's a user-contributed package for Arch Linux in AUR,
 [tint3-cpp-git](https://aur.archlinux.org/packages/tint3-cpp-git).
 * You can easily obtain a `.deb` or `.rpm` package through
 [CPack](https://cmake.org/Wiki/CMake:Packaging_With_CPack). Check the build
 instructions at [README.source.md](README.source.md) for more info.
 * Other distributions: please build and install it manually.

## How do I build it?

Please refer to [README.source.md](README.source.md).

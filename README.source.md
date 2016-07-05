# Git hooks

Git doesn't allow for automatically enabled hook scripts.
Because of this, you'll need to manually do the following:

  - remove the default `.git/hooks`, which only contains example scripts:
    ```rm -rf .git/hooks```
  - symlink the `hooks` directory in:
    ```ln -s ../hooks .git/hooks```

The `pre-commit` script uses
[clang-format](http://clang.llvm.org/docs/ClangFormat.html), so make sure you
have that installed.

# Dependencies

  - cairo (with X support)
  - imlib2 (with X support)
  - pango
  - glib2
  - libX11
  - libXinerama
  - libXrandr
  - libXrender
  - libXcomposite
  - libXdamage
  - startup-notification (optional)
  - libc++ (optional, recommended)

## Debian/Ubuntu systems

You can install all needed compile-time dependencies with the following command:

```
$ sudo apt-get install libcairo2-dev libpango1.0-dev libglib2.0-dev \
                       libimlib2-dev libgtk2.0-dev libxinerama-dev libx11-dev \
                       libxdamage-dev libxcomposite-dev libxrender-dev \
                       libxrandr-dev librsvg2-dev libstartup-notification0-dev \
                       libc++-dev
```

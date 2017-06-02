# Git hooks

Git doesn't allow for automatically enabled hook scripts.
Because of this, you'll need to manually do the following:

-   remove the default `.git/hooks`, which only contains example scripts:
    ```rm -rf .git/hooks```
-   symlink the `hooks` directory in:
    ```ln -s ../hooks .git/hooks```

The `pre-commit` script uses
[clang-format](http://clang.llvm.org/docs/ClangFormat.html), so make sure you
have that installed.

> Note: the `pre-commit` script runs `clang-format` over the entire file and
> adds it back to the Git index -- this breaks a partial file staging workflow
> (e.g., `git add -p`).

# Dependencies

-   cairo (with X support)
-   imlib2 (with X support)
-   pango
-   glib2
-   libX11
-   libXinerama
-   libXrandr
-   libXrender
-   libXcomposite
-   libXdamage
-   xvfb (needed for running some tests)
-   xauth (needed for running some tests)
-   startup-notification (optional)
-   libc++ (optional, recommended)
-   pandoc (optional, recommended, needed for building documentation)

## Debian/Ubuntu systems

You can install all needed compile-time dependencies with the following command:

```
$ sudo apt-get install libcairo2-dev libpango1.0-dev libglib2.0-dev \
                       libimlib2-dev libxinerama-dev libx11-dev libxdamage-dev \
                       libxcomposite-dev libxrender-dev libxrandr-dev \
                       librsvg2-dev libstartup-notification0-dev libc++-dev \
                       pandoc xvfb xauth
```

You can now build a Debian package as follows:

```
$ mkdir package-build && cd package-build
$ cmake -DCMAKE_BUILD_TYPE=Release ../
$ cd ../ && cpack -G DEB
```

## Red Hat systems

You can now build a Red Hat package as follows:

```
$ mkdir package-build && cd package-build
$ cmake -DCMAKE_BUILD_TYPE=Release ../
$ cd ../ && cpack -G RPM
```

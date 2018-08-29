% TINT3(1) v0.3.0 | tint3 user manual

# NAME

**tint3** - lightweight panel/taskbar

# SYNOPSIS

| **tint3** \[**-c** config-file] \[**-h** or **--help**] \[**-v** or **--version**]

| **tint3** **theme** ... \
|   \[**search** *queries*] \[**s** *queries*] \
|   \[**install** *queries*] \[**in** *queries*] \
|   \[**uninstall** *queries*] \[**rm** *queries*] \
|   \[**list-local**] \[**ls**]

# DESCRIPTION

This manual page documents briefly the `tint3` command.
See the README at <https://github.com/jmc-88/tint3> for the most up-to-date
information.

tint3 is a simple panel/taskbar derived from tint2.
Initially written for Openbox, it should work fine with any modern window
manager. It provides support for launchers, taskbars, battery indicator, clock,
system tray, transparency and gradients.

# OPTIONS

-c *config-file*

:   Specify which configuration file to use instead of the default.

    If this flag is not provided and *$XDG_CONFIG_HOME/tint3/tint3rc* is
    missing, tint3 will copy its default configuration file to that path.

    See **tint3rc(5)** for a description of the configuration file format.

-h, --help

:   Shows a brief help screen and exits immediately.

-v, --version

:   Shows the version info and exits immediately.

theme *operation*

:   Acts as a theme manager, performing the requested *operation*. This can
    be any of:

    - **set**: expects one or more arguments, used to match
      against *locally* available themes based on their author name or theme
      name, or alternatively their full `author_name/theme_name` identifier.
      The resulting query joins these arguments through an OR operator.
      The resulting query needs to match with exactly one theme, which will be
      set as the one in use by unlinking *$XDG_CONFIG_HOME/tint3/tint3rc* and
      symlinking it to the local file name of the matching theme.
    - **search** or **s**: accepts any number of arguments, used to match against
      *remotely* available themes based on their author name or theme name.
      The resulting query joins these arguments through an AND operator.
      All matching themes will be listed on the standard output.
    - **install** or **in**: expects one or more arguments, used to match
      against *locally* available themes based on their author name or theme
      name, or alternatively their full `author_name/theme_name` identifier.
      The resulting query joins these arguments through an OR operator.
      All matching themes will be installed to the local repository.
    - **uninstall** or **rm**: expects one or more arguments, used to match
      against *locally* available themes based on their author name or theme
      name, or alternatively their full `author_name/theme_name` identifier.
      The resulting query joins these arguments through an OR operator.
      All matching themes, after receiving explicit confirmation from the user,
      will be uninstalled from the local repository.
    - **list-local** or **ls**: expects no arguments, and ignores them if any
      are provided. All *locally* available themes will be listed on the
      standard output.

# FILES

*$XDG_CONFIG_HOME/tint3/tint3rc*

:   Per-user configuration file.

*$XDG_CONFIG_DIRS/tint3/tint3rc*

:   System-wide configuration file. Only loaded if no per-user one is found.

# ENVIRONMENT

*XDG_CONFIG_HOME*

:   The value of this variable influences the lookup of the per-user
    configuration file. Typically defaults to *~/.config*.

*XDG_DATA_HOME*

:   The value of this variable influences the lookup of `.desktop` files for
    launcher items. Typically defaults to *~/.local/share*.

*XDG_CONFIG_DIRS*

:   The value of this variable influences the lookup of the system-wide
    configuration file. Typically defaults to */usr/local/etc/xdg:/etc/xdg*.

*XDG_DATA_DIRS*

:   The value of this variable influences the lookup of `.desktop` files for
    launcher items. Typically defaults to */usr/local/share:/usr/share*.

# SIGNALS

*SIGHUP*, *SIGTERM*, *SIGINT*

:   Gracefully terminates tint3.

*SIGUSR1*

:   Causes tint3 to restart and reload the configuration file in use.

*SIGUSR2*

:   Causes tint3 to **execvp(3)** and replace its current process image with the
    one pointed at by `argv[0]`. Allows for a seamless upgrade of tint3 through
    binary package upgrades.

# AUTHORS

tint3 written by Daniele Cocca <https://github.com/jmc-88> and based on tint2,
originally written by Thierry Lorthiois &lt;lorthiois@bbsoft.fr>, in turn based
on ttm, originally written by Pål Staurland &lt;staura@gmail.com>.

# SEE ALSO

**tint2(1)**, **tint3rc(5)**

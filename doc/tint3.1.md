% TINT3(1) v0.3.0 | tint3 user manual

# NAME

**tint3** - lightweight panel/taskbar

# SYNOPSIS

| **tint3** \[**-c** config-file]

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

% TINT3RC(5) v0.3.0 | tint3 user manual

# NAME

**tint3rc** - Configuration file for the tint3 panel

# DESCRIPTION

A tint3 configuration file describes the look and the behaviour of the tint3 panel.

# CONFIGURATION ENTRIES

A tint3 configuration file consists of a series of directives and key/value
pairs, one pair on each line.
Keys can contain alphanumeric characters and underscores ("**\_**"), but cannot start with an underscore.
Values can contain any character and are terminated by a newline.
Keys and values are separated by a single equals sign ("**=**"), optionally surrounded by whitespace.

As the configuration file is unstructured, some entries take the special meaning of a section marker. These are explicitly noted below.

## COMMENTS

Comments are only allowed to start at the _beginning_ of a line (not counting
whitespace), and are initiated by a leading **#** character.
Comments are intended for human consumption and don't bear special meaning in
the configuration file.

## DIRECTIVES

@import *path*

:   Imports the contents of another tint3 configuration file into the one
    currently being evaluated.

    If the file cannot be loaded, an error will be logged and the entry will be
    skipped.

    The given path name can be *relative* (to the logical path of the config
    file currently being evaluated), *absolute*, and it supports shell-like
    *expansion*.

## GENERAL

font_shadow = &lt;boolean>

:   Determines whether font shadows are enabled.

strut_policy = *none* | *minimum* | *follow_size*

:   Instructs the window manager on how to handle maximized windows.

    - *none* means maximized windows will use the full screen area.
    - *minimum* means maximized windows will expand to the full screen area,
    minus the hidden panel size.
    - *follow_size* means maximized windows will be expand to the full screen
    area, minux the current panel size.

## BACKGROUNDS

rounded = &lt;integer>

:   Determines the rounding, in pixels, of this background.

    This entry is a *section marker* that determines the beginning of a new
    background. Backgrounds are given an incremental id number that can be used
    to refer to them, starting with 1 (background 0 is preallocated and
    defaults to be fully transparent).

border_width = &lt;integer>

:   Width, in pixels, of this border.

background_color = &lt;color>

:   Color of this background in the normal state.

background_color_hover = &lt;color>

:   Color of this background in the mouse hover state.

background_color_pressed = &lt;color>

:   Color of this background in the mouse pressed state.

border_sides = *TRBL*

:   Sides on which the border should be drawn. Can be any combination of:

    - **T** (top)
    - **R** (right)
    - **B** (bottom)
    - **L** (left)

    If not given, defaults to all sides.

border_color = &lt;color>

:   Color of this background's border in the normal state.

border_color_hover = &lt;color>

:   Color of this background's border in the mouse hover state.

border_color_pressed = &lt;color>

:   Color of this background's border in the mouse pressed state.

gradient_id = &lt;integer>

:   Id of this background's gradient in the normal state.

gradient_id_hover = &lt;integer>

:   Id of this background's gradient in the mouse hover state.

gradient_id_pressed = &lt;integer>

:   Id of this background's gradient in the mouse pressed state.

## GRADIENTS

gradient = *vertical* | *horizontal* | *radial*

:   Determines the type of gradient to use. It can be *vertical* (top to
    bottom), *horizontal* (left to right), or *radial* (centre to outer edges).

    This entry is a *section marker* that determines the beginning of a new
    gradient. Gradients are given an incremental id number that can be used
    to refer to them, starting with 1 (background 0 is preallocated and
    defaults to be fully transparent).

start_color = &lt;color>

:   Starting color of this gradient.

end_color = &lt;color>

:   Ending color of this gradient.

color_stop = &lt;integer> &lt;color>

:   Additional color stop for this gradient. The first integer of this value
    indicates the percentage where to add the color stop, and it must be in the
    open range (0; 100); i.e., negative values or values above 100 are not
    allowed, as well as **0** and **100**, to avoid conflicting with
    *start_color* and *end_color*.

## PANEL

panel_monitor = &lt;integer> | &lt;string> | "all"

:   Indicates the monitor on which the panel should be displayed.
    This value can be given as a positive integer representing the index of the
    monitor (starting from 1), or as a string representing the monitor name,
    or as the special string value of *all* which makes tint3 display the panel
    on all available monitors.

    This option requires XRANDR. If the extension is not enabled at compile
    time or unavailable at run time, or if the given monitor name or index can't be found, tint3 will default to displaying the panel on all available monitors.

panel_size = &lt;integer>\[%] &lt;integer>\[%]

:   Size of the panel.

    The first integer indicates the width of the panel. This value is
    considered to be given in pixels, unless it is followed by *%*, in which
    case it's considered a percentage of the current screen width.

    The second integer indicates the height of the panel. This value is
    considered to be given in pixels, unless it is followed by *%*, in which
    case it's considered a percentage of the current screen height.

panel_items = *LTBSC*

:   Chooses the items to show in the panel, and their ordering.
    This value is a string where each character represents an item to display.

    - **L** indicates the launcher.
    - **T** indicates the taskbar.
    - **B** indicates the battery.
    - **S** indicates the system tray.
    - **C** indicates the clock.

panel_margin = &lt;integer> \[&lt;integer>]

:   Margin, in pixels, of the panel. The first integer represents the margin
    on the **X** axis. The second, if given, represents the margin on the
    **Y** axis.

panel_padding = &lt;integer> \[&lt;integer> \[&lt;integer>]]

:   Padding, in pixels, of the panel.

    The first integer represents the internal padding on the **X** axis (and the spacing between children areas, if a third integer is not given).

    The second integer represents the internal padding on the **Y** axis.

    The third integer represents the spacing on the **X** axis between
    children areas, overriding the value given by the first integer.

panel_position = &lt;alignment> &lt;alignment> \[&lt;position>]

:   Position and alignment of the panel.

    The first value indicates the vertical alignment of the panel and can be
    either *top*, *bottom* or *center*. Defaults to *center*.

    The second value indicates the horizontal alignment of the panel and can
    be either *left*, *right* or *center*. Defaults to *center*.

    The third value indicates the position of the panel and can be either
    *horizontal* or *vertical*. Defaults to *horizontal*.

panel_background_id = &lt;integer>

:   Id of the background used by the panel.

wm_menu = &lt;boolean>

:   Determines whether unhandled clicks should be forwarded to the window
    manager.

panel_dock = &lt;boolean>

:   Determines whether the panel window should be placed in the window manager's
    dock.

urgent_nb_of_blink = &lt;integer>

:   Maximum number of blinks allowed for windows with the *urgent* hint.

panel_layer = *normal* | *top* | *bottom*

:   Chooses whether the panel should behave like a *normal* window, or placed
    always on *top* of other windows, or always at the *bottom*.

## BATTERY

battery = &lt;boolean>

:   Determines whether the battery indicator is enabled.
    This is a legacy entry, superseded by *panel_items*. If that option is
    present, this one is ignored.

battery_low_status = &lt;integer>

:   Battery percentage to consider "low".

battery_low_cmd = &lt;command>

:   Command to run when the battery reaches the "low" level.

bat1_font = &lt;string>

:   Pango font descriptor to use for the first line of the battery indicator.

bat2_font = &lt;string>

:   Pango font descriptor to use for the second line of the battery indicator.

battery_font_color = &lt;color>

:   Color to use for the battery font.

battery_padding = &lt;integer> \[&lt;integer> \[&lt;integer>]]

:   Padding, in pixels, of the battery.

    The first integer represents the internal padding on the **X** axis (and the spacing between children areas, if a third integer is not given).

    The second integer represents the internal padding on the **Y** axis.

    The third integer represents the spacing on the **X** axis between
    children areas, overriding the value given by the first integer.

battery_background_id = &lt;integer>

:   Id of the background used by the battery.

battery_hide = &lt;integer>

:   Battery fullness percentage after which to hide the indicator.
    Defaults to **101%** if not given, meaning the indicator will always be
    shown.

## CLOCK

time1_format = &lt;string>

:   Format string to use for the first line of the clock applet.
    See **strftime(3)** for accepted conversion specifications.

    This entry is a *section marker* that determines the presence of a clock
    applet in legacy configurations. It is ignored for this purpose if
    *panel_items* is present.

time2_format = &lt;string>

:   Format string to use for the second line of the clock applet.
    See **strftime(3)** for accepted conversion specifications.

time1_font = &lt;string>

:   Pango font descriptor to use for the first line of the clock applet.

time1_timezone = &lt;string>

:   Name of the timezone to use for the first line of the clock applet.

time2_timezone = &lt;string>

:   Name of the timezone to use for the second line of the clock applet.

time2_font = &lt;string>

:   Pango font descriptor to use for the second line of the clock applet.

clock_font_color = &lt;color>

:   Color to use for the clock font.

clock_padding = &lt;integer> \[&lt;integer> \[&lt;integer>]]

:   Padding, in pixels, of the clock applet.

    The first integer represents the internal padding on the **X** axis (and the spacing between children areas, if a third integer is not given).

    The second integer represents the internal padding on the **Y** axis.

    The third integer represents the spacing on the **X** axis between
    children areas, overriding the value given by the first integer.

clock_background_id = &lt;integer>

:   Id of the background used by the clock applet.

clock_tooltip = &lt;string>

:   Format string to use for the clock applet tooltip.
    See **strftime(3)** for accepted conversion specifications.

clock_tooltip_timezone = &lt;string>

:   Name of the timezone to use for the second line of the clock tooltip.

clock_lclick_command = &lt;string>

:   Command to execute when the clock applet is left clicked.

clock_rclick_command = &lt;string>

:   Command to execute when the clock applet is right clicked.

## TASKBAR

taskbar_mode = *single_desktop* | *multi_desktop*

:   Determines whether the taskbar should be shown on a single desktop or on
    all desktops.

taskbar_padding = &lt;integer> \[&lt;integer> \[&lt;integer>]]

:   Padding, in pixels, of the taskbar.

    The first integer represents the internal padding on the **X** axis (and the spacing between children areas, if a third integer is not given).

    The second integer represents the internal padding on the **Y** axis.

    The third integer represents the spacing on the **X** axis between
    children areas, overriding the value given by the first integer.

taskbar_background_id = &lt;integer;

:   Id of the background used by the taskbar.

taskbar_active_background_id = &lt;integer>

:   Id of the background used by the active taskbar.

taskbar_name = &lt;boolean>

:   Determines whether the taskbar name should be displayed.

taskbar_name_padding = &lt;integer> \[&lt;integer> \[&lt;integer>]]

:   Padding, in pixels, of the taskbar name.

    The first integer represents the internal padding on the **X** axis (and the spacing between children areas, if a third integer is not given).

    The second integer represents the internal padding on the **Y** axis.

    The third integer represents the spacing on the **X** axis between
    children areas, overriding the value given by the first integer.

taskbar_name_background_id = &lt;integer>

:   Id of the background used by the taskbar name.

taskbar_name_active_background_id = &lt;integer>

:   Id of the background used by the active taskbar name.
    Defaults to *taskbar_name_background_id*.

taskbar_name_font = &lt;string>

:   Pango font descriptor to use for the taskbar name.

taskbar_name_font_color = &lt;color>

:   Color to use for the taskbar name font.

taskbar_name_active_font_color = &lt;color>

:   Color to use for the active taskbar name font.

## TASKS

task_text = &lt;boolean>

:   Determines whether the task name should be displayed.

task_icon = &lt;boolean>

:   Determines whether the task icon should be displayed.

task_centered = &lt;boolean>

:   Determines whether the task icon and/or text should be centered.

task_width = &lt;integer>

:   Sets the maximum task area width to the given integer and the maximum task
    area height to its default of **30**.
    Legacy entry, superseded by *task_maximum_size*.

task_maximum_size = &lt;integer> \[&lt;integer>]

:   Sets the maximum task area width to the first given integer and the maximum
    task area height to the second given integer (or its default of **30**, if
    missing).

task_padding = &lt;integer> \[&lt;integer> \[&lt;integer>]]

:   Padding, in pixels, of the task area.

    The first integer represents the internal padding on the **X** axis (and the spacing between children areas, if a third integer is not given).

    The second integer represents the internal padding on the **Y** axis.

    The third integer represents the spacing on the **X** axis between
    children areas, overriding the value given by the first integer.

task_font_color = &lt;color>

:   Color to use for the task font.

task_active_font_color = &lt;color>

:   Color to use for the active task font.

task_iconified_font_color = &lt;color>

:   Color to use for the iconified task font.

task_urgent_font_color = &lt;color>

:   Color to use for the urgent task font.

task_icon_asb = &lt;integer> &lt;integer> &lt;integer>

:   Relative alpha, saturation and brightness adjustments for the task icon,
    expressed as percentages.

task_active_icon_asb = &lt;integer> &lt;integer> &lt;integer>

:   Relative alpha, saturation and brightness adjustments for the active task
    icon, expressed as percentages.

task_iconified_icon_asb = &lt;integer> &lt;integer> &lt;integer>

:   Relative alpha, saturation and brightness adjustments for the iconified
    task icon, expressed as percentages.

task_urgent_icon_asb = &lt;integer> &lt;integer> &lt;integer>

:   Relative alpha, saturation and brightness adjustments for the urgent task
    icon, expressed as percentages.

task_background_id = &lt;integer;

:   Id of the background used by the task area.

task_active_background_id = &lt;integer;

:   Id of the background used by the active task area.

task_iconified_background_id = &lt;integer;

:   Id of the background used by the iconified task area.

task_urgent_background_id = &lt;integer;

:   Id of the background used by the urgent task area.

tooltip = &lt;boolean>

:   Determines whether the task tooltip is enabled.
    Legacy entry, superseded by *task_tooltip*.

task_tooltip = &lt;boolean>

:   Determines whether the task tooltip is enabled.

## SYSTEM TRAY

systray = &lt;boolean>

:   Determines whether the systray is enabled.
    This is a legacy entry, superseded by *panel_items*. If that option is
    present, this one is ignored.

systray_padding = &lt;integer> \[&lt;integer> \[&lt;integer>]]

:   Padding, in pixels, of the system tray area.

    The first integer represents the internal padding on the **X** axis (and the spacing between children areas, if a third integer is not given).

    The second integer represents the internal padding on the **Y** axis.

    The third integer represents the spacing on the **X** axis between
    children areas, overriding the value given by the first integer.

    This entry is a *section marker* that determines the presence of a system
    tray area in legacy configurations. It is ignored for this purpose if
    *panel_items* is present.

systray_background_id = &lt;integer>

:   Id of the background used by the system tray area.

systray_sort = *descending* | *ascending* | *left2right* | *right2left*

:   Determines how the system tray icons should be sorted.

    - *descending* inserts new icons according to the window name, in reverse
    lexicographical order.
    - *ascending* inserts new icons according to the window name, in
    lexicographical order.
    - *left2right* appends new icons to the end of the icon list.
    - *right2left* prepends new icons to the start of the icon list.

systray_icon_size = &lt;integer>

:   Size, in pixels, of system tray icons.

systray_icon_asb = &lt;integer> &lt;integer> &lt;integer>

:   Relative alpha, saturation and brightness adjustments for the system tray
    icons, expressed as percentages.

## LAUNCHERS

launcher_padding = &lt;integer> \[&lt;integer> \[&lt;integer>]]

:   Padding, in pixels, of the launchers area.

    The first integer represents the internal padding on the **X** axis (and the spacing between children areas, if a third integer is not given).

    The second integer represents the internal padding on the **Y** axis.

    The third integer represents the spacing on the **X** axis between
    children areas, overriding the value given by the first integer.

launcher_background_id = &lt;integer>

:   Id of the background used by the launchers area.

launcher_icon_size = &lt;integer>

:   Size, in pixels, of the launcher icons.

launcher_item_app = &lt;string>

:   Path to a *.desktop* file to add to the launcher area. This must be
    conforming to the Desktop Entry Specification from freedesktop.org.

    The path given here supports shell-like expansion. As an example,
    home-relative paths for the current user can be be specified in any of the
    following formats:

    - ~/something.desktop
    - ~*user*/something.desktop
    - $HOME/something.desktop

launcher_icon_theme &lt;string>

:   Name of the icon theme to use when looking up for icons used by the
    *.desktop* files to be loaded.
    If an XSETTINGS manager is running, tint3 will ignore this setting and
    load the icon name from there.

launcher_icon_asb = &lt;integer> &lt;integer> &lt;integer>

:   Relative alpha, saturation and brightness adjustments for the launcher
    icons, expressed as percentages.

launcher_tooltip = &lt;boolean>

:   Determines whether the launcher tooltip is enabled.

## TOOLTIPS

tooltip_show_timeout = &lt;float>

:   Time, in seconds, after which tooltips are shown.

tooltip_hide_timeout = &lt;float>

:   Time, in seconds, after which tooltips are hidden.

tooltip_padding = &lt;integer> \[&lt;integer>]

:   Padding, in pixels, of the tooltip area.

    The first integer represents the internal padding on the **X** axis.

    The second integer represents the internal padding on the **Y** axis.

tooltip_background_id = &lt;integer>

:   Id of the background used by the tooltip area.

tooltip_font_color = &lt;color>

:   Color to use for the tooltip font.

tooltip_font = &lt;string>

:   Pango font descriptor to use for tooltips.

## MOUSE ACTIONS

mouse_middle = &lt;mouse_action>

:   Determines how to handle a click of the middle mouse button or the mouse
    wheel. See **FORMAT OF VALUES** for a description of the accepted values.

mouse_right = &lt;mouse_action>

:   Determines how to handle a click of the right mouse button.
    See **FORMAT OF VALUES** for a description of the accepted values.

mouse_scroll_up = &lt;mouse_action>

:   Determines how to handle a mouse wheel up event.
    See **FORMAT OF VALUES** for a description of the accepted values.

mouse_scroll_down = &lt;mouse_action>

:   Determines how to handle a mouse wheel down event.
    See **FORMAT OF VALUES** for a description of the accepted values.

mouse_effects = &lt;boolean>

:   Determines whether icon effects on mouse events are enabled.

mouse_hover_icon_asb = &lt;integer> &lt;integer> &lt;integer>

:   Relative alpha, saturation and brightness adjustments for launcher and
    taskbar icons on a mouse hover event, expressed as percentages.

mouse_pressed_icon_asb = &lt;integer> &lt;integer> &lt;integer>

:   Relative alpha, saturation and brightness adjustments for launcher and
    taskbar icons on a mouse down event, expressed as percentages.

## AUTOHIDE

autohide = &lt;boolean>

:   Determines whether panel autohide is enabled.

autohide_show_timeout = &lt;float>

:   Time, in seconds, after which the panel is shown.

autohide_hide_timeout = &lt;float>

:   Time, in seconds, after which the panel is hidden.

autohide_height = &lt;integer>

:   Height, in pixel, of the panel in its hidden state.
    This entry has a lower bound of **1**.

# FORMAT OF VALUES

## &lt;boolean>

Either a string value or a numerical value.

If a string: "true", "t", "yes", "y" or "on" mean **true**; "false", "f", "no",
"n" or "off" mean **false**.

If a number: **0** means **false**, **non-zero** means **true**.

## &lt;color>

RGBA color value. It's specified as a CSS-like hex color string, followed by
an integer representing the opacity. As an example:

    background_color = #ff0000 25

creates a red background with 25% opacity.
A short hex value is also accepted. As an example:

    background_color = #f00 25

is identical to the previous example.

## &lt;mouse_action>

String indicating the action to perform for the given mouse event.
It can be one of the following:

- *none* does nothing (default).
- *close* closes the focused window.
- *toggle* toggles the active state on the focused window.
- *iconify* iconifies the focused window.
- *shade* toggles the shaded state on the focused window.
- *toggle_iconify* toggles the iconified state of the focused window.
- *maximize_restore* maximizes or restores the focused window.
- *desktop_left* moves the window to the desktop to the left.
- *desktop_right* moves the window to the desktop to the right.
- *next_task* activates the next window.
- *prev_task* activates the previous window.

# SEE ALSO

**tint3(1)**, **strftime(3)**

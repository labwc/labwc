% labwc-actions(5)
% Johan Malm
% 31 Aug, 2020

# NAME

labwc - actions

# ACTIONS

Actions are used in key bindings.

Action syntax:

    <action name="NAME">
      OPTION
    </action>

where `NAME` is the name of the action as listed below, and `OPTION` is a set
of tags specific to each action as defined below.

`Execute`

:   Execute command specified by `<command>` option.

`Exit`

:   Exit labwc.

`NextWindow`

:   Cycle focus to next window.

# SEE ALSO

labwc(1), labwc-config(5), labwc-theme(5)

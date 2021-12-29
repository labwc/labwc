labwc-environment(1)

# NAME

labwc - environment

# DESCRIPTION

The environment file is parsed as <variable>=<value> and sets environment
variables accordingly.

Note that the environment file is treated differently by openbox where
it is simply sourced prior to running openbox.

# CONFIGURATION

It is recommended to specify keyboard settings here. See
xkeyboard-config(7) for details.

For example, this sets keyboard layout to Swedish:

```
XKB_DEFAULT_LAYOUT=se
```

*XCURSOR_THEME* and *XCURSOR_SIZE* are also supported to set cursor theme
and size respectively. The default size is 24.

Cursor themes can be found a the command below or similar:

```
find /usr/share/icons/ -type d -name "cursors"
```

# SEE ALSO

labwc(1)

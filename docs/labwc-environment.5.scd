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

# SEE ALSO

labwc(1)

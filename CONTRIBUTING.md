# How to Contribute

1. Report bugs as github issues. We don't use a template, but try to provide
   some sensible information such as what happened, what you expected to happen
   and steps to reproduce. If applicable try with default configuration.  If
   you are able to, try to do some debugging (guidelines below).

2. Submit patches as github pull-requests. If you wish to introduces significant
   changes or new features, consult the [scope document], discuss on IRC or via
   a github issue first.

# Debugging

There is no one-way-fits-all method for debugging, so you have to use your
antennae and do some detective work.

This section contains some approachies which may prove useful.

If the compositor crashes, a good starting point is to produce a backtrace by
building with ASAN/UBSAN:

```
meson -Db_sanitize=address,undefined build/
ninja -C build/
```

Get debug log with `labwc -d`. The log can be directed to a file with `labwc -d
2>log.txt`

To see what is happening on the wayland protocol for a specific client, run it
with environment variable `WAYLAND_DEBUG` set to 1, for example `WAYLAND_DEBUG=1
foot`.

To see what the compositor is doing on the protocol run `labwc` nested (i.e.
start labwc from a terminal in another instance of labwc or some other
compositor) with `WAYLAND_DEBUG=server`. This filters out anything from clients.

For wayland clients, you can get a live view of some useful info using [wlhax].

If you think you've got a damage issue, you can run labwc like this:
`WLR_SCENE_DEBUG_DAMAGE=highlight labwc` to get a visual indication of damage
regions.

To emulate multiple outputs (even if you only have one physical monitor), run
with `WLR_WL_OUTPUTS=2 labwc` or similar. See [`wlroots/docs/env_vars.md`] for
more options.

For some types of bugs, it might be useful to find out which mesa driver (.so)
you are using. This can be done with `EGL_LOG_LEVEL=debug labwc 2>&1 | grep
MESA-LOADER`

To rule out driver issues you can run with `WLR_RENDERER=pixman labwc`

You can also get some useful system info with [drm_info].

Use `sudo libinput debug-events` to show input events.

From a terminal you can use `xev -event keyboard` and `wev -f wl_keyboard:key`
to analyse keyboard events

# Packaging

Some distributions carry labwc in their repositories or user repositories.

- @ptrcnull (Alpine)
- @narrat (Arch)
- @jbeich (FreeBSD)
- @adcdam (Slackware)

kindly maintain the packages in their respective distro.

Let's keep them informed of new releases and any changes that relate to
packaging.  If you are maintaining a labwc package for another distro feel free
to open an issue so we can add you to this list.

# Coding Style

labwc is written in Drew Devault's preferred [coding style] which is based
on the [Linux kernel coding style].

The reasons for specifying a style is not that we enjoy creating rules, but
because it makes reading/maintaining the code and spotting problems much easier.

If you're new to this style and want to get going quickly, either just imitate
the style around you, or read the summary below and use [checkpatch.pl] to run
some formatting checks.

## Style summary

Functions are defined as:

```
return type
name(parameters...)
{
	body
}
```

Opening braces go on the same line, except for function definitions, and are
mandatory even for one-line statements.

```
if (cond) {
	...
}
```

Put * with the identifier when defining pointers: `char *foo`

Spaces are placed around binary operators `int x = y * (2 + z);`, but not unary
`foo(--a, -b);`

```
	/* This is a single-line comment */

	/*
	 * This is a multi-line comment which is much much much much much much
	 * longer.
	 */
```

`#include` statements at the top of the file are organized by locality (`<>`
first, then `""`), then alphabetized.

The line length limit is 80 columns, although it is a bit soft.

Tabs are 8 columns wide. Identation with spaces is not used.

When breaking a statement onto several lines, indent the subsequent lines once.
If the statement declares a `{}` block, indent twice instead.

## labwc specifics

We prefer `sizeof(*foo)` over `sizeof(struct foo)`

We use `if (!p)` instead of `if (p == 0)` or `if (p == NULL)`

When documenting functions in header files we use the Linux kernel approach.

```
/**
 * foo - what foo does
 * @var1: description of variable
 * @var2: ditto
 * Notes and further information
 * Return value unless it is obvious
 */
```

# API

We have a very small, modest API and encourage you to use it.

1. [common/mem.h](https://github.com/labwc/labwc/blob/master/include/common/mem.h)

Use `struct wlr_box *box = znew(*box);` instead of
`struct wlr_box *box = calloc(1, sizeof(struct wlr_box));`

Use `zfree(foo)` instead of `free(foo)` to set foo=NULL.

# The use of glib

We try to keep the use of glib pretty minimal for the following reasons:

- The use of glib has been known to make AddressSanitiser diagnose false
  positives and negatives.
- Log messages coming from glib functions look inconsistent.
- The use of glib functions, naming-conventions and iterators in a code base
  that is predominantly ANSI C creates a clash which makes readability and
  maintainability harder.
- Mixing gmalloc()/malloc() and respective free()s can create problems with
  memory pools [^1]

Having said that, with our use of cairo and pango we depend on glib-2.0 anyway
so linking with it and making use of some of its helper functions comes for free
and can keep the code simpler.

For example, if we were going to carry out extensive string manipulation,
GString and utf8 helpers would be okay. Some functions such as
`g_utf8_casefold()` would be pretty hard to write from scratch and are fine to
use. Having said that, labwc does not do much string-work and thus far none of
these have been needed.

The following functions are used today and are deemed acceptable by the core
devs:

- `g_shell_parse_argv()`
- `g_strsplit()`

# The use of GNU extensions

We avoid [GNU C extensions] because we want to fit into the eco-system
(wayland and wlroots) we live in.

We do use `__typeof__` which strictly speaking is a GNU C extension (`typeof`)
but through the use of `__` is supported by gcc and clang without defining
`_GNU_SOURCE`. The justification for this is that Wayland uses it, for example
in the [`wl_container_of()`] macro which is needed in `wl_list*` and it
does provide pretty big benefits in terms of type safety.

We compile with `-std=c11` because that's what 'wlroots' uses and we do not
want to increase the entry-level for OSs without good reason (and currently
we can't think of one).

# Commit Messages

The log messages that explain changes are just as important as the changes
themselves. Try to describe the 'why' to help future developers.

Write [commit messages] like so, keeping the top line to this sort of syntax:

```
cursor: add special feature
```

This first line should:

- Be a short description
- In most cases be prefixed with "area: " where area refers to a filename
  or identifier for the general area of the code being modified.
- Not capitalize the first word following the "area: " prefix, unless
  it's a name, acronym or similar.
- Skip the full stop

And please wrap the commit message at max 74 characters, otherwise `git log`
and similar look so weird. URLs and other references are exempt.

# Naming Convention

There are three types of coordinate systems: surface, output and layout - for
which the variables (sx, sy), (ox, oy) and (lx, ly) are used respectively in
line with wlroots.

With the introduction of the scene-graph API, some wlroots functions also use
node coordinates (nx, ny) but we prefer (sx, sy) where possible.

We use the prefix `handle_` for signal-handler-functions in order to be
consistent with sway and rootston. For example
`view->request_resize.notify = handle_request_resize`

# Submitting patches

Base both bugfixes and new features on `master`.

# Native Language Support

Translators can add their `MY_LOCALE.po` files to the `po` directory
based on `po/labwc.pot` and issue a pull request. For example:

```
xgettext --keyword=_ --language=C --add-comments -o po/labwc.pot src/menu/menu.c
```

[See this tutorial for further guidance](https://www.labri.fr/perso/fleury/posts/programming/a-quick-gettext-tutorial.html)

[scope document]: https://github.com/labwc/labwc-scope#readme
[`wlroots/docs/env_vars.md`]: https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/docs/env_vars.md
[wlhax]: https://git.sr.ht/~kennylevinsen/wlhax
[drm_info]: https://github.com/ascent12/drm_info
[coding style]: https://git.sr.ht/~sircmpwn/cstyle
[Linux kernel coding style]: https://www.kernel.org/doc/html/v4.10/process/coding-style.html
[commit messages]: https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/CONTRIBUTING.md#commit-messages 
[checkpatch.pl]: https://github.com/johanmalm/checkpatch.pl
[GNU C extensions]: https://gcc.gnu.org/onlinedocs/gcc/C-Extensions.html
[`wl_container_of()`]: https://github.com/wayland-project/wayland/blob/985ab55d59db45ea62795c76dff5949343e86b2f/src/wayland-util.h#L409

[^1]: The reference docuementation for glib notes that:
      "It's important to match g_malloc() with g_free(), plain malloc() with
      free(), and (if you're using C++) new with delete and new[] with
      delete[]. Otherwise bad things can happen, since these allocators may use
      different memory pools (and new/delete call constructors and
      destructors)."
      See: https://developer.gimp.org/api/2.0/glib/glib-Memory-Allocation.html


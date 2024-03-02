- [1. How to Contribute](#how-to-contribute)
- [2. Debugging](#debugging)
  - [2.1 Backtraces](#backtraces)
  - [2.2 Debug Logs](#debug-logs)
  - [2.3 Output](#output)
  - [2.4 Input](#input)
- [3. Packaging](#packaging)
- [4. Coding Style](#coding-style)
  - [4.1 Linux Kernel Style Basics](#linux-kernel-style-basics)
  - [4.2 Devault Deviations](#devault-deviations)
  - [4.3 Labwc Specifics](#labwc-specifics)
    - [4.3.1 API](#api)
    - [4.3.2 The Use of glib](#the-use-of-glib)
    - [4.3.3 The use of GNU extensions](#the-use-of-gnu-extensions)
    - [4.3.4 Naming Conventions](#naming-conventions)
- [5. Commit Messages](#commit-messages)
- [6. Submitting Patches](#submitting-patches)
- [7. Native Language Support](#native-language-support)
- [8. Upversion](#upversion)

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

This section contains some approaches which may prove useful.

## Backtraces

If the compositor crashes, a good starting point is to produce a backtrace by
building with ASAN/UBSAN:

```
meson setup -Db_sanitize=address,undefined build/
meson compile -C build/
```

## Debug Logs

Get debug log with `labwc -d`. The log can be directed to a file with `labwc -d
2>log.txt`

To see what is happening on the wayland protocol for a specific client, run it
with environment variable `WAYLAND_DEBUG` set to 1, for example `WAYLAND_DEBUG=1
foot`.

To see what the compositor is doing on the protocol run `labwc` nested (i.e.
start labwc from a terminal in another instance of labwc or some other
compositor) with `WAYLAND_DEBUG=server`. This filters out anything from clients.

For wayland clients, you can get a live view of some useful info using [wlhax].

## Output

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

## Input

Use `sudo libinput debug-events` to show input events.

From a terminal you can use `xev -event keyboard` and `wev -f wl_keyboard:key`
to analyse keyboard events

# Packaging

Some distributions carry labwc in their repositories or user repositories.

- @ptrcnull (Alpine)
- @narrat (Arch)
- @b1rger (Debian)
- @jbeich (FreeBSD)
- @AndersonTorres (NixOS)
- @adcdam (Slackware)
- @bdantas (Tiny Core Linux)
- @Visone-Selektah (Venom Linux)
- @tranzystorekk (Void Linux)

kindly maintain the packages in their respective distro.

Let's keep them informed of new releases and any changes that relate to
packaging.  If you are maintaining a labwc package for another distro feel free
to open an issue so we can add you to this list.

# Coding Style

labwc is written in the [Linux kernel coding style] with a small number of
deviations to align with [Drew Devault's preferred coding style] namely:

1. [Function Declaration](https://git.sr.ht/~sircmpwn/cstyle#function-declarations)
2. [Braces for one-line statement](https://git.sr.ht/~sircmpwn/cstyle#brace-placement)
3. [Organisation of header #include statements](https://git.sr.ht/~sircmpwn/cstyle#header-files)
4. [Breaking of long lines](https://git.sr.ht/~sircmpwn/cstyle#splitting-long-lines)

The reasons for specifying a style is not that we enjoy creating rules, but
because it makes reading/maintaining the code and spotting problems much easier.

If you are new to this style and want to get going quickly, either just imitate
the style around you, or read the summary below and use `./scripts/check` to run
some formatting checks.

## Linux Kernel Style Basics

The preferred line length limit is 80 columns, although this is a bit soft.

Tabs are 8 columns wide. Indentation with spaces is not used.

Opening braces go on the same line, except for function definitions.

Put `*` with the identifier when defining pointers, for example `char *foo`

Spaces are placed around binary operators but not unary, like this:

```
int x = y * (2 + z);
foo(--a, -b);
```

`sizeof(*foo)` is preferred over `sizeof(struct foo)`

Use `if (!p)` instead of `if (p == 0)` or `if (p == NULL)`

Comments are written as follows:

```
/* This is a single-line comment */

/*
 * This is a multi-line comment which is much much much much much much
 * longer.
 */
```

When documenting functions in header files we use the [kernel-doc format]:

```
/**
 * function_name() - Brief description of function.
 * @arg1: Describe the first argument.
 * @arg2: Describe the second argument.
 *        One can provide multiple line descriptions
 *        for arguments.
 *
 * A longer description, with more discussion of the function function_name()
 * that might be useful to those using or modifying it. Begins with an
 * empty comment line, and may include additional embedded empty
 * comment lines.
 *
 * The longer description may have multiple paragraphs.
 *
 * Return: Describe the return value of function_name.
 *
 * The return value description can also have multiple paragraphs, and should
 * be placed at the end of the comment block.
 */
```

## Devault Deviations

Functions are defined as below with `name` on a new line:

```
return type
name(parameters...)
{
	body
}
```

Braces are mandatory even for one-line statements.

```
if (cond) {
	...
}
```

`#include` statements at the top of the file are organized by locality (`<>`
first, then `""`), then alphabetized.

When breaking a statement onto several lines, indent the subsequent lines once.
If the statement declares a `{}` block, indent twice instead. Also, place
operators (for example `&&`) on the next line.

```
	if (seat->pressed.surface && ctx->surface != seat->pressed.surface
			&& !update_pressed_surface(seat, ctx)
			&& !seat->drag_icon) {
		if (cursor_has_moved) {
			process_cursor_motion_out_of_surface(server, time_msec);
		}
		return;
	}
```

## Labwc Specifics

### API

We have a very small, modest API and encourage you to use it.

1. `znew()` - as a shorthand for calloc(1, sizeof()) with type checking
   [common/mem.h]

2. `zfree()` to zero after free - [common/mem.h]

3. `wl_list_append()` to add elements at end of lists - [common/list.h]

4. `wl_array_len()` to get number of elements in a `wl_array` [common/array.h]

5. `ARRAY_SIZE()` to get number of elements in visible array
   [common/macros.h]

[common/mem.h]: https://github.com/labwc/labwc/blob/master/include/common/mem.h
[common/list.h]: https://github.com/labwc/labwc/blob/master/include/common/list.h
[common/array.h]: https://github.com/labwc/labwc/blob/master/include/common/array.h
[common/macros.h]: https://github.com/labwc/labwc/blob/master/include/common/macros.h

### The Use of glib

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
use. Having said that, labwc does not do much string-mangling.

The following functions are used today and are deemed acceptable by the core
devs:

- `g_shell_parse_argv()`
- `g_strsplit()`
- `g_pattern_match_simple()`

When using these types of functions it is often desirable to support with some
glib code, which is okay provided it is kept local and self-contained. See
example from `src/theme.c`:

```
static bool
match(const gchar *pattern, const gchar *string)
{
	GString *p = g_string_new(pattern);
	g_string_ascii_down(p);
	bool ret = (bool)g_pattern_match_simple(p->str, string);
	g_string_free(p, true);
	return ret;
}
```

### The use of GNU extensions

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

### Naming Conventions

There are three types of coordinate systems: surface, output and layout - for
which the variables (sx, sy), (ox, oy) and (lx, ly) are used respectively in
line with wlroots.

With the introduction of the scene-graph API, some wlroots functions also use
node coordinates (nx, ny) but we prefer (sx, sy) where possible.

We do not worry about namespace issues too much and we try to not make the code
a pain to use just to uniquify names. If we were writing a library we would
prefix public functions and structs with `lab_`, but we are not. We do however
prefix public function names with the filename of the translation unit.  For
example, public functions in `view.c` begin with `view_`.

We do start enums with `LAB_`

We use the prefix `handle_` for signal-handler-functions in order to be
consistent with sway and rootston. For example
`view->request_resize.notify = handle_request_resize`

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

# Submitting patches

Base both bugfixes and new features on `master`.

# Native Language Support

Translators can add their `MY_LOCALE.po` files to the `po` directory
based on `po/labwc.pot` and issue a pull request. To do this they can
generate their `MY_LOCALE.po` file in a few steps:

1. Edit the `po/LINGUAS` file to add their locale code in English
   alphabetical order to the field of locale codes.
2. Copy the `po/labwc.pot` to `po/MY_LOCALE.po`
3. Edit the newly generated `MY_LOCALE.po` file with some of their
contact and locale details in the header of the file then add the
translation strings under each English string.

[See this tutorial for further guidance](https://www.labri.fr/perso/fleury/posts/programming/a-quick-gettext-tutorial.html)

Code contributors may need to update relevant files if their additions
affect UI elements (at the moment only `src/menu/menu.c`). In this case
the `po/labwc.pot` file needs to be updated so that translators can
update their translations. Remember, many translators are _not_ coders!

The process is fairly trivial however does involve some manual steps.

1. After adding and testing your code additions to satisfaction, backup
`po/labwc.pot`. You need the custom header from that file for the newly
generated .pot file in the next step.

2. From the root of the repository run this:

```
xgettext --keyword=_ --language=C --add-comments -o po/labwc.pot src/menu/menu.c
```

This generates a new pot file at `po/labwc.pot`

3. Copy the header from the original `labwc.pot` to the new one, check
for sanity and commit.

# Upversion

It is generally only the lead-maintainer who will upversion, but in order
not to forget any key step or in case someone else needs to do it, here
follow the steps to be taken:

1. If appropriate, update `revision` in `subprojects/wlroots.wrap` and run
   `git commit -m 'wlroots.wrap: use A.B.C'`
2. Update `NEWS.md` with the release details and run
   `git commit -m 'NEWS.md: update notes for X.Y.Z'`
   Note: If new dependencies are needed, make this clear.
3. In `meson.build` update the version and (if required) the wlroots
   dependency version. Then run `git commit -m 'build: bump version to X.Y.Z'`
4. Run `git tag -a X.Y.Z`. The first line of the commit message should be
   "labwc X.Y.Z" and the body should be the `NEWS.md` additions removing
   hash characters (#) from the headings as these will otherwise be
   ignored by git.
5. On github, create a 'Release' as some distros use this as a trigger. Set it
   as 'latest release'.

[scope document]: https://github.com/labwc/labwc-scope#readme
[`wlroots/docs/env_vars.md`]: https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/docs/env_vars.md
[wlhax]: https://git.sr.ht/~kennylevinsen/wlhax
[drm_info]: https://github.com/ascent12/drm_info
[Drew Devault's preferred coding style]: https://git.sr.ht/~sircmpwn/cstyle
[Linux kernel coding style]: https://www.kernel.org/doc/html/v4.10/process/coding-style.html
[kernel-doc format]: https://docs.kernel.org/doc-guide/kernel-doc.html
[commit messages]: https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/CONTRIBUTING.md#commit-messages
[GNU C extensions]: https://gcc.gnu.org/onlinedocs/gcc/C-Extensions.html
[`wl_container_of()`]: https://github.com/wayland-project/wayland/blob/985ab55d59db45ea62795c76dff5949343e86b2f/src/wayland-util.h#L409

[^1]: The reference documentation for glib notes that:
      "It's important to match g_malloc() with g_free(), plain malloc() with
      free(), and (if you're using C++) new with delete and new[] with
      delete[]. Otherwise bad things can happen, since these allocators may use
      different memory pools (and new/delete call constructors and
      destructors)."
      See: https://developer.gimp.org/api/2.0/glib/glib-Memory-Allocation.html


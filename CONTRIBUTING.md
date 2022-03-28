# Contributing

## How to Contribute

## Packaging

@narrat and @jbeich kindly maintain package on Arch Linux and FreeBSD. Let's
keep them informed of any changes that relate to packaging.

## Coding Style

Let's try to stick to sircmpwn's [coding style]. If you're not used to it, you
can use [checkpatch.pl] to run a few simple formatting checks.

## Commit Messages

Write [commit messages] like so, keeping the top line to this sort of syntax:

```
cursor: add special feature
```

And please wrap the main commit message at max 74 characters, otherwise `git log` and similar look so weird.

## Naming Convention

There are three types of coordinate systems: surface, output and layout - for
which the variables (sx, sy), (ox, oy) and (lx, ly) are used respectively in
line with wlroots.
With the introduction of the scene-graph API, some wlroots functions also use
node coordinates (nx, ny) but we prefer (sx, sy) where possible.


[coding style]: https://git.sr.ht/~sircmpwn/cstyle
[commit messages]: https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/CONTRIBUTING.md#commit-messages 
[checkpatch.pl]: https://github.com/johanmalm/checkpatch.pl


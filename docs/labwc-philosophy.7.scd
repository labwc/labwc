labwc-philosophy(7)

# INTRODUCTION

The purpose of this document is to provide a clear, public definition of the
project's "taste". It is intended to help set the direction of labwc,
particularly when evaluating code changes and new features.

Unless otherwise stated, "stable" in this document describes a mindset and
approach rather than merely the absence of bugs. It is the opposite of
exciting, dramatic and hyped.

Throughout this document, "we" refers to the maintainers who merge code into
labwc:

- @johanmalm
- @Consolatis
- @jlindgren90

This is a living document. It should change as labwc and the surrounding
ecosystem mature. We consider this important: principles should guide the
project, but should not become an identity that prevents it from changing.

# RESPONSIBILITIES

The primary responsibility of maintainers is not to accept good code. It is to
preserve the project's long-term coherence.

A change can therefore be technically correct, well written and useful, and
still be inappropriate for labwc.

Maintainers are responsible for considering not only whether something can be
merged, but what merging it means for the project five or ten years from now.

# TASTE

We want labwc to be small, calm, predictable and narrowly focused. We regard
this as taste discipline, not a lack of ambition.

We approach labwc as craftsmanship rather than a vehicle for dominance.
labwc is intentionally stable, non-experimental and narrowly scoped.
The goal is not to innovate in UX or to create a full desktop environment.

Where other compositors may optimise for animation, workflow
re-invention or novel UX, we generally prefer predictability, muscle memory,
configuration-file-driven behaviour and Openbox parity.

In short: Wayland, but without philosophical re-education.

. We add things slowly, preserve semantic meaning and behaviour, and avoid
  breaking things that people rely upon.
. We believe longevity requires tolerance, conservatism and
  predictability.
. Where possible, we push compatibility upwards in the stack, through
  Wayland protocols and shared wlroots functionality, rather than
  implementing compositor-specific solutions.
. We prefer the ecosystem around the compositor to absorb complexity,
  provide bridges and accommodate differing UX preferences.
. We generally avoid non-standard extension systems and
  compositor-specific plugin APIs.
. We resist fear-driven roadmaps. Arguments such as "if you do not merge
  this, users will leave" are not, by themselves, reasons to change the
  project's direction.
. We value stability over innovation, correctness over features and
  clarity over abstraction.
. We trade purity for practical benefit. labwc exists in the real world
  and must work within it.
. We prefer innovation to happen outwards, in applications and tools built
  around the compositor, rather than in the compositor itself.
. We optimise for predictable behaviour over the long term, rather than
  for minimal change or minimal implementation complexity in the short
  term.
. We think that backward-compatible configuration is important. Changes
  should not introduce surprising changes in behaviour.
. We try to deliver a predictable release cycle as an important part of
  the overall user experience.
. We prioritise interoperability and compatibility, including support for
  desktop components that use standard protocols.
. We accept inherited or awkward solutions when their practical benefits
  outweigh the cost of replacing them. For example, using XML for
  configuration is preferable to replacing a working system merely because
  something newer is fashionable.

We try to avoid:

. Unnecessary divergence from upstream projects such as wlroots.
. Multiple ways of configuring the same thing, particularly where this
  creates complex precedence rules.
. Features that introduce fragile logic or increase the probability of
  race conditions.
. Absorbing responsibilities that properly belong elsewhere in the
  ecosystem.

As a stable compositor, we generally prefer not to:

. Thrive on strong opinions.
. Pursue rapid evolution for its own sake.
. Knowingly break existing behaviour.
. Champion workarounds merely because standards are slow to establish.
. Treat the compositor as an artistic expression.
. Impose a strong aesthetic identity.
. Equate visible momentum with progress.
. Rely on charismatic leadership.
. Use innovation itself as a source of legitimacy.

# NEW FEATURES

Written, public criteria for evaluating new features are important. They help
maintain consistency and reduce the possibility that decisions become personal.

Well-tested, working code is necessary but not sufficient for inclusion in
labwc.

Every feature adds to the project's permanent surface area. Once users depend
on a feature, removing it can be disproportionately expensive in terms of
development effort, user experience and project reputation. New features
therefore need to be considered partly as long-term commitments.

We generally accept a new feature only when:

. It serves a clearly articulated system-level goal and provides a net
  benefit.
. The additional complexity is justified by broadly applicable use-cases,
  rather than primarily by niche or esoteric requirements,
. It reduces overall complexity, or replaces something worse.
. It fits within the declared scope of labwc.
. There is a clear and compelling answer to the question: what breaks, or
  what important problem remains, if we do not accept it?
. There is a credible prospect of long-term maintenance.
. The potential for scope creep and future feature pressure is understood.
. It does not absorb responsibility that properly belongs to another layer
  of the ecosystem.
. Its maintenance cost is proportionate to its benefit, including the cost
  of handling edge cases, regressions and future changes.
. It does not unnecessarily constrain future design choices.

# CHANGE

These principles are not intended to prevent change.

They are intended to make change deliberate.

labwc should be able to evolve when the evidence says that it should. But
change should normally be driven by correctness, compatibility, coherence and
long-term usefulness rather than novelty, fashion, competition or fear.

The objective is not for labwc to become the most ambitious compositor.

The objective is for it to remain a good one.

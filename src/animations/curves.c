#include "animation.h"
#include <math.h>
#include <stdbool.h>

double
animation_curve_linear_in(double t)
{
	return t;
}

double
animation_curve_linear_out(double t)
{
	return 1.0 - t;
}

/*
 * This is a harmonic oscillator with a step input. See:
 * https://en.wikipedia.org/wiki/Harmonic_oscillator#Step_input
 *
 * The formula is:
 *
 *    x(t) = 1 - exp(-s w t) sin(sqrt(1 - s^2) w t - p) / sin(p)
 *
 * where
 *
 *    cos(p) = s.
 *
 * The settling time is
 *
 *    T = 10 / (s w).
 *
 * With a constant settling time, it is convenient to define either s or w in
 * terms of the other. We'll choose w:
 *
 *    w = 10 / s.
 *
 * Substituting for w in x(t):
 *
 *    x(t) = 1 - exp(-10 t) sin(sqrt(1 - s^2) 10 t / s - p) / sin(p).
 *
 * Because sin(p) = sqrt(1 - cos^2(p)) and cos(p) = s, we can write
 *
 *    sin(p) = +/- sqrt(1 - s^2),
 *
 * thus
 *
 *    x(t) = 1 - exp(-10 t) sin(10 sin(p) t / s - p) / sin(p).
 *
 * We can just as easily choose to substitute the other way around, but it
 * makes no difference. The goal is to precompute as much as possible so that
 * we're only left with the exponent-sine multiple as a function of t.
 */

struct harmonic_oscillator_parameters {
	bool configured;
	double s, p, sin_p;
};

static void
harmonic_oscillator_configure(struct harmonic_oscillator_parameters *p,
		double damping_ratio)
{
	if (p->configured)
		return;
	p->s = damping_ratio;
	p->p = acos(damping_ratio);
	p->sin_p = sin(p->p);
	p->configured = true;
}

static double
harmonic_oscillator(struct harmonic_oscillator_parameters *p, double t)
{
	if (t <= 0) {
		return 0;
	}
	return exp(-10 * t) * sin(10 * p->sin_p * t / p->s + p->p) / p->sin_p;
}

double
animation_curve_elastic_in(double t)
{
	static struct harmonic_oscillator_parameters p;
	harmonic_oscillator_configure(&p, 0.4);
	return 1.0 - harmonic_oscillator(&p, t);
}

double
animation_curve_elastic_out(double t)
{
	static struct harmonic_oscillator_parameters p;
	harmonic_oscillator_configure(&p, 0.4);
	return harmonic_oscillator(&p, t);
}

double
animation_curve_bounce_in(double t)
{
	static struct harmonic_oscillator_parameters p;
	harmonic_oscillator_configure(&p, 0.4);
	return 1.0 - fabs(harmonic_oscillator(&p, t));
}

double
animation_curve_bounce_out(double t)
{
	static struct harmonic_oscillator_parameters p;
	harmonic_oscillator_configure(&p, 0.4);
	return fabs(harmonic_oscillator(&p, t));
}

double
animation_curve_overshoot_in(double t)
{
	static struct harmonic_oscillator_parameters p;
	harmonic_oscillator_configure(&p, 0.65);
	return 1.0 - harmonic_oscillator(&p, t);
}

double
animation_curve_overshoot_out(double t)
{
	static struct harmonic_oscillator_parameters p;
	harmonic_oscillator_configure(&p, 0.65);
	return harmonic_oscillator(&p, t);
}

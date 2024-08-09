# UNIT TESTS

## Introduction

In the bigger scheme a validating that the compositor meets user needs, unit
tests do not contribute a great deal. However, they have a role to play in
providing some verification that stand-alone functions behave as expected.

On this project, writing unit-tests is not compulsory nor do we measure
coverage. The inclusion of the t/ directly does not signifiy a move towards
test-driven development. We intend to use unit tests sparingly and only when
devs find them useful.

## Usage

From toplevel directory, run:

```
make -C t/ && prove
```

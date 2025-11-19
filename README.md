# <img src="./Data/graphics/logo.svg" width="5%" alt="OpenSoar Logo"> OpenSoar

[![.github/workflows/build-native.yml](../../actions/workflows/build-native.yml/badge.svg)](../../actions/workflows/build-native.yml)
[![.github/workflows/build-container.yml](../../actions/workflows/build-container.yml/badge.svg)](../../actions/workflows/build-container.yml)
[![.github/workflows/build-docs.yml](../../actions/workflows/build-docs.yml/badge.svg)](../../actions/workflows/build-docs.yml)

OpenSoar - is an experimental fork of the wellknown gliding software XCSoar.

... and XCSoar is a tactical glide computer for Android, Linux, Mac OS X and Windows.

This file is aimed at developers.  Developers should [read the (XCSoar-)
developer manual](https://xcsoar.readthedocs.io/en/latest/).

Users can refer to the Users' Manual which, for the latest release, can be
downloaded via the [XCSoar home page](https://xcsoar.org/discover/manual.html), because there is no difference yet.

## Getting the source

The OpenSoar source code is managed with git. It can be fetched with the following command:

```bash
git clone --recurse-submodules https://github.com/Opensoaring/OpenSoar
```

To update your repository, use the following command:

```bash
git pull --recurse-submodules
```

For more information, please refer to the [git
documentation](http://git-scm.com/).

## Compiling from source

Please read the current [Developer's
Manual](https://xcsoar.readthedocs.io/en/latest/build.html) for
detailed build instructions.

## Submitting patches

Patches may be submitted using the Developers' mail list or GitHub. Refer to
chapter 3 of the current Developers' Manual for details of how to write and
submit patches upstream.

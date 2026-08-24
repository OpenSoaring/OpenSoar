#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright The XCSoar Project
"""
Generate include/MathTables.h (sine/cosine lookup tables and the thermal
recency table).

Python port of tools/GenerateSineTables.cpp: the C++ generator has to be
compiled and *executed* during the build, which fails on Windows machines
where a Device Guard / Smart App Control policy blocks unsigned, freshly
built executables.  A Python script has no such problem - python.exe is
signed and already used all over the build (resources, version, ANGLE).

The constants mirror src/Math/FastTrig.hpp, src/Math/Constants.hpp and
src/Computer/ThermalRecency.hpp - keep them in sync.

Usage:  generate_math_tables.py <output-file>
        generate_math_tables.py -          (stdout, like the C++ tool)
"""

import math
import sys

# src/Math/Constants.hpp: M_2PI is *not* provided by math.h, the project
# defines this (truncated) literal - use exactly the same value so the
# tables match the runtime's IntAngleToRadians()/NATIVE_TO_INT().
M_2PI = 6.28318530718

# src/Math/FastTrig.hpp
INT_ANGLE_RANGE = 4096
INT_ANGLE_MULT = INT_ANGLE_RANGE / M_2PI

# src/Computer/ThermalRecency.hpp
THERMALRECENCY_SIZE = 60


def int_angle_to_radians(i: int) -> float:
    return i / INT_ANGLE_MULT


def lround(x: float) -> int:
    """C lround(): round half away from zero (Python's round() is
    round-half-to-even)."""
    return int(math.floor(x + 0.5)) if x >= 0 else int(math.ceil(x - 0.5))


def thermal_fn(x: int) -> float:
    return math.exp((-0.2 / THERMALRECENCY_SIZE) * math.pow(x, 1.5))


def write_tables(out) -> None:
    w = out.write
    w("#include <array>\n")

    w("constinit const std::array<double, %u> SINETABLE{\n" % INT_ANGLE_RANGE)
    for i in range(INT_ANGLE_RANGE):
        w("  %.20e,\n" % math.sin(int_angle_to_radians(i)))
    w("};\n")

    w("constinit const std::array<short, %u> ISINETABLE{\n" % INT_ANGLE_RANGE)
    for i in range(INT_ANGLE_RANGE):
        w("  %d,\n" % lround(math.sin(int_angle_to_radians(i)) * 1024))
    w("};\n")

    w("constinit const std::array<double, %u> INVCOSINETABLE{\n"
      % INT_ANGLE_RANGE)
    for i in range(INT_ANGLE_RANGE):
        x = math.cos(int_angle_to_radians(i))
        if 0 <= x < 1.0e-8:
            x = 1.0e-8
        elif -1.0e-8 < x < 0:
            x = -1.0e-8
        w("  %.20e,\n" % (1.0 / x))
    w("};\n")

    w("constinit const std::array<double, %d> THERMALRECENCY{\n"
      % THERMALRECENCY_SIZE)
    for i in range(THERMALRECENCY_SIZE):
        w("  %.20e,\n" % thermal_fn(i))
    w("};\n")


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 1
    if sys.argv[1] == "-":
        write_tables(sys.stdout)
    else:
        # newline='\n': identical output on all platforms
        with open(sys.argv[1], "w", newline="\n") as f:
            write_tables(f)
    return 0


if __name__ == "__main__":
    sys.exit(main())

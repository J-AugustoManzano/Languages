ECMA-55 BASIC [AM-42] - v0.1.11.2609
(c) 2016-2018 (2026 - Revised)
Augusto Manzano - All rights reserved
==============================================


The "ecma55basic" interpreter written in C++ conforms to the
ECMA-55 standard, which specifies the minimum features required for
the BASIC language published around 1965, without any additions beyond
what the standard defines.


This is an educational project, intended for classroom use, with the
objective of serving as an exact reference of what the original BASIC
language was, historically and formally, without the deviations and
extensions that each hardware manufacturer or even language
software vendor added over time.


Project structure
-----------------

ecma55-basic/
├── Makefile    Build (make / make run / make clean)
├── LEIAME.txt  This file in Portuguese
├── README.txt  This file in English
├── src/        Interpreter source code
├── docs/       Language documentation in Portuguese and English
├── examples/   Example programs (100% standard coverage)
└── tests/      Automated tests


Compilation
-----------

Requires a C++17 compiler (g++ or compatible) with "make/mingw32-make".

>>> LINUX

make        - compiles the interpreter
make run    - compiles (if necessary) and opens the interactive environment
make test   - compiles and runs the three built-in test suites
make nbs    - compiles and runs the NBS conformance test suite (tests/nbs/)
make clean  - removes generated files

>>> WINDOWS

mingw32-make        - compiles the interpreter
mingw32-make run    - compiles (if necessary) and opens the interactive environment
mingw32-make test   - compiles and runs the three built-in test suites
mingw32-make nbs    - compiles and runs the NBS conformance test suite (tests/nbs/)
mingw32-make clean  - removes generated files

Execution
---------

./ecma55basic                - opens the interactive environment (REPL), in English
./ecma55basic --pt           - opens the environment in Portuguese
./ecma55basic programa.bas   - executes a program directly, without opening the environment


Documentation and examples
--------------------------

> "docs"      - language manual in English and Portuguese.
> "examples"  - 12 programs from "hello, world" to the "Sieve of Eratosthenes",
                covering every language feature.


Current project status
----------------------

- Project developed between 2016 and 2018 in spare time.
- Forgotten for years in one of several project directories.
- Recovered and revised for publication in 2026.
- Lexer (Layer 1), parser (Layer 2), and interpreter (Layer 3)
  are complete, along with the interactive environment (REPL), fully bilingual
  through "LOCALIZE PT|EN". HELP and ALL error messages from the
  lexer/parser/interpreter follow the selected language. "HELP <command>"
  explains each of the 20 commands and 11 functions (description,
  syntax, example). Running "ecma55basic" without arguments opens the environment where:
  RUN/LIST/NEW/LOAD/SAVE/LOCALIZE/SYSTEM/HELP can be executed
  immediately; with a file, it executes the program directly, and the flags
  "--pt" or "--en" select the initial access language in both modes.
- "make test" runs the three built-in test suites (192 tests: 63 lexer + 91
  parser + 38 interpreter).
- "make nbs" runs the NBS formal conformance test suite (174/205
  tests matching bas55; the remaining 31 tests are 16 RND-dependent tests,
  plus deliberate divergences and numerical precision noise documented in
  "tests/nbs/LEIAME.txt").
- Known limitations: formatting of extreme numbers in PRINT may
  not match the standard character-for-character in rare cases
  (floating-point noise between implementations, not an incorrect algorithm);
  the REPL immediate mode and RUN do not share variables with each other.


Authorship
----------

(c) 2016-2018 (2026 - Revised)
Augusto Manzano
All rights reserved.

# Building OpenZz

The supported build interface is GNU Autotools on POSIX systems, with a C
compiler supporting GNU C11 extensions (GCC or Clang), Make, and a dlopen API.
Native Windows/MSVC is not supported: the runtime also uses POSIX process,
resource and filesystem APIs. macOS and Linux are the initial CI targets.

From a release/source tree containing configure:

```sh
mkdir build
cd build
../configure
make -j4
make check
```

After modifying configure.ac or Makefile.am, regenerate with `./autogen.sh`.
This requires Autoconf, Automake and GNU Libtool (`glibtoolize` on macOS).
Generated configure/Makefile.in files and helper scripts are tracked, so end
users do not need these tools. Use a clean source tree for out-of-tree builds;
run `make distclean` first if it was previously configured in-tree.

Readline is auto-detected, including its headers and terminal dependencies.
Use `--without-readline` for the standard-input fallback or `--with-readline`
to require it. Supply CPPFLAGS/LDFLAGS for libraries in nonstandard locations.
Use `--disable-shared` for static libraries; the dynamic module test then skips
explicitly. Test modules are built only by `make check`, never installed.

`make check` reports individual tests and propagates failures. Each language
test runs in an isolated temporary directory; dynamic module names come from
Libtool, not a hard-coded .so/.dylib suffix. `make zztest` in testsuite and
`./test.sh` from a configured root remain aliases. `build.sh` runs configure
and make in the current directory and accepts configure arguments.

For a staged installation and packaging validation:

```sh
make install DESTDIR="$PWD/stage"
make distcheck
```

For diagnostic builds, pass sanitizer options in both CFLAGS and LDFLAGS.
The legacy runtime has known undefined behavior and process-lifetime memory;
a successful ordinary suite is not a sanitizer-clean or thread-safety claim.

GNU C11 is selected explicitly: the legacy callback ABI is incompatible with
C23. This is a compatibility measure, not a completed modernization of that ABI.

## Experimental checked profile

`ozz --checked source.zz` selects the isolated numerical DSL. Use
`ozz --checked --emit-c source.zz` for standalone C11 output. The ordinary command
line and legacy API keep their previous behavior. See `docs/CHECKED_PROFILE.md`
for implemented directives, transaction contracts, embedding and explicit limits.

The CI matrix also exercises Linux arm64 and macOS Intel, and separately compiles
the checked profile with strict C11, ASan, UBSan and Linux leak detection. Runner
labels follow the GitHub-hosted runner reference:
https://docs.github.com/en/actions/reference/runners/github-hosted-runners

## ZZPy source-to-source experiment

With Python 3.9+ and a built `ozz`, `tools/zzpy/zzpy.py` translates the explicitly
limited Python dialect described in `docs/ZZPY_PROTOTIPO.md`. The host grammar
and imported syntax rules are parsed by the legacy ZZ engine in a fresh process;
this does not extend the separate `--checked` i64 parser. No external Python
packages are needed. `configure` detects Python optionally; `make check` skips
only the ZZPy test when Python 3.9+ is unavailable. Ordinary C builds do not
require Python. Example modules and generated Python are in `examples/zzpy/`.

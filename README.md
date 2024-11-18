<img src="/docs/nodes-logo-full.png" width=35% height=35%>

# NODES

NODES is developed by the [*System Tools and Advanced Runtimes (STAR)*](https://www.bsc.es/discover-bsc/organisation/scientific-structure/system-tools-and-advanced-runtimes) group at the [**Barcelona Supercomputing Center**](http://www.bsc.es/). NODES -- short for nOS-V-based Dependency System -- is a runtime library designed to work on top of the nOS-V runtime while providing most of the functionalities from its predecessor, Nanos6. NODES acts as a dependency system for nOS-V by synchronizing the data flow of codes annotated with OmpSs-2 directives and bridges the gap between nOS-V and user applications through these annotations. Simultaneously, nOS-V manages the life cycle of multiple generic tasks from various applications or libraries such as NODES and handles their interaction with the system.

## Licensing

The NODES (nOS-V based OmpSs-2 DEpendency System) Runtime Library is
Free Software, licensed under the clauses of the GNU GPL v3 License
included in the [COPYING](COPYING) file. The copyright of the files
included in this package belongs to the Barcelona Supercomputing
Center, unless otherwise stated.

## Installation

### Build requirements

The following software is required to build and install NODES:

1. automake, autoconf, libtool, pkg-config, make, and a C++17 compiler
1. [boost](http://boost.org) >= 1.71
1. [nOS-V](https://github.com/bsc-pm/nos-v)

### Optional libraries or tools

Additionally, NODES is prepared to use the following optional libraries:

1. [ovni](https://ovni.readthedocs.io/en/master/) to instrument and generate execution traces for offline performance analysis with paraver. Minimum version required 1.5.0
1. A C++20 compiler to enable further functionalities

### Build procedure

When cloning from a repository, the building environment must be prepared through the following command:

```sh
$ autoreconf -fiv
```

When the code is distributed through a tarball, it usually does not need that command.

Then execute the following commands:

```sh
./configure --prefix=$INSTALL_PATH_NODES \
    --with-nosv=$INSTALL_PATH_NOSV       \
    --with-boost=$INSTALL_PATH_BOOST     \
    --with-ovni=$INSTALL_PATH_OVNI       \
    ...other options...

make all
make install
```

where `$INSTALL_PATH_NODES` is the directory into which to install NODES.

The configure script also accepts the following options:

1. `--with-nosv` to specify the prefix of the nOS-V installation
1. `--with-boost` to specify the prefix of the boost installation
1. `--with-ovni` to specify the prefix of the ovni installation (Optional)
1. `--with-nodes-clang` to specify the prefix of a CLANG installation with NODES support (Optional)

## Contributing

The development of NODES requires contributors to follow these few simple guidelines:

1. C++17
1. K&R indentation style
1. Camel case coding style

## Instrumenting with ovni

For ovni to work properly in NODES, the used nOS-V installation must be configured with ovni support as well, with version 1.5.0 minimum.
Once both nOS-V and NODES are configured with ovni support, nOS-V has to enable its own instrumentation. At the time of writing, this is done as follows:

```sh
$ export NOSV_CONFIG_OVERRIDE="instrumentation.version=ovni"
```

By default, in NODES the `ovni` instrumentation is disabled, even when compiling with ovni support. To
enable instrumenting executions with ovni, set the `NODES_OVNI` environment variable to `1`:

```sh
$ export NODES_OVNI=1
```

## Coroutine Support

NODES supports the use of Coroutines provided that a compiler with C++20 support is used.
To compile with Coroutine support, the `-fcoroutines` flag must be passed, as shown in the example below:

```
clang++ -std=c++20 -fcoroutines -o test-coroutines.bin test-coroutines.cpp
```

For more detailed examples on the usage of Coroutines, check our correctness tests in the `tests/correctness/coroutine` subdirectory.


## Known Limitations

NODES supports most of the features found in the Nanos6 runtime. However, at the moment, it does not support the following:
1. Linear-region dependency system
1. Assert directive

Furthermore, the instrumentation provided differs in the sense that it only provides (1) entry-exit points instrumentation, and (2) instrumentation related to the dependency system. Thus, instrumentation variants such as `profile`, `graph`, and the linter (`lint`) are not available in NODES.

# NODES Library

The NODES Library is developed by the [*System Tools and Advanced Runtimes (STAR)*](https://www.bsc.es/discover-bsc/organisation/scientific-structure/system-tools-and-advanced-runtimes) group
at the [**Barcelona Supercomputing Center**](http://www.bsc.es/).

## Licensing

The NODES (nOS-V based OmpSs-2 DEpendency System) Runtime Library is
Free Software, licensed under the clauses of the GNU GPL v3 License
included in the [COPYING](COPYING) file. The copyright of the files
included in this package belongs to the Barcelona Supercomputing
Center, unless otherwise stated.

## Installation

### Build requirements

The following software is required to build and install NODES:

1. automake, autoconf, libtool, pkg-config, make, and a C++11 compiler
1. [boost](http://boost.org) >= 1.71
1. [nOS-V](https://github.com/bsc-pm/nos-v)

### Optional libraries or tools

Additionally, NODES is prepared to use the following optional libraries:

1. [ovni](https://ovni.readthedocs.io/en/master/) to instrument and generate execution traces for offline performance analysis with paraver. Minimum version required 1.5.0

### Build procedure

When cloning from a repository, the building environment must be prepared through the following command:

```sh
$ autoreconf -fiv
```

When the code is distributed through a tarball, it usually does not need that command.

Then execute the following commands:

```sh
$ ./configure --prefix=INSTALLATION_PREFIX \
$   --with-nosv=NOSV_INSTALL_PATH          \
$   --with-boost=BOOST_INSTALL_PATH        \
$   ...other options...
$ make all
$ make install
```

where `INSTALLATION_PREFIX` is the directory into which to install NODES.

The configure script also accepts the following options:

1. `--with-nosv` to specify the prefix of the nOS-V installation
1. `--with-boost` to specify the prefix of the boost installation
1. `--with-ovni` to specify the prefix of the ovni installation (Optional)
1. `--with-nodes-clang` to specify the prefix of a CLANG installation with NODES support (Optional)

## Contributing

The development of NODES requires contributors to follow these few simple guidelines:

1. C++11
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

## Features and Known Limitations

NODES supports most of the features found in the Nanos6 runtime. However, at the moment, it does not support the following:
1. Linear-region dependency system
1. Assert directive

Furthermore, the instrumentation provided differs in the sense that it only provides (1) entry-exit points instrumentation, and (2) instrumentation related to the dependency system. Thus, instrumentation variants such as `profile`, `graph`, and the linter (`lint`) are not available in NODES.

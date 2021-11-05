# Nanos6-Lite Library

## Licensing

The Nanos6-Lite Library is Free Software, licensed under the clauses of the
GNU GPL v3 License included in the [COPYING](COPYING) file.
The copyright of the files included in this package belongs to the
Barcelona Supercomputing Center, unless otherwise stated.

## Installation

### Build requirements

The following software is required to build and install Nanos6-Lite:

1. automake, autoconf, libtool, pkg-config, make and a C++11 compiler

### Build procedure

When cloning from a repository, the building environment must be prepared through the following command:

```sh
$ autoreconf -fiv
```

When the code is distributed through a tarball, it usually does not need that command.

Then execute the following commands:

```sh
$ ./configure --prefix=INSTALLATION_PREFIX ...other options...
$ make all
$ make install
```

where `INSTALLATION_PREFIX` is the directory into which to install Nanos6-Lite.

## Contributing

The development of Nanos6-Lite requires contributors to follow these few simple guidelines:

1. C++11
1. K&R indentation style
1. Camel case coding style

## Features and Known Limitations

Nanos6-Lite supports most of the features found in the Nanos6 runtime. However, at the moment, it does not support the following:
1. Linear-region dependency system
1. Taskfor construct
1. Assert directive

Furthermore, the instrumentation provided differs in the sense that it only provides (1) entry-exit points instrumentation, and (2) instrumentation related to the dependency system. Thus, instrumentation variants such as `profile`, `graph`, and the linter (`lint`) are not available in Nanos6-Lite.

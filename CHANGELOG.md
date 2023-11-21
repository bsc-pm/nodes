# NODES Release Notes
All notable changes to this project will be documented in this file.

## Version 1.0.1, Wed Jul 25, 2023
The 1.0.1 version of NODES introduces minor patches regarding documentation and incorrect parameters when using external libraries.

### General
- Improve the `ovni` usage documentation within the README.
- Fix several minor errors regarding the ovni instrumentation and the tags used in nOS-V API calls.


## Version 1.0, Wed May 28, 2023
The 1.0 release corresponds to the OmpSs-2 2023.05 release. It is the first release of NODES, and implements the base infrastructure of the runtime, which is built on top of nOS-V. This runtime implements the `taskiter` construct and leverages directed task graphs (DCTG) to optimize the execution of iterative applications.


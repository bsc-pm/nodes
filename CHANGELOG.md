# NODES Release Notes
All notable changes to this project will be documented in this file.

## Version 1.2.0, Wed May 15, 2024
The 1.2.0 version of NODES introduces several bug fixes and general improvements related to the nOS-V, ALPI, and ovni API integrations.

### General
- Use proper nOS-V labels, depending on the API's current call.
- Improve error-handling of nOS-V return codes.
- Improve ovni instrumentation through various fixes.
- Use `ovni_thread_require` to request NODES model.
- Require ovni version 1.5.0 or greater.

### Bug Fixes
- Fix non-descriptive task labels in ovni instrumentation by generating placeholders.
- Fix a priority-clause related bug that caused executions to end abruptly.
- Fix (through a temporary TLS solution) ALPI interoperability issues.


## Version 1.1.0, Wed Nov 22, 2023
The 1.1.0 version of NODES introduces a major refactor of the `taskiter` construct along with many fixes for issues regarding the same construct and performance improvements.

### General
- Refactor of the implementation of the taskiter feature, focused on fixing issues with the past implementation and improving its performance.
- Use the updated attach/detach from nOS-V 2.0.
- Drop support for nOS-V versions older than 2.0.
- Improve and simplify the configuration step.

### Bug Fixes
- Fix the implementation of the taskiter through a major refactor.
- Fix issues regarding the order of linkage with several libraries by reordering nOS-V to be a first level dependency.
- Avoid neglecting custom user compilation flags when configuring.
- Fix several issues with the discovery of the ovni library.


## Version 1.0.1, Wed Jul 25, 2023
The 1.0.1 version of NODES introduces minor patches regarding documentation and incorrect parameters when using external libraries.

### General
- Improve the `ovni` usage documentation within the README.
- Fix several minor errors regarding the ovni instrumentation and the tags used in nOS-V API calls.


## Version 1.0, Wed May 28, 2023
The 1.0 release corresponds to the OmpSs-2 2023.05 release. It is the first release of NODES, and implements the base infrastructure of the runtime, which is built on top of nOS-V. This runtime implements the `taskiter` construct and leverages directed task graphs (DCTG) to optimize the execution of iterative applications.


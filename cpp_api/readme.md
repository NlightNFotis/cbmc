# The CPROVER C++ API

This folder includes the interface and the implementation of a new C++-based
API for the CProver libraries. The aim is for this to be able to be used by
clients to drive all of the verification pipeline without the need to invoke
any of the associated tools.

## Implementation

The major part of the implementation of the API is in [`api.cpp`](api.cpp).
To use the API as it stands, you need to include the header [`api.h`](api.h)
in your program, and link against the resultant shared-object from the CProver
build process (`libcprover-api.a`).

## Example

An example for driving the API in its current form is in the file [`call_bmc.cpp`](call_bmc.cpp),
which we are also using as a binary driver for testing the API.

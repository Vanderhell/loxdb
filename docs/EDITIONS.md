# loxdb edition

This repository contains the MIT-licensed `loxdb` embedded database core.

The core provides:

- bounded KV, time-series, and relational engines;
- the storage HAL, WAL, recovery, and compaction;
- predictable core memory behavior and the `lox_err_t` contract;
- supported ports, adapters, tests, and verification tooling shipped here.

The public headers and source in this repository define the supported surface.
No claim is made here about unshipped editions, future repositories, or future
licensing.

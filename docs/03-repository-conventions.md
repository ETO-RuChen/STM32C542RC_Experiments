# Repository Conventions

This is one Git repository containing independent labs, reusable modules, documentation, and boot/update work. A lab may be independently built and verified; shared code belongs under platform, drivers, middleware, services, rtos, or osal only after it meets the reuse criteria.

## Layer direction

`vendor -> platform -> drivers -> middleware/services -> rtos/osal -> apps/labs`

A lower layer must not depend on an application or a specific lab. Board pin mapping belongs to the board/platform layer.

## Reuse gate

Extract code from a lab only when it has a stable interface, documented dependencies, a minimal example, and an automated test or recorded board verification. Keep one-off demonstrations in the lab.

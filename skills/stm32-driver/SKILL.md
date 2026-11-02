---
name: stm32-driver
description: Design, extract, review, and test reusable STM32C5 drivers while keeping board policy and application behavior out of the driver.
---

Keep vendor code, platform/board mapping, driver API/implementation, and application policy separate. A reusable module needs a documented interface, dependencies, lifecycle and blocking semantics, a minimal example, and an automated test or recorded board verification. Do not copy lab globals into a shared driver.

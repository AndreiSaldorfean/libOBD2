#ifndef SYSCALLS_H
#define SYSCALLS_H

/* =============================================== MODULE API ============================================== */

/**
 * @brief Initialize syscall resources (must be called before vTaskStartScheduler).
 *        Creates the printf mutex used to serialize concurrent CDC writes.
 */
void syscalls_init(void);

#endif /* SYSCALLS_H */

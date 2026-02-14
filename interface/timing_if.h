#ifndef TIMING_IF_H
#define TIMING_IF_H

/*************************************************************************************************************
 * This header file provides the timing interface required for protocol timing requirements.
 * The integrator must provide implementations for their specific platform:
 *   - Bare metal: Hardware timers, busy-wait loops
 *   - RTOS: vTaskDelay, xTaskGetTickCount, software timers
 *   - POSIX: usleep, clock_gettime, timer_create
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "srv_status.h"
#include <stdint.h>
#include <stdbool.h>

/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */

/**
 * @brief Callback function type for timer expiration
 * @param pUserData User-provided context data
 */
typedef void (*timing_callback_t)(void *pUserData);

/**
 * @brief Timing operations interface
 *
 * The integrator must provide implementations for these functions.
 * All time values are in milliseconds unless otherwise specified.
 */
typedef struct
{
    /**
     * @brief Get current timestamp in milliseconds
     * @param pHandle Platform-specific handle (e.g., timer peripheral)
     * @return Current time in milliseconds (can wrap around)
     */
    uint32_t (*get_time_ms)(void *pHandle);

    /**
     * @brief Blocking delay
     * @param pHandle Platform-specific handle
     * @param delay_ms Delay duration in milliseconds
     * @return OBD_STATUS_OK on success
     *
     * Implementation examples:
     *   - Bare metal: busy-wait or hardware timer
     *   - FreeRTOS: vTaskDelay(pdMS_TO_TICKS(delay_ms))
     *   - POSIX: usleep(delay_ms * 1000)
     */
    obd_status_t (*delay_ms)(void *pHandle, uint32_t delay_ms);

    /**
     * @brief Start a one-shot timeout timer
     * @param pHandle Platform-specific handle
     * @param timeout_ms Timeout duration in milliseconds
     * @param callback Function to call on timeout (can be NULL for polling)
     * @param pUserData User data passed to callback
     * @return OBD_STATUS_OK on success
     *
     * If callback is NULL, use is_timeout_expired() to poll.
     */
    obd_status_t (*start_timeout)(void *pHandle, uint32_t timeout_ms,
                                  timing_callback_t callback, void *pUserData);

    /**
     * @brief Stop/cancel the current timeout
     * @param pHandle Platform-specific handle
     * @return OBD_STATUS_OK on success
     */
    obd_status_t (*stop_timeout)(void *pHandle);

    /**
     * @brief Check if timeout has expired (polling mode)
     * @param pHandle Platform-specific handle
     * @return true if timeout expired, false otherwise
     */
    bool (*is_timeout_expired)(void *pHandle);

} obd_timing_ops_t;

/* ============================================ INLINE FUNCTIONS =========================================== */

/**
 * @brief Helper to calculate elapsed time handling wrap-around
 * @param start Start timestamp
 * @param now Current timestamp
 * @return Elapsed time in milliseconds
 */
static inline uint32_t timing_elapsed_ms(uint32_t start, uint32_t now)
{
    return (now >= start) ? (now - start) : (UINT32_MAX - start + now + 1);
}

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */

#endif /* TIMING_IF_H */

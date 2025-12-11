/**
 * PCILeech FPGA DNA Activator - LeechCore API Wrapper Header
 *
 * Function: Wrap LeechCore API calls, provide register 24-31 operation interface
 * Author: PCILeech Project
 * Date: 2025-11-28 (Updated for simplified register architecture)
 */

#ifndef LEECH_API_H
#define LEECH_API_H

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include "leechcore.h"

// Custom command definitions (must match device_fpga.c)
#define LC_CMD_FPGA_CUSTOM_READ         0x0200000000000000
#define LC_CMD_FPGA_CUSTOM_WRITE        0x0201000000000000

// Macros for multi-register access
#define LC_CMD_FPGA_CUSTOM_READ_REG(n)  (LC_CMD_FPGA_CUSTOM_READ | ((n) & 0xFF))
#define LC_CMD_FPGA_CUSTOM_WRITE_REG(n) (LC_CMD_FPGA_CUSTOM_WRITE | ((n) & 0xFF))

// Register function definitions (Simplified Architecture: 24-31)
#define REG_DNA_LOW         24      // DNA value low 32 bits
#define REG_DNA_HIGH        25      // DNA value high 25 bits
#define REG_ENCRYPTED_VALUE 26      // Encrypted random value (FPGA-generated)
#define REG_DECRYPTED_RESULT 27     // Decrypted result (software-written)
#define REG_VERIFY_STATUS   28      // Verification status (0=fail, 1=success)
#define REG_TLP_CONTROL     29      // TLP control (0=disabled, 1=enabled)
#define REG_CONTROL_CMD     30      // Control command (1=start verification)
#define REG_SYSTEM_STATUS   31      // System status (0=idle, 1=processing, 2=complete)

// System status value definitions
#define STATUS_IDLE         0       // Idle status
#define STATUS_PROCESSING   1       // Processing
#define STATUS_COMPLETED    2       // Completed
#define STATUS_ERROR        3       // Error

// Verification status value definitions
#define VERIFY_FAILED       0       // Verification failed
#define VERIFY_SUCCESS      1       // Verification success

// TLP control value definitions
#define TLP_DISABLED        0       // Disable TLP return
#define TLP_ENABLED         1       // Enable TLP return

// Control command value definitions
#define CMD_IDLE            0       // Idle command
#define CMD_START_VERIFY    1       // Start verification process

// Timeout settings
#define DEFAULT_TIMEOUT_MS  5000    // Default timeout (milliseconds)
#define POLL_INTERVAL_MS    10      // Polling interval (milliseconds)

/**
 * Initialize LeechCore connection
 * @param device_name Device name (e.g., "fpga")
 * @return LeechCore handle, returns NULL if failed
 */
HANDLE init_leechcore(const char* device_name);

/**
 * Close LeechCore connection and cleanup resources
 * @param hLC LeechCore handle
 */
void cleanup_leechcore(HANDLE hLC);

/**
 * Read specified register value
 * @param hLC     LeechCore handle
 * @param reg_num Register number (24-31 for DNA verification)
 * @return Register value, returns 0 if failed
 */
uint32_t read_register(HANDLE hLC, int reg_num);

/**
 * Write specified register value
 * @param hLC     LeechCore handle
 * @param reg_num Register number (24-31 for DNA verification)
 * @param value   Value to write
 * @return true=success, false=failed
 */
bool write_register(HANDLE hLC, int reg_num, uint32_t value);

/**
 * Read complete DNA value (register 24+25)
 * @param hLC LeechCore handle
 * @return 57-bit DNA value, returns 0 if failed
 */
uint64_t read_dna_value(HANDLE hLC);

/**
 * Start verification process (write register 30=1)
 * @param hLC LeechCore handle
 * @return true=success, false=failed
 */
bool start_verification(HANDLE hLC);

/**
 * Read encrypted random value (register 26)
 * @param hLC LeechCore handle
 * @return Encrypted random value, returns 0 if failed
 */
uint32_t read_encrypted_value(HANDLE hLC);

/**
 * Write decrypted result (register 27)
 * @param hLC   LeechCore handle
 * @param value Decrypted value
 * @return true=success, false=failed
 */
bool write_decrypted_result(HANDLE hLC, uint32_t value);

/**
 * Check verification status (register 28)
 * @param hLC LeechCore handle
 * @return Verification status value, returns -1 if failed
 */
int check_verification_status(HANDLE hLC);

/**
 * Check TLP control status (register 29)
 * @param hLC LeechCore handle
 * @return TLP control status value, returns -1 if failed
 */
int check_tlp_control_status(HANDLE hLC);

/**
 * Check system status (register 31)
 * @param hLC LeechCore handle
 * @return System status value, returns -1 if failed
 */
int check_system_status(HANDLE hLC);

/**
 * Wait for firmware processing completion
 * @param hLC        LeechCore handle
 * @param timeout_ms Timeout in milliseconds
 * @return true=processing completed, false=timeout or error
 */
bool wait_for_firmware_ready(HANDLE hLC, int timeout_ms);

/**
 * Wait for verification completion
 * @param hLC        LeechCore handle
 * @param timeout_ms Timeout in milliseconds
 * @return true=verification completed, false=timeout or error
 */
bool wait_for_verification_complete(HANDLE hLC, int timeout_ms);

/**
 * Reset verification process
 * @param hLC LeechCore handle
 * @return true=success, false=failed
 */
bool reset_verification(HANDLE hLC);

/**
 * Get detailed error information
 * @return Error description string
 */
const char* get_last_error_string(void);

/**
 * Print register status (for debugging)
 * @param hLC LeechCore handle
 */
void print_register_status(HANDLE hLC);

#endif // LEECH_API_H
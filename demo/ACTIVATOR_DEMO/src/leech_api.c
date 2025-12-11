/*
 * PCILeech FPGA DNA Activator - LeechCore API Wrapper Implementation
 *
 * Function: Wrapper for LeechCore API calls, provides register 24-31 operation interface
 * Author: PCILeech Project
 * Date: 2025-11-28 (Updated for simplified register architecture)
 */

#include "leech_api.h"
#include "dna_crypto.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

// Custom register macros are now defined in leech_api.h

// Global error information
static char g_last_error[256] = {0};

// Internal function declarations
static void set_last_error(const char* format, ...);
static bool is_valid_register(int reg_num);

/**
 * Initialize LeechCore connection
 */
HANDLE init_leechcore(const char* device_name)
{
    if (!device_name) {
        set_last_error("Device name cannot be empty");
        return NULL;
    }
    
    printf("Initializing LeechCore connection...\n");
    printf("Device name: %s\n", device_name);
    
    // Configure LeechCore
    PLC_CONFIG_ERRORINFO pErrorInfo = NULL;
    LC_CONFIG Config = {0};
    
    // Set basic configuration
    Config.dwVersion = LC_CONFIG_VERSION;
    strcpy_s(Config.szDevice, sizeof(Config.szDevice), device_name);
    Config.fVolatile = FALSE;
    Config.fWritable = TRUE;
    
    // Create LeechCore handle
    HANDLE hLC = LcCreateEx(&Config, &pErrorInfo);
    
    if (!hLC) {
        if (pErrorInfo) {
            set_last_error("Initialization failed: %ws", pErrorInfo->wszUserText ? pErrorInfo->wszUserText : L"Unknown error");
            LocalFree(pErrorInfo);
        } else {
            set_last_error("Initialization failed: Unknown error");
        }
        return NULL;
    }
    
    printf("LeechCore connection successful!\n");
    return hLC;
}

/**
 * Close LeechCore connection and cleanup resources
 */
void cleanup_leechcore(HANDLE hLC)
{
    if (hLC) {
        printf("Closing LeechCore connection...\n");
        LcClose(hLC);
        printf("LeechCore connection closed\n");
    }
}

/**
 * Read specified register value
 */
uint32_t read_register(HANDLE hLC, int reg_num)
{
    if (!hLC) {
        set_last_error("LeechCore handle invalid");
        return 0;
    }
    
    if (!is_valid_register(reg_num)) {
        set_last_error("Invalid register number: %d", reg_num);
        return 0;
    }
    
    PBYTE pbDataOut = NULL;
    DWORD cbDataOut = 0;
    
    // Use custom register read macro
    QWORD cmd = LC_CMD_FPGA_CUSTOM_READ_REG(reg_num);
    
    BOOL result = LcCommand(hLC, cmd, 0, NULL, &pbDataOut, &cbDataOut);
    
    if (!result || !pbDataOut || cbDataOut != sizeof(DWORD)) {
        set_last_error("Failed to read register %d", reg_num);
        if (pbDataOut) LocalFree(pbDataOut);
        return 0;
    }
    
    uint32_t value = *(DWORD*)pbDataOut;
    LocalFree(pbDataOut);
    
    return value;
}

/**
 * Write specified register value
 */
bool write_register(HANDLE hLC, int reg_num, uint32_t value)
{
    if (!hLC) {
        set_last_error("LeechCore handle invalid");
        return false;
    }
    
    if (!is_valid_register(reg_num)) {
        set_last_error("Invalid register number: %d", reg_num);
        return false;
    }
    
    // Use custom register write macro
    QWORD cmd = LC_CMD_FPGA_CUSTOM_WRITE_REG(reg_num);
    
    DWORD data = value;
    BOOL result = LcCommand(hLC, cmd, sizeof(DWORD), (PBYTE)&data, NULL, NULL);
    
    if (!result) {
        set_last_error("Failed to write register %d", reg_num);
        return false;
    }
    
    return true;
}

/**
 * Read complete DNA value (register 24+25)
 */
uint64_t read_dna_value(HANDLE hLC)
{
    if (!hLC) {
        set_last_error("LeechCore handle invalid");
        return 0;
    }
    
    printf("Reading FPGA DNA value...\n");
    
    // Read DNA low 32 bits
    uint32_t dna_low = read_register(hLC, REG_DNA_LOW);
    
    // Read DNA high 25 bits
    uint32_t dna_high = read_register(hLC, REG_DNA_HIGH);
    
    // Combine complete DNA value
    uint64_t dna_value = combine_dna_parts(dna_low, dna_high);
    
    printf("DNA low 32 bits: 0x%08X\n", dna_low);
    printf("DNA high 25 bits: 0x%07X\n", dna_high & DNA_HIGH_MASK);
    printf("Complete DNA value: 0x%016llX\n", dna_value);
    
    if (!is_dna_valid(dna_value)) {
        set_last_error("Read DNA value is invalid");
        return 0;
    }
    
    return dna_value;
}

/**
 * Start verification process (write register 30=1)
 */
bool start_verification(HANDLE hLC)
{
    if (!hLC) {
        set_last_error("LeechCore handle invalid");
        return false;
    }
    
    printf("Starting verification process...\n");
    
    // Clear previous states first
    write_register(hLC, REG_SYSTEM_STATUS, STATUS_IDLE);
    write_register(hLC, REG_VERIFY_STATUS, VERIFY_FAILED);
    write_register(hLC, REG_TLP_CONTROL, TLP_DISABLED);
    write_register(hLC, REG_DECRYPTED_RESULT, 0);
    
    // Send start verification command
    bool result = write_register(hLC, REG_CONTROL_CMD, CMD_START_VERIFY);
    
    if (result) {
        printf("Verification process started successfully\n");
    } else {
        printf("Failed to start verification process\n");
    }
    
    return result;
}

/**
 * Read encrypted random value (register 26)
 */
uint32_t read_encrypted_value(HANDLE hLC)
{
    if (!hLC) {
        set_last_error("LeechCore handle invalid");
        return 0;
    }
    
    uint32_t value = read_register(hLC, REG_ENCRYPTED_VALUE);
    printf("Read encrypted value: 0x%08X\n", value);
    return value;
}

/**
 * Write decrypted result (register 27)
 */
bool write_decrypted_result(HANDLE hLC, uint32_t value)
{
    if (!hLC) {
        set_last_error("LeechCore handle invalid");
        return false;
    }
    
    printf("Writing decrypted result: 0x%08X\n", value);
    return write_register(hLC, REG_DECRYPTED_RESULT, value);
}

/**
 * Check verification status (register 28)
 */
int check_verification_status(HANDLE hLC)
{
    if (!hLC) {
        set_last_error("LeechCore handle invalid");
        return -1;
    }
    
    uint32_t status = read_register(hLC, REG_VERIFY_STATUS);
    return (int)status;
}

/**
 * Check TLP control status (register 29)
 */
int check_tlp_control_status(HANDLE hLC)
{
    if (!hLC) {
        set_last_error("LeechCore handle invalid");
        return -1;
    }
    
    uint32_t status = read_register(hLC, REG_TLP_CONTROL);
    return (int)status;
}

/**
 * Check system status (register 31)
 */
int check_system_status(HANDLE hLC)
{
    if (!hLC) {
        set_last_error("LeechCore handle invalid");
        return -1;
    }
    
    uint32_t status = read_register(hLC, REG_SYSTEM_STATUS);
    return (int)status;
}

/**
 * Wait for firmware processing completion
 */
bool wait_for_firmware_ready(HANDLE hLC, int timeout_ms)
{
    if (!hLC) {
        set_last_error("LeechCore handle invalid");
        return false;
    }
    
    printf("Waiting for firmware processing completion (timeout: %d ms)...\n", timeout_ms);
    
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        int status = check_system_status(hLC);
        
        if (status == STATUS_COMPLETED) {
            printf("Firmware processing completed\n");
            return true;
        }
        
        if (status == STATUS_ERROR) {
            set_last_error("Firmware processing error");
            return false;
        }
        
        if (status == -1) {
            set_last_error("Cannot read firmware status");
            return false;
        }
        
        Sleep(POLL_INTERVAL_MS);
        elapsed += POLL_INTERVAL_MS;
        
        // Show progress every second
        if (elapsed % 1000 == 0) {
            printf("Waiting... (%d/%d seconds)\n", elapsed/1000, timeout_ms/1000);
        }
    }
    
    set_last_error("Firmware processing timeout");
    return false;
}

/**
 * Wait for verification completion
 */
bool wait_for_verification_complete(HANDLE hLC, int timeout_ms)
{
    if (!hLC) {
        set_last_error("LeechCore handle invalid");
        return false;
    }
    
    printf("Waiting for verification completion (timeout: %d ms)...\n", timeout_ms);
    
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        int verify_status = check_verification_status(hLC);
        
        if (verify_status == VERIFY_SUCCESS) {
            printf("Verification successful!\n");
            return true;
        }
        
        if (verify_status == -1) {
            set_last_error("Cannot read verification status");
            return false;
        }
        
        Sleep(POLL_INTERVAL_MS);
        elapsed += POLL_INTERVAL_MS;
    }
    
    set_last_error("Verification completion timeout");
    return false;
}

/**
 * Reset verification process
 */
bool reset_verification(HANDLE hLC)
{
    if (!hLC) {
        set_last_error("LeechCore handle invalid");
        return false;
    }
    
    printf("Resetting verification process...\n");
    
    // Clear all status registers
    bool success = true;
    success &= write_register(hLC, REG_CONTROL_CMD, CMD_IDLE);
    success &= write_register(hLC, REG_SYSTEM_STATUS, STATUS_IDLE);
    success &= write_register(hLC, REG_VERIFY_STATUS, VERIFY_FAILED);
    success &= write_register(hLC, REG_TLP_CONTROL, TLP_DISABLED);
    success &= write_register(hLC, REG_ENCRYPTED_VALUE, 0);
    success &= write_register(hLC, REG_DECRYPTED_RESULT, 0);
    
    if (success) {
        printf("Verification process reset successfully\n");
    } else {
        printf("Failed to reset verification process\n");
    }
    
    return success;
}

/**
 * Get detailed error information
 */
const char* get_last_error_string(void)
{
    return g_last_error;
}

/**
 * Print register status (for debugging)
 */
void print_register_status(HANDLE hLC)
{
    if (!hLC) {
        printf("ERROR: LeechCore handle invalid\n");
        return;
    }
    
    printf("\n=== Register Status ===\n");
    printf("Register %d (DNA low 32 bits): 0x%08X\n", REG_DNA_LOW, read_register(hLC, REG_DNA_LOW));
    printf("Register %d (DNA high 25 bits): 0x%08X\n", REG_DNA_HIGH, read_register(hLC, REG_DNA_HIGH));
    printf("Register %d (Encrypted random): 0x%08X\n", REG_ENCRYPTED_VALUE, read_register(hLC, REG_ENCRYPTED_VALUE));
    printf("Register %d (Decrypted result): 0x%08X\n", REG_DECRYPTED_RESULT, read_register(hLC, REG_DECRYPTED_RESULT));
    printf("Register %d (Verification status): %d\n", REG_VERIFY_STATUS, read_register(hLC, REG_VERIFY_STATUS));
    printf("Register %d (TLP control): %d\n", REG_TLP_CONTROL, read_register(hLC, REG_TLP_CONTROL));
    printf("Register %d (Control command): %d\n", REG_CONTROL_CMD, read_register(hLC, REG_CONTROL_CMD));
    printf("Register %d (System status): %d\n", REG_SYSTEM_STATUS, read_register(hLC, REG_SYSTEM_STATUS));
    printf("=======================\n\n");
}

// ==================== Internal Helper Functions ====================

/**
 * Set error information
 */
static void set_last_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_last_error, sizeof(g_last_error), format, args);
    va_end(args);
    
    // Also output to console
    printf("ERROR: %s\n", g_last_error);
}

/**
 * Verify if register number is valid (DNA registers: 24-31)
 */
static bool is_valid_register(int reg_num)
{
    return (reg_num >= REG_DNA_LOW && reg_num <= REG_SYSTEM_STATUS);
}
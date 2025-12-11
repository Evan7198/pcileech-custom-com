/**
 * PCILeech FPGA DNA Activator - DEMO Program
 *
 * Function: Demonstrates how to use LeechCore API for FPGA DNA verification and activation
 * Author: PCILeech Project
 * Date: 2025-11-28
 * Version: v3.0 (Simplified DEMO)
 *
 * This program demonstrates:
 * 1. Connecting to FPGA device
 * 2. Reading FPGA hardware DNA value (57-bit unique identifier)
 * 3. Executing DNA verification workflow
 * 4. Enabling TLP access control
 * 5. Verifying activation status
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "leech_api.h"
#include "dna_crypto.h"

// Program information
#define PROGRAM_NAME    "PCILeech FPGA DNA Activator"
#define PROGRAM_VERSION "v3.0 (Simplified DEMO)"
#define PROGRAM_DATE    "2025-11-28"

// DNA verification register constants (consistent with leech_api.h)
#define DNA_REG_LOW          REG_DNA_LOW          // 24: DNA value lower 32 bits
#define DNA_REG_HIGH         REG_DNA_HIGH         // 25: DNA value upper 25 bits
#define DNA_REG_ENCRYPTED    REG_ENCRYPTED_VALUE  // 26: Encrypted random value
#define DNA_REG_DECRYPTED    REG_DECRYPTED_RESULT // 27: Decrypted result
#define DNA_REG_VERIFY       REG_VERIFY_STATUS    // 28: Verification status
#define DNA_REG_TLP          REG_TLP_CONTROL      // 29: TLP control
#define DNA_REG_CMD          REG_CONTROL_CMD      // 30: Control command
#define DNA_REG_SYS          REG_SYSTEM_STATUS    // 31: System status

// Global variables
static bool g_verbose = false;  // Verbose output mode

/**
 * Print program banner
 */
static void print_banner(void) {
    printf("====================================================\n");
    printf("  %s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("  FPGA Hardware DNA Authentication Demo Program\n");
    printf("  Build Date: %s\n", PROGRAM_DATE);
    printf("====================================================\n");
    printf("  Register Architecture: Simplified (24-31, 8 registers)\n");
    printf("  DNA Bits: 57-bit hardware unique identifier\n");
    printf("  Encryption Algorithm: XOR dynamic encryption\n");
    printf("====================================================\n\n");
}

/**
 * Print usage instructions
 */
static void print_usage(const char* program_name) {
    printf("Usage:\n");
    printf("  %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -v, --verbose     Verbose output mode (show all steps)\n");
    printf("  -r, --registers   Show register status (for debugging)\n\n");
    printf("Examples:\n");
    printf("  %s                # Simple activation\n", program_name);
    printf("  %s -v             # Verbose mode activation\n", program_name);
    printf("  %s -r             # Show register status\n", program_name);
}

/**
 * ========================================
 *  Step 1: Connect to FPGA Device
 * ========================================
 */
static HANDLE step1_connect_device(void) {
    printf("\n========================================\n");
    printf("  Step 1: Connect to FPGA Device\n");
    printf("========================================\n");

    if (g_verbose) {
        printf("[INFO] Initializing LeechCore...\n");
    }

    HANDLE hLC = init_leechcore("fpga");

    if (!hLC) {
        printf("[FAIL] Cannot connect to FPGA device\n");
        printf("[HINT] Please check:\n");
        printf("       1. FPGA device is connected via USB\n");
        printf("       2. FTD3XX driver is installed\n");
        printf("       3. leechcore.dll and FTD3XX.dll are in program directory\n");
        return NULL;
    }

    printf("[PASS] FPGA device connected successfully\n");

    if (g_verbose) {
        printf("[INFO] LeechCore handle: %p\n", hLC);
    }

    return hLC;
}

/**
 * ========================================
 *  Step 2: Read FPGA DNA Value
 * ========================================
 */
static bool step2_read_dna(HANDLE hLC, uint64_t* pDnaValue) {
    printf("\n========================================\n");
    printf("  Step 2: Read FPGA DNA Value\n");
    printf("========================================\n");

    if (g_verbose) {
        printf("[INFO] Reading FPGA DNA value (registers %d-%d)...\n",
               DNA_REG_LOW, DNA_REG_HIGH);
    }

    // Read DNA value
    uint64_t dna_value = read_dna_value(hLC);

    if (dna_value == 0 || !is_dna_valid(dna_value)) {
        printf("[FAIL] DNA value read failed or invalid\n");
        return false;
    }

    // Extract lower 32 bits and upper 25 bits
    uint32_t dna_low = (uint32_t)(dna_value & 0xFFFFFFFF);
    uint32_t dna_high = (uint32_t)((dna_value >> 32) & 0x1FFFFFF);

    printf("[PASS] DNA value read successfully\n");
    printf("[INFO] DNA Value = 0x%016llX (57 bits)\n", dna_value);

    if (g_verbose) {
        printf("[INFO]   Lower 32 bits = 0x%08X (register %d)\n", dna_low, DNA_REG_LOW);
        printf("[INFO]   Upper 25 bits = 0x%07X (register %d)\n", dna_high, DNA_REG_HIGH);
    }

    *pDnaValue = dna_value;
    return true;
}

/**
 * ========================================
 *  Step 3: Execute DNA Verification Workflow
 * ========================================
 */
static bool step3_verify_dna(HANDLE hLC, uint64_t dna_value) {
    printf("\n========================================\n");
    printf("  Step 3: Execute DNA Verification Workflow\n");
    printf("========================================\n");

    // 3.1 Start verification process
    if (g_verbose) {
        printf("[INFO] 3.1 Writing start command to register %d...\n", DNA_REG_CMD);
    }

    if (!start_verification(hLC)) {
        printf("[FAIL] Failed to start verification process\n");
        return false;
    }

    printf("[PASS] Verification process started\n");

    // 3.2 Wait for firmware ready
    if (g_verbose) {
        printf("[INFO] 3.2 Waiting for firmware ready (polling register %d)...\n", DNA_REG_SYS);
    }

    if (!wait_for_firmware_ready(hLC, 5000)) {
        printf("[FAIL] Firmware ready timeout\n");
        return false;
    }

    if (g_verbose) {
        printf("[INFO] Firmware is ready\n");
    }

    // 3.3 Read FPGA-generated encrypted random value
    if (g_verbose) {
        printf("[INFO] 3.3 Reading FPGA-generated encrypted random value (register %d)...\n",
               DNA_REG_ENCRYPTED);
    }

    uint32_t encrypted_value = read_encrypted_value(hLC);

    if (encrypted_value == 0) {
        printf("[FAIL] Failed to read encrypted value\n");
        return false;
    }

    printf("[INFO] FPGA-generated encrypted value = 0x%08X\n", encrypted_value);

    // 3.4 Software performs decryption
    if (g_verbose) {
        printf("[INFO] 3.4 Software executing XOR decryption algorithm...\n");
    }

    uint32_t decrypted_value = dna_decrypt(encrypted_value, dna_value);

    printf("[INFO] Software decryption result = 0x%08X\n", decrypted_value);

    // 3.5 Write decryption result back to FPGA
    if (g_verbose) {
        printf("[INFO] 3.5 Writing decryption result to register %d...\n", DNA_REG_DECRYPTED);
    }

    if (!write_decrypted_result(hLC, decrypted_value)) {
        printf("[FAIL] Failed to write decryption result\n");
        return false;
    }

    // 3.6 Wait for verification completion
    if (g_verbose) {
        printf("[INFO] 3.6 Waiting for FPGA verification completion (polling register %d)...\n",
               DNA_REG_VERIFY);
    }

    if (!wait_for_verification_complete(hLC, 5000)) {
        printf("[FAIL] Verification timeout\n");
        return false;
    }

    // 3.7 Check verification result
    int verify_status = check_verification_status(hLC);

    if (verify_status != VERIFY_SUCCESS) {
        printf("[FAIL] DNA verification failed (status = %d)\n", verify_status);
        return false;
    }

    printf("[PASS] DNA verification successful\n");

    return true;
}

/**
 * ========================================
 *  Step 4: Check TLP Control Status
 * ========================================
 */
static bool step4_check_tlp_control(HANDLE hLC) {
    printf("\n========================================\n");
    printf("  Step 4: Check TLP Control Status\n");
    printf("========================================\n");

    if (g_verbose) {
        printf("[INFO] Reading TLP control status (register %d)...\n", DNA_REG_TLP);
    }

    int tlp_status = check_tlp_control_status(hLC);

    if (tlp_status < 0) {
        printf("[FAIL] Failed to read TLP status\n");
        return false;
    }

    printf("[INFO] TLP Control Status = %d (%s)\n",
           tlp_status, tlp_status == TLP_ENABLED ? "Enabled" : "Disabled");

    if (tlp_status != TLP_ENABLED) {
        printf("[WARN] TLP control not enabled, registers 0-23 may be inaccessible\n");
        return false;
    }

    printf("[PASS] TLP access control enabled\n");
    printf("[INFO] Registers 0-23 (general-purpose custom registers) are now accessible\n");

    return true;
}

/**
 * ========================================
 *  Step 5: Display Activation Summary
 * ========================================
 */
static void step5_show_summary(uint64_t dna_value, bool tlp_enabled) {
    printf("\n========================================\n");
    printf("  Step 5: Activation Summary\n");
    printf("========================================\n");
    printf("  [OK] Activation Status: Success\n");
    printf("  [OK] FPGA DNA: 0x%016llX\n", dna_value);
    printf("  [OK] TLP Control: %s\n", tlp_enabled ? "Enabled" : "Disabled");
    printf("  [OK] Available Registers: 0-31 (32 total)\n");
    printf("     - Registers 0-23: General-purpose custom registers (requires activation)\n");
    printf("     - Registers 24-31: DNA verification registers (verified)\n");
    printf("========================================\n");
}

/**
 * ========================================
 *  Main Function: DNA Activation Demo Workflow
 * ========================================
 */
int main(int argc, char* argv[]) {
    // Parse command line arguments
    bool show_registers = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_banner();
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            g_verbose = true;
        }
        else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--registers") == 0) {
            show_registers = true;
        }
        else {
            printf("Error: Unknown option: %s\n", argv[i]);
            printf("Use -h or --help to view help information\n");
            return 1;
        }
    }

    // Print banner
    print_banner();

    // Execute activation workflow
    HANDLE hLC = NULL;
    uint64_t dna_value = 0;
    bool success = false;

    do {
        // Step 1: Connect device
        hLC = step1_connect_device();
        if (!hLC) break;

        // Step 2: Read DNA
        if (!step2_read_dna(hLC, &dna_value)) break;

        // Step 3: DNA verification
        if (!step3_verify_dna(hLC, dna_value)) break;

        // Step 4: Check TLP
        bool tlp_enabled = step4_check_tlp_control(hLC);

        // Step 5: Show summary
        step5_show_summary(dna_value, tlp_enabled);

        // Optional: Show register status
        if (show_registers) {
            print_register_status(hLC);
        }

        success = true;

    } while (0);

    // Cleanup resources
    if (hLC) {
        if (g_verbose) {
            printf("\n[INFO] Closing LeechCore connection...\n");
        }
        cleanup_leechcore(hLC);
    }

    // Return result
    if (success) {
        printf("\n====================================================\n");
        printf("  [OK] DNA Activation Demo Completed\n");
        printf("  FPGA has been verified and is ready!\n");
        printf("====================================================\n");
        return 0;
    } else {
        printf("\n====================================================\n");
        printf("  [ERROR] DNA Activation Failed\n");
        printf("  Please check error messages and retry\n");
        printf("====================================================\n");
        return 1;
    }
}

/*
 * PCILeech FPGA DNA Activator - DNA Crypto Implementation
 * 
 * Function: DNA-based encryption/decryption algorithms
 * Author: PCILeech Project
 * Date: 2025-07-17
 */

#include "dna_crypto.h"
#include <string.h>
#include <time.h>

/**
 * Combine DNA low 32 bits and high 25 bits into complete 57-bit DNA value
 */
uint64_t combine_dna_parts(uint32_t low32, uint32_t high25)
{
    // Ensure high 25 bits only keep valid bits
    uint64_t high_masked = (uint64_t)(high25 & DNA_HIGH_MASK);
    
    // Combine: high 25 bits shift left 32 bits + low 32 bits
    uint64_t result = (high_masked << DNA_LOW_BITS) | (uint64_t)low32;
    
    return result;
}

/**
 * DNA-based data encryption (consistent with firmware algorithm)
 */
uint32_t dna_encrypt(uint32_t data, uint64_t dna_57bit)
{
    if (!is_dna_valid(dna_57bit)) {
        return data; // Return original data if DNA invalid
    }

    // Use custom XOR encryption algorithm (consistent with firmware)
    uint32_t key1 = (uint32_t)(dna_57bit & 0xFFFFFFFF);
    uint32_t key2 = (uint32_t)((dna_57bit >> 32) & DNA_HIGH_MASK);

    // Custom XOR encryption algorithm
    uint32_t result = data ^ (key2 << 11) ^ key1 ^ (key2 >> 19);

    return result;
}

/**
 * DNA-based data decryption (consistent with firmware algorithm)
 */
uint32_t dna_decrypt(uint32_t encrypted_data, uint64_t dna_57bit)
{
    if (!is_dna_valid(dna_57bit)) {
        return encrypted_data; // Return original data if DNA invalid
    }

    // Use custom XOR decryption algorithm (consistent with firmware)
    uint32_t key1 = (uint32_t)(dna_57bit & 0xFFFFFFFF);          // dna_value[31:0]
    uint32_t key2 = (uint32_t)((dna_57bit >> 32) & DNA_HIGH_MASK); // dna_value[56:32] & 25'h1FFFFFF

    // XOR decryption (XOR is self-inverse)
    uint32_t result = encrypted_data ^ (key2 << 11) ^ key1 ^ (key2 >> 19);

    return result;
}

/**
 * Verify if DNA value is valid
 */
bool is_dna_valid(uint64_t dna_value)
{
    // DNA value cannot be 0 or all 1s
    if (dna_value == 0 || dna_value == 0x1FFFFFFFFFFFFFF) {
        return false;
    }
    
    // Check if exceeds 57-bit range
    if (dna_value > 0x1FFFFFFFFFFFFFF) {
        return false;
    }
    
    return true;
}

/**
 * Print DNA value information (for debugging)
 */
void print_dna_info(uint64_t dna_value)
{
    printf("=== DNA Information ===\n");
    printf("Complete DNA value: 0x%016llX\n", dna_value);
    printf("DNA low 32 bits: 0x%08X\n", (uint32_t)(dna_value & 0xFFFFFFFF));
    printf("DNA high 25 bits: 0x%07X\n", (uint32_t)((dna_value >> 32) & DNA_HIGH_MASK));
    printf("DNA validity: %s\n", is_dna_valid(dna_value) ? "Valid" : "Invalid");
    printf("DNA total bits: %d bits\n", DNA_TOTAL_BITS);
    printf("=======================\n");
}

/**
 * Generate DNA-based random seed
 */
uint32_t generate_dna_seed(uint64_t dna_value, uint64_t timestamp)
{
    uint32_t dna_low = (uint32_t)(dna_value & 0xFFFFFFFF);
    uint32_t dna_high = (uint32_t)((dna_value >> 32) & DNA_HIGH_MASK);
    uint32_t time_low = (uint32_t)(timestamp & 0xFFFFFFFF);
    uint32_t time_high = (uint32_t)((timestamp >> 32) & 0xFFFFFFFF);
    
    // Mix DNA and timestamp to generate seed
    uint32_t seed = dna_low ^ ((dna_high << 13) | (dna_high >> 12)) ^ 
                   ((time_low >> 7) | (time_low << 25)) ^ ((time_high << 19) | (time_high >> 13));
    
    return seed;
}

/**
 * Secure zero memory (prevent sensitive data leakage)
 */
void secure_zero_memory(void* ptr, size_t size)
{
    if (ptr && size > 0) {
        volatile char* vptr = (volatile char*)ptr;
        for (size_t i = 0; i < size; i++) {
            vptr[i] = 0;
        }
    }
}

/**
 * Get current timestamp (milliseconds)
 */
uint64_t get_timestamp_ms(void)
{
    return (uint64_t)time(NULL) * 1000;
}

/**
 * Test crypto function correctness (test function)
 */
bool test_crypto_functions(uint64_t dna_value)
{
    printf("=== Testing DNA Crypto Algorithm ===\n");
    
    if (!is_dna_valid(dna_value)) {
        printf("ERROR: DNA value invalid\n");
        return false;
    }
    
    // Test data
    uint32_t test_data[] = {
        0x12345678, 0xABCDEF00, 0x11111111, 0xFFFFFFFF,
        0x00000000, 0xAAAAAAAA, 0x55555555, 0xDEADBEEF
    };
    
    int test_count = sizeof(test_data) / sizeof(test_data[0]);
    int success_count = 0;
    
    for (int i = 0; i < test_count; i++) {
        uint32_t original = test_data[i];
        uint32_t encrypted = dna_encrypt(original, dna_value);
        uint32_t decrypted = dna_decrypt(encrypted, dna_value);
        
        printf("Test %d: Original=0x%08X, Encrypted=0x%08X, Decrypted=0x%08X", 
               i+1, original, encrypted, decrypted);
        
        if (original == decrypted) {
            printf(" [PASS]\n");
            success_count++;
        } else {
            printf(" [FAIL]\n");
        }
    }
    
    printf("Test result: %d/%d passed\n", success_count, test_count);
    printf("=====================================\n");
    
    return (success_count == test_count);
}
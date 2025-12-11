/**
 * PCILeech FPGA DNA Activator - DNA Crypto Algorithm Header
 * 
 * Function: FPGA hardware DNA-based encryption/decryption algorithms
 * Author: PCILeech Project
 * Date: 2025-07-17
 */

#ifndef DNA_CRYPTO_H
#define DNA_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// DNA related constants
#define DNA_TOTAL_BITS      57              // FPGA DNA total bits
#define DNA_LOW_BITS        32              // DNA low bits
#define DNA_HIGH_BITS       25              // DNA high bits  
#define DNA_HIGH_MASK       0x1FFFFFF       // DNA high mask (25 bits)

// Crypto algorithm related constants
#define CRYPTO_ROUNDS       3               // Encryption rounds
#define CRYPTO_KEY_SHIFT    16              // Key shift amount

/**
 * Combine DNA low 32 bits and high 25 bits into complete 57-bit DNA value
 * @param low32  DNA low 32 bits
 * @param high25 DNA high 25 bits
 * @return Complete 57-bit DNA value
 */
uint64_t combine_dna_parts(uint32_t low32, uint32_t high25);

/**
 * Encrypt data based on DNA value
 * @param data      32-bit data to encrypt
 * @param dna_57bit 57-bit DNA value
 * @return Encrypted 32-bit data
 */
uint32_t dna_encrypt(uint32_t data, uint64_t dna_57bit);

/**
 * Decrypt data based on DNA value
 * @param encrypted_data Encrypted 32-bit data
 * @param dna_57bit      57-bit DNA value
 * @return Decrypted 32-bit data
 */
uint32_t dna_decrypt(uint32_t encrypted_data, uint64_t dna_57bit);

/**
 * Verify if DNA value is valid
 * @param dna_value 57-bit DNA value
 * @return true=valid, false=invalid
 */
bool is_dna_valid(uint64_t dna_value);

/**
 * Print DNA value information (for debugging)
 * @param dna_value 57-bit DNA value
 */
void print_dna_info(uint64_t dna_value);

/**
 * Generate DNA-based random seed
 * @param dna_value 57-bit DNA value
 * @param timestamp timestamp
 * @return Random seed value
 */
uint32_t generate_dna_seed(uint64_t dna_value, uint64_t timestamp);

/**
 * Secure zero memory (prevent sensitive data leakage)
 * @param ptr  Memory pointer
 * @param size Memory size
 */
void secure_zero_memory(void* ptr, size_t size);

/**
 * Get current timestamp (milliseconds)
 * @return Current timestamp (milliseconds)
 */
uint64_t get_timestamp_ms(void);

/**
 * Verify encryption/decryption correctness (test function)
 * @param dna_value 57-bit DNA value
 * @return true=test passed, false=test failed
 */
bool test_crypto_functions(uint64_t dna_value);

#endif // DNA_CRYPTO_H
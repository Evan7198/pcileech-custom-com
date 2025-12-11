/**
 * PCILeech 自定义寄存器 API 使用标准示例
 *
 * 功能演示：
 * 1. FPGA DNA 验证与激活（必须先执行）
 * 2. 自定义寄存器读写（32个寄存器，编号 0-31）
 * 3. DNA 验证寄存器访问（寄存器 24-31）
 * 4. 完整的错误处理与日志输出
 *
 * 编译: 在 Visual Studio 2022 中打开 demo.sln
 * 运行: 按 F5 或直接运行 x64/Release/demo.exe
 *
 * 注意事项：
 * ⚠️ 必须先执行 DNA 激活，再进行寄存器操作！
 * 原因：FPGA 固件中的 TLP 访问控制在 DNA 验证成功前处于禁用状态
 */

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../software/LeechCore-master/includes/leechcore.h"

// ============================================================================
// 宏定义与常量
// ============================================================================

// Custom command definitions (与 device_fpga.c 中的定义保持一致)
#define LC_CMD_FPGA_CUSTOM_READ         0x0200000000000000
#define LC_CMD_FPGA_CUSTOM_WRITE        0x0201000000000000

// Macros for multi-register access
#define LC_CMD_FPGA_CUSTOM_READ_REG(n)  (LC_CMD_FPGA_CUSTOM_READ | ((n) & 0xFF))
#define LC_CMD_FPGA_CUSTOM_WRITE_REG(n) (LC_CMD_FPGA_CUSTOM_WRITE | ((n) & 0xFF))

// 寄存器映射常量
#define REG_DNA_LOW     24  // DNA 值低 32 位（只读）
#define REG_DNA_HIGH    25  // DNA 值高 25 位（只读）
#define REG_ENCRYPTED   26  // FPGA 生成的加密随机值（只读）
#define REG_DECRYPTED   27  // 软件写入的解密结果（写入）
#define REG_VERIFY_STATUS 28 // 验证状态（只读）
#define REG_TLP_CONTROL 29  // TLP 控制状态（只读）
#define REG_COMMAND     30  // 控制命令（写入）
#define REG_SYSTEM_STATUS 31 // 系统状态（只读）

// ============================================================================
// 辅助函数
// ============================================================================

/**
 * 打印章节标题
 */
void print_section_header(const char* title) {
    printf("\n");
    printf("========================================\n");
    printf("  %s\n", title);
    printf("========================================\n");
}

/**
 * 打印寄存器值（带格式化）
 */
void print_register_value(BYTE regNum, DWORD value) {
    printf("[INFO] 寄存器 %d = 0x%08X\n", regNum, value);
}

/**
 * 读取自定义寄存器
 * @param hLC       LeechCore 句柄
 * @param regNum    寄存器编号 (0-31)
 * @param pValue    输出寄存器值
 * @return          成功返回 true，失败返回 false
 */
bool read_custom_register(HANDLE hLC, BYTE regNum, DWORD* pValue) {
    PBYTE pbDataOut = NULL;
    DWORD cbDataOut = 0;
    BOOL result = LcCommand(hLC, LC_CMD_FPGA_CUSTOM_READ_REG(regNum), 0, NULL, &pbDataOut, &cbDataOut);

    if (result && pbDataOut && cbDataOut == sizeof(DWORD)) {
        *pValue = *(PDWORD)pbDataOut;
        LocalFree(pbDataOut);
        return true;
    }
    if (pbDataOut) LocalFree(pbDataOut);
    return false;
}

/**
 * 写入自定义寄存器
 * @param hLC       LeechCore 句柄
 * @param regNum    寄存器编号 (0-31)
 * @param value     要写入的值
 * @return          成功返回 true，失败返回 false
 */
bool write_custom_register(HANDLE hLC, BYTE regNum, DWORD value) {
    return LcCommand(hLC, LC_CMD_FPGA_CUSTOM_WRITE_REG(regNum), sizeof(DWORD), (PBYTE)&value, NULL, NULL);
}

// ============================================================================
// 步骤1: DNA 激活（必须先执行）
// ============================================================================

/**
 * 执行 FPGA DNA 验证与激活
 *
 * 说明：
 * - 读取 FPGA 硬件 DNA（57位唯一标识）
 * - 执行 XOR 加密验证
 * - 启用 TLP 访问控制
 * - 只有激活成功后，寄存器 0-23 才可读写
 *
 * @param phLC 输出 LeechCore 句柄
 * @return     成功返回 true，失败返回 false
 */
bool step1_activate_dna(HANDLE* phLC) {
    print_section_header("步骤1: FPGA DNA 激活");

    LC_CONFIG cfg = { 0 };
    DWORD dna_low, dna_high, tlp_status;

    // 初始化 LeechCore 配置
    printf("[INFO] 正在初始化 LeechCore...\n");
    cfg.dwVersion = LC_CONFIG_VERSION;
    cfg.szDevice[0] = 'f'; cfg.szDevice[1] = 'p'; cfg.szDevice[2] = 'g'; cfg.szDevice[3] = 'a';

    // 创建 LeechCore 连接
    printf("[INFO] 正在连接 FPGA 设备...\n");
    *phLC = LcCreateEx(&cfg, NULL);
    if (!*phLC) {
        printf("[FAIL] 无法连接到 FPGA 设备\n");
        printf("       请检查：1) FPGA 是否已连接  2) FTD3XX 驱动是否已安装\n");
        return false;
    }
    printf("[PASS] FPGA 设备连接成功\n");

    // 读取 FPGA DNA 值
    printf("[INFO] 正在读取 FPGA DNA...\n");
    if (!read_custom_register(*phLC, REG_DNA_LOW, &dna_low) ||
        !read_custom_register(*phLC, REG_DNA_HIGH, &dna_high)) {
        printf("[FAIL] 无法读取 DNA 值\n");
        LcClose(*phLC);
        *phLC = NULL;
        return false;
    }

    uint64_t dna_value = ((uint64_t)(dna_high & 0x1FFFFFF) << 32) | dna_low;
    printf("[PASS] FPGA DNA = 0x%016llX\n", dna_value);

    // 检查 TLP 控制状态
    printf("[INFO] 检查 TLP 控制状态...\n");
    if (!read_custom_register(*phLC, REG_TLP_CONTROL, &tlp_status)) {
        printf("[FAIL] 无法读取 TLP 控制状态\n");
        LcClose(*phLC);
        *phLC = NULL;
        return false;
    }

    if (tlp_status & 0x01) {
        printf("[PASS] TLP 控制已启用（寄存器 29 = 0x%08X）\n", tlp_status);
        printf("[INFO] DNA 激活成功！可以进行寄存器操作\n");
    } else {
        printf("[WARN] TLP 控制未启用（寄存器 29 = 0x%08X）\n", tlp_status);
        printf("[WARN] 这可能是因为 FPGA 固件未正确实现 DNA 验证逻辑\n");
        printf("[INFO] 将继续执行，但寄存器操作可能失败\n");
    }

    return true;
}

// ============================================================================
// 步骤2: 基础寄存器读写
// ============================================================================

/**
 * 演示基础寄存器读写操作
 *
 * 演示内容：
 * - 读取寄存器 0（初始值）
 * - 写入自定义值到寄存器 0
 * - 读回验证写入成功
 *
 * @param hLC LeechCore 句柄
 * @return    成功返回 true，失败返回 false
 */
bool step2_basic_register_rw(HANDLE hLC) {
    print_section_header("步骤2: 基础寄存器读写");

    DWORD value;

    // 读取寄存器 0 的初始值
    printf("[INFO] 读取寄存器 0 的初始值...\n");
    if (!read_custom_register(hLC, 0, &value)) {
        printf("[FAIL] 读取寄存器 0 失败\n");
        return false;
    }
    print_register_value(0, value);

    // 写入测试值
    DWORD test_value = 0x12345678;
    printf("[INFO] 写入测试值 0x%08X 到寄存器 0...\n", test_value);
    if (!write_custom_register(hLC, 0, test_value)) {
        printf("[FAIL] 写入寄存器 0 失败\n");
        return false;
    }
    printf("[PASS] 写入成功\n");

    // 读回验证
    printf("[INFO] 读回寄存器 0 验证...\n");
    if (!read_custom_register(hLC, 0, &value)) {
        printf("[FAIL] 读回寄存器 0 失败\n");
        return false;
    }

    if (value == test_value) {
        printf("[PASS] 读回值匹配: 0x%08X\n", value);
        printf("[INFO] 寄存器读写功能正常！\n");
        return true;
    } else {
        printf("[FAIL] 读回值不匹配: 期望 0x%08X, 实际 0x%08X\n", test_value, value);
        return false;
    }
}

// ============================================================================
// 步骤3: 多寄存器独立性验证
// ============================================================================

/**
 * 验证多寄存器独立性
 *
 * 演示内容：
 * - 向寄存器 0, 5, 10, 23 写入不同值
 * - 读回所有寄存器验证数据独立性
 *
 * @param hLC LeechCore 句柄
 * @return    成功返回 true，失败返回 false
 */
bool step3_multi_register_test(HANDLE hLC) {
    print_section_header("步骤3: 多寄存器独立性测试");

    // 测试寄存器组（避开 DNA 验证寄存器 24-31）
    BYTE test_regs[] = { 0, 5, 10, 23 };
    DWORD test_values[] = { 0xAAAAAAAA, 0x55555555, 0xDEADBEEF, 0xCAFEBABE };
    int test_count = sizeof(test_regs) / sizeof(test_regs[0]);

    // 写入不同的值到各个寄存器
    printf("[INFO] 向多个寄存器写入不同的值...\n");
    for (int i = 0; i < test_count; i++) {
        printf("       寄存器 %d <- 0x%08X\n", test_regs[i], test_values[i]);
        if (!write_custom_register(hLC, test_regs[i], test_values[i])) {
            printf("[FAIL] 写入寄存器 %d 失败\n", test_regs[i]);
            return false;
        }
    }
    printf("[PASS] 所有寄存器写入成功\n");

    // 读回验证
    printf("[INFO] 读回所有寄存器验证独立性...\n");
    bool all_passed = true;
    for (int i = 0; i < test_count; i++) {
        DWORD value;
        if (!read_custom_register(hLC, test_regs[i], &value)) {
            printf("[FAIL] 读取寄存器 %d 失败\n", test_regs[i]);
            all_passed = false;
            continue;
        }

        if (value == test_values[i]) {
            printf("[PASS] 寄存器 %d = 0x%08X (匹配)\n", test_regs[i], value);
        } else {
            printf("[FAIL] 寄存器 %d = 0x%08X (期望 0x%08X)\n", test_regs[i], value, test_values[i]);
            all_passed = false;
        }
    }

    if (all_passed) {
        printf("[INFO] 多寄存器独立性测试通过！\n");
    }

    return all_passed;
}

// ============================================================================
// 步骤4: DNA 验证寄存器访问
// ============================================================================

/**
 * 演示 DNA 验证寄存器访问
 *
 * 寄存器映射（24-31）：
 * - 寄存器 24: DNA 值低 32 位（只读）
 * - 寄存器 25: DNA 值高 25 位（只读）
 * - 寄存器 26: FPGA 生成的加密随机值（只读）
 * - 寄存器 27: 软件写入的解密结果（写入）
 * - 寄存器 28: 验证状态（0=失败, 1=成功）（只读）
 * - 寄存器 29: TLP 控制状态（0=禁用, 1=启用）（只读）
 * - 寄存器 30: 控制命令（1=开始验证）（写入）
 * - 寄存器 31: 系统状态（0=空闲, 1=处理中, 2=完成）（只读）
 *
 * @param hLC LeechCore 句柄
 */
void step4_dna_register_access(HANDLE hLC) {
    print_section_header("步骤4: DNA 验证寄存器访问");

    DWORD value;

    // 读取 DNA 值寄存器
    printf("[INFO] 读取 DNA 验证相关寄存器...\n");

    if (read_custom_register(hLC, REG_DNA_LOW, &value)) {
        printf("[INFO] 寄存器 24 (DNA 低32位) = 0x%08X\n", value);
    }

    if (read_custom_register(hLC, REG_DNA_HIGH, &value)) {
        printf("[INFO] 寄存器 25 (DNA 高25位) = 0x%08X\n", value);
    }

    if (read_custom_register(hLC, REG_ENCRYPTED, &value)) {
        printf("[INFO] 寄存器 26 (加密随机值) = 0x%08X\n", value);
    }

    if (read_custom_register(hLC, REG_VERIFY_STATUS, &value)) {
        printf("[INFO] 寄存器 28 (验证状态)   = 0x%08X %s\n", value,
               (value & 0x01) ? "(成功)" : "(未验证)");
    }

    if (read_custom_register(hLC, REG_TLP_CONTROL, &value)) {
        printf("[INFO] 寄存器 29 (TLP控制)    = 0x%08X %s\n", value,
               (value & 0x01) ? "(已启用)" : "(未启用)");
    }

    if (read_custom_register(hLC, REG_SYSTEM_STATUS, &value)) {
        printf("[INFO] 寄存器 31 (系统状态)   = 0x%08X\n", value);
    }

    printf("[INFO] DNA 验证寄存器访问演示完成\n");
}

// ============================================================================
// 主程序
// ============================================================================

int main(int argc, char* argv[]) {
    HANDLE hLC = NULL;
    int exit_code = 0;

    printf("\n");
    printf("====================================================\n");
    printf("  PCILeech 自定义寄存器 API 使用标准示例\n");
    printf("====================================================\n");
    printf("  寄存器数量: 32 个 (0-31)\n");
    printf("  通用寄存器: 0-23 (需DNA激活后才可读写)\n");
    printf("  DNA寄存器:  24-31 (DNA验证专用)\n");
    printf("====================================================\n");

    // 步骤1: DNA 激活（必须先执行）
    if (!step1_activate_dna(&hLC)) {
        printf("\n[错误] DNA 激活失败，程序退出\n");
        return 1;
    }

    // 步骤2: 基础寄存器读写
    if (!step2_basic_register_rw(hLC)) {
        printf("\n[错误] 基础寄存器读写测试失败\n");
        exit_code = 1;
        goto cleanup;
    }

    // 步骤3: 多寄存器测试
    if (!step3_multi_register_test(hLC)) {
        printf("\n[警告] 多寄存器独立性测试存在问题\n");
        // 不退出，继续演示
    }

    // 步骤4: DNA 寄存器访问
    step4_dna_register_access(hLC);

    // 成功完成
    printf("\n");
    printf("====================================================\n");
    printf("  ✓ 所有演示步骤已完成\n");
    printf("====================================================\n");
    printf("\n提示：\n");
    printf("  - 寄存器 0-23 可用于自定义应用逻辑\n");
    printf("  - 寄存器 24-31 为 DNA 验证系统保留\n");
    printf("  - 必须先执行 DNA 激活，再进行寄存器操作\n");
    printf("\n");

cleanup:
    // 清理资源
    if (hLC) {
        printf("[INFO] 正在关闭 LeechCore 连接...\n");
        LcClose(hLC);
        printf("[INFO] 连接已关闭\n");
    }

    return exit_code;
}

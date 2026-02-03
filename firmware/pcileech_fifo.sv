//
// PCILeech FPGA.
//
// FIFO network / control.
//
// (c) Ulf Frisk, 2017-2024
// Author: Ulf Frisk, pcileech@frizk.net
// Special thanks to: Dmytro Oleksiuk @d_olex
//
// Custom Registers R/W Module Author: evan, @https://discord.gg/PwAXYPMkkF
//

`timescale 1ns / 1ps
`include "pcileech_header.svh"

`define ENABLE_STARTUPE2

module pcileech_fifo #(
    parameter               PARAM_DEVICE_ID = 0,
    parameter               PARAM_VERSION_NUMBER_MAJOR = 0,
    parameter               PARAM_VERSION_NUMBER_MINOR = 0,
    parameter               PARAM_CUSTOM_VALUE = 0
) (
    input                   clk,
    input                   rst,
    input                   rst_cfg_reload,
    
    input                   pcie_present,
    input                   pcie_perst_n,
    
    IfComToFifo.mp_fifo     dcom,
    
    IfPCIeFifoCfg.mp_fifo   dcfg,
    IfPCIeFifoTlp.mp_fifo   dtlp,
    IfPCIeFifoCore.mp_fifo  dpcie,
    IfShadow2Fifo.fifo      dshadow2fifo
    );

    // -------------------------------------------------------------------------
    // DNA CHECK MODULE SIGNALS
    // -------------------------------------------------------------------------
    wire        dna_match;
    wire [56:0] dna_value;

    dna_check i_dna_check (
        .clk        ( clk       ),
        .i_enable   ( dna_match ),
        .GetDNA     ( dna_value )
    );


    // ----------------------------------------------------
    // TickCount64
    // ----------------------------------------------------
    
    time tickcount64 = 0;
    always @ ( posedge clk )
        tickcount64 <= tickcount64 + 1;
   
    // ----------------------------------------------------------------------------
    // RX FROM USB/FT601/FT245 BELOW:
    // ----------------------------------------------------------------------------
    // Incoming data received from FT601 is converted from 32-bit to 64-bit.
    // This always happen regardless whether receiving FIFOs are full or not.
    // The actual data contains a MAGIC which tells which receiving FIFO should
    // accept the data. Receiving TYPEs are: PCIe TLP, PCIe CFG, Loopback, Command.
    //
    //         /--> PCIe TLP (32-bit)
    //         |
    // FT601 --+--> PCIe CFG (64-bit)
    //         |
    //         +--> LOOPBACK (32-bit)
    //         |
    //         \--> COMMAND  (64-bit)
    //
            
    // ----------------------------------------------------------
    // Route 64-bit incoming FT601 data to correct receiver below:
    // ----------------------------------------------------------
    `define CHECK_MAGIC     (dcom.com_dout[7:0] == 8'h77)
    `define CHECK_TYPE_TLP  (dcom.com_dout[9:8] == 2'b00)
    `define CHECK_TYPE_CFG  (dcom.com_dout[9:8] == 2'b01)
    `define CHECK_TYPE_LOOP (dcom.com_dout[9:8] == 2'b10)
    `define CHECK_TYPE_CMD  (dcom.com_dout[9:8] == 2'b11)

    // DNA Authentication Gate - TLP transmission controlled by DNA verification
    assign   dtlp.tx_valid = dcom.com_dout_valid & `CHECK_MAGIC & `CHECK_TYPE_TLP & tlp_control_enabled;
    assign   dcfg.tx_valid = dcom.com_dout_valid & `CHECK_MAGIC & `CHECK_TYPE_CFG;
    wire     _loop_rx_wren = dcom.com_dout_valid & `CHECK_MAGIC & `CHECK_TYPE_LOOP;
    wire     _cmd_rx_wren  = dcom.com_dout_valid & `CHECK_MAGIC & `CHECK_TYPE_CMD;
    
    // Incoming TLPs are forwarded to PCIe core logic.
    assign dtlp.tx_data = dcom.com_dout[63:32];
    assign dtlp.tx_last = dcom.com_dout[10];
    // Incoming CFGs are forwarded to PCIe core logic.
    assign dcfg.tx_data = dcom.com_dout;

    // ----------------------------------------------------------------------------
    // TX TO USB/FT601/FT245 BELOW:
    // ----------------------------------------------------------------------------
    //
    //                                         MULTIPLEXER
    //                                         ===========
    //                                         1st priority
    // PCIe TLP ->(32-bit) --------------------->--\
    //                                         2nd |    /-----------------------------------------\
    // PCIe CFG ->(32-bit)---------------------->--+--> | 256-> BUFFER FIFO (NATIVE OR DRAM) ->32 | -> FT601
    //                                             |    \-----------------------------------------/
    //            /--------------------------\ 3rd |
    // FT601   -> | 34->  Loopback FIFO ->34 | ->--/
    //            \--------------------------/     | 
    //                                             |
    //            /--------------------------\ 4th |
    // COMMAND -> | 34->  Command FIFO  ->34 | ->--/
    //            \--------------------------/
    //
   
    // ----------------------------------------------------------
    // LOOPBACK FIFO:
    // ----------------------------------------------------------
    wire [33:0]       _loop_dout;
    wire              _loop_valid;
    wire              _loop_rd_en;
    fifo_34_34 i_fifo_loop_tx(
        .clk            ( clk                       ),
        .srst           ( rst                       ),
        .rd_en          ( _loop_rd_en               ),
        .dout           ( _loop_dout                ),
        .din            ( {dcom.com_dout[11:10], dcom.com_dout[63:32]} ),
        .wr_en          ( _loop_rx_wren             ),
        .full           (                           ),
        .almost_full    (                           ),
        .empty          (                           ),
        .valid          ( _loop_valid               )
    );
   
    // ----------------------------------------------------------
    // COMMAND FIFO:
    // ----------------------------------------------------------
    wire [33:0]       _cmd_tx_dout;
    wire              _cmd_tx_valid;
    wire              _cmd_tx_rd_en;
    wire              _cmd_tx_almost_full;
    reg               _cmd_tx_wr_en;
    reg [33:0]        _cmd_tx_din;
    fifo_34_34 i_fifo_cmd_tx(
        .clk            ( clk                       ),
        .srst           ( rst                       ),
        .rd_en          ( _cmd_tx_rd_en             ),
        .dout           ( _cmd_tx_dout              ),
        .din            ( _cmd_tx_din               ),
        .wr_en          ( _cmd_tx_wr_en             ),
        .full           (                           ),
        .almost_full    ( _cmd_tx_almost_full       ),
        .empty          (                           ),
        .valid          ( _cmd_tx_valid             )
    );

    // ----------------------------------------------------------
    // MULTIPLEXER
    // ----------------------------------------------------------
    pcileech_mux i_pcileech_mux(
        .clk            ( clk                           ),
        .rst            ( rst                           ),
        // output
        .dout           ( dcom.com_din                  ),
        .valid          ( dcom.com_din_wr_en            ),
        .rd_en          ( dcom.com_din_ready            ),
        // LOOPBACK:
        .p0_din         ( _loop_dout[31:0]              ),
        .p0_tag         ( 2'b10                         ),
        .p0_ctx         ( _loop_dout[33:32]             ),
        .p0_wr_en       ( _loop_valid                   ),
        .p0_req_data    ( _loop_rd_en                   ),
        // COMMAND:
        .p1_din         ( _cmd_tx_dout[31:0]            ),
        .p1_tag         ( 2'b11                         ),
        .p1_ctx         ( _cmd_tx_dout[33:32]           ),
        .p1_wr_en       ( _cmd_tx_valid                 ),
        .p1_req_data    ( _cmd_tx_rd_en                 ),
        // PCIe CFG
        .p2_din         ( dcfg.rx_data                  ),
        .p2_tag         ( 2'b01                         ),
        .p2_ctx         ( 2'b00                         ),
        .p2_wr_en       ( dcfg.rx_valid                 ),
        .p2_req_data    ( dcfg.rx_rd_en                 ),
        // PCIe TLP #1 - Gated by DNA verification status
        .p3_din         ( dtlp.rx_data[0]                        ),
        .p3_tag         ( 2'b00                                  ),
        .p3_ctx         ( {dtlp.rx_first[0], dtlp.rx_last[0]}    ),
        .p3_wr_en       ( dtlp.rx_valid[0] & tlp_control_enabled ),
        .p3_req_data    ( dtlp.rx_rd_en                          ),
        // PCIe TLP #2 - Gated by DNA verification status
        .p4_din         ( dtlp.rx_data[1]                        ),
        .p4_tag         ( 2'b00                                  ),
        .p4_ctx         ( {dtlp.rx_first[1], dtlp.rx_last[1]}    ),
        .p4_wr_en       ( dtlp.rx_valid[1] & tlp_control_enabled ),
        .p4_req_data    (                                        ),
        // PCIe TLP #3 - Gated by DNA verification status
        .p5_din         ( dtlp.rx_data[2]                        ),
        .p5_tag         ( 2'b00                                  ),
        .p5_ctx         ( {dtlp.rx_first[2], dtlp.rx_last[2]}    ),
        .p5_wr_en       ( dtlp.rx_valid[2] & tlp_control_enabled ),
        .p5_req_data    ( dtlp.rx_rd_en                          ),
        // PCIe TLP #4 - Gated by DNA verification status
        .p6_din         ( dtlp.rx_data[3]                        ),
        .p6_tag         ( 2'b00                                  ),
        .p6_ctx         ( {dtlp.rx_first[3], dtlp.rx_last[3]}    ),
        .p6_wr_en       ( dtlp.rx_valid[3] & tlp_control_enabled ),
        .p6_req_data    (                                        ),
        // P7
        .p7_din         ( 32'h00000000                  ),
        .p7_tag         ( 2'b11                         ),
        .p7_ctx         ( 2'b00                         ),
        .p7_wr_en       ( 1'b0                          ),
        .p7_req_data    (                               )
    );
   
    // ------------------------------------------------------------------------
    // COMMAND / CONTROL FIFO: REGISTER FILE: COMMON
    // ------------------------------------------------------------------------
    
    localparam          RWPOS_WAIT_COMPLETE         = 18;   // WAIT FOR DRP COMPLETION
    localparam          RWPOS_DRP_RD_EN             = 20;
    localparam          RWPOS_DRP_WR_EN             = 21;
    localparam          RWPOS_GLOBAL_SYSTEM_RESET   = 31;
    
    wire    [319:0]     ro;
    reg     [239:0]     rw;

    // =========================================================================
    // CUSTOM REGISTER EXTENSION & DNA VERIFICATION SYSTEM
    // =========================================================================
    //
    // Module Description:
    // - Original PCILeech core functions (memory R/W, TLP, PCIe) remain unchanged
    // - Custom register system: 32x 32-bit registers (0-31) via LeechCore API
    // - DNA verification: Hardware DNA-based encryption authentication
    //
    // Register Allocation (32 registers):
    // -------------------------------------------------------------------------
    // | Register | Type | Description                                         |
    // |----------|------|-----------------------------------------------------|
    // | 0-23     | R/W  | General-purpose custom registers (user available)   |
    // | 24       | RO   | DNA value lower 32 bits                             |
    // | 25       | RO   | DNA value upper 25 bits (zero-extended to 32)       |
    // | 26       | RO   | FPGA-generated encrypted random value               |
    // | 27       | WO   | Software decryption result (write to verify)        |
    // | 28       | RO   | Verification status (0=failed, 1=success)           |
    // | 29       | RO   | TLP control status (0=disabled, 1=enabled)          |
    // | 30       | WO   | Control command (write 1 to start verification)     |
    // | 31       | RO   | System state (0=idle, 1=processing, 2=complete)     |
    // -------------------------------------------------------------------------
    //
    // DNA Verification Workflow:
    // 1. FPGA generates DNA-encrypted random value -> stored in reg[26]
    // 2. Software reads reg[24]+reg[25] to get complete DNA value
    // 3. Software writes reg[30]=1 to start verification
    // 4. Software reads reg[26], decrypts using DNA algorithm
    // 5. Software writes decryption result to reg[27]
    // 6. FPGA verifies result, sets reg[28] verification status
    // 7. On success, reg[29] enables TLP control
    //
    // Compatibility: All original PCILeech functions remain unchanged
    // =========================================================================

    // 32 custom 32-bit registers array
    reg [31:0]  custom_registers [0:31];

    // -------------------------------------------------------------------------
    // DNA Verification System State Variables
    // -------------------------------------------------------------------------
    reg [31:0]  current_random;         // Current plaintext random number
    reg [31:0]  encrypted_random;       // DNA-encrypted random (intermediate)
    reg         verification_active;    // Verification in progress flag
    reg         tlp_control_enabled;    // TLP control enable (DNA verified)
    reg [31:0]  dna_seed;               // Random seed derived from hardware DNA
    reg [31:0]  random_counter;         // Counter for random uniqueness
    reg         random_ready;           // Random generation complete flag

    // -------------------------------------------------------------------------
    // Anti-Brute-Force Attack State Variables
    // -------------------------------------------------------------------------
    reg [7:0]   failed_attempts_counter;        // Failed attempt counter (0-255)
    reg         activation_permanently_blocked; // Permanent block flag

    // Anti-brute-force configuration (configurable)
    localparam MAX_FAILED_ATTEMPTS = 8'd50;     // Max 50 failed attempts allowed

    // =========================================================================

    // special non-user accessible registers 
    reg     [79:0]      _pcie_core_config = { 4'hf, 1'b1, 1'b1, 1'b0, 1'b0, 8'h02, 16'h0666, 16'h10EE, 16'h0007, 16'h10EE };
    time                _cmd_timer_inactivity_base;
    reg                 rwi_drp_rd_en;
    reg                 rwi_drp_wr_en;
    reg     [15:0]      rwi_drp_data;
    
    // ------------------------------------------------------------------------
    // COMMAND / CONTROL FIFO: REGISTER FILE: READ-ONLY LAYOUT/SPECIFICATION
    // ------------------------------------------------------------------------
    
    // MAGIC
    assign ro[15:0]     = 16'hab89;                     // +000: MAGIC
    // SPECIAL
    assign ro[19:16]    = 0;                            // +002: SPECIAL
    assign ro[20]       = rwi_drp_rd_en;                //
    assign ro[21]       = rwi_drp_wr_en;                //
    assign ro[31:22]    = 0;                            //
    // SIZEOF / BYTECOUNT [little-endian]
    assign ro[63:32]    = $bits(ro) >> 3;               // +004: SIZEOF / BYTECOUNT [little-endian]
    // ID: VERSION & DEVICE
    assign ro[71:64]    = PARAM_VERSION_NUMBER_MAJOR;   // +008: VERSION MAJOR
    assign ro[79:72]    = PARAM_VERSION_NUMBER_MINOR;   // +009: VERSION MINOR
    assign ro[87:80]    = PARAM_DEVICE_ID;              // +00A: DEVICE ID
    assign ro[127:88]   = 0;                            // +00B: SLACK
    // UPTIME (tickcount64*100MHz)
    assign ro[191:128]   = tickcount64;                 // +010: UPTIME (tickcount64*100MHz)
    // INACTIVITY TIMER
    assign ro[255:192]  = _cmd_timer_inactivity_base;   // +018: INACTIVITY TIMER
    // PCIe DRP 
    assign ro[271:256]  = rwi_drp_data;                 // +020: DRP: pcie_drp_do
    // PCIe
    assign ro[272]      = pcie_present;                 // +022: PCIe PRSNT#
    assign ro[273]      = pcie_perst_n;                 //       PCIe PERST#
    assign ro[287:274]  = 0;                            //       SLACK
    // CUSTOM_VALUE
    assign ro[319:288]  = PARAM_CUSTOM_VALUE;           // +024: CUSTOM VALUE
    // +028
    
    // ------------------------------------------------------------------------
    // INITIALIZATION/RESET BLOCK _AND_
    // COMMAND / CONTROL FIFO: REGISTER FILE: READ-WRITE LAYOUT/SPECIFICATION
    // ------------------------------------------------------------------------
    
    task pcileech_fifo_ctl_initialvalues;               // task is non automatic
        begin
            _cmd_tx_wr_en  <= 1'b0;
               
            // MAGIC
            rw[15:0]    <= 16'hefcd;                    // +000: MAGIC
            // SPECIAL
            rw[16]      <= 0;                           // +002: enable inactivity timer
            rw[17]      <= 0;                           //       enable send count
            rw[18]      <= 1;                           //       WAIT FOR PCIe DRP RD/WR COMPLETION BEFORE ACCEPT NEW FIFO READ/WRITES
            rw[19]      <= 0;                           //       SLACK
            rw[20]      <= 0;                           //       DRP RD EN
            rw[21]      <= 0;                           //       DRP WR EN
            rw[30:22]   <= 0;                           //       RESERVED FUTURE
            rw[31]      <= 0;                           //       global system reset (GSR) via STARTUPE2 primitive
            // SIZEOF / BYTECOUNT [little-endian]
            rw[63:32]   <= $bits(rw) >> 3;              // +004: bytecount [little endian]
            // CMD INACTIVITY TIMER TRIGGER VALUE
            rw[95:64]   <= 0;                           // +008: cmd_inactivity_timer (ticks) [little-endian]
            // CMD SEND COUNT
            rw[127:96]  <= 0;                           // +00C: cmd_send_count [little-endian]
            // PCIE INITIAL CONFIG (SPECIAL BITSTREAM)
            // NB! "initial" CLK0 values may also be changed in: '_pcie_core_config = {...};' [important on PCIeScreamer].
            rw[143:128] <= 16'h10EE;                    // +010: CFG_SUBSYS_VEND_ID (NOT IMPLEMENTED)
            rw[159:144] <= 16'h0007;                    // +012: CFG_SUBSYS_ID      (NOT IMPLEMENTED)
            rw[175:160] <= 16'h10EE;                    // +014: CFG_VEND_ID        (NOT IMPLEMENTED)
            rw[191:176] <= 16'h0666;                    // +016: CFG_DEV_ID         (NOT IMPLEMENTED)
            rw[199:192] <= 8'h02;                       // +018: CFG_REV_ID         (NOT IMPLEMENTED)
            rw[200]     <= 1'b1;                        // +019: PCIE CORE RESET
            rw[201]     <= 1'b0;                        //       PCIE SUBSYSTEM RESET
            rw[202]     <= 1'b1;                        //       CFGTLP PROCESSING ENABLE
            rw[203]     <= 1'b0;                        //       CFGTLP ZERO DATA
            rw[204]     <= 1'b1;                        //       CFGTLP FILTER TLP FROM USER
            rw[205]     <= 1'b1;                        //       PCIE BAR PIO ON-BOARD PROCESSING ENABLE
            rw[206]     <= 1'b1;                        //       CFGTLP PCIE WRITE ENABLE
            rw[207]     <= 1'b0;                        //       TLP FILTER FROM USER: EXCEPT: Cpl,CplD and CfgRd/CfgWr (handled by rw[204])
            // PCIe DRP, PRSNT#, PERST#
            rw[208+:16] <= 0;                           // +01A: DRP: pcie_drp_di
            rw[224+:9]  <= 0;                           // +01C: DRP: pcie_drp_addr
            rw[233+:7]  <= 0;                           //       SLACK
            // 01E -  
             
        end
    endtask
    
    // =========================================================================
    // CUSTOM REGISTER INITIALIZATION TASK
    // =========================================================================
    task custom_registers_initialvalues;                           // non automatic
        begin
            // Custom register array initialization (32 registers)
            custom_registers[0] <= 32'hE1E2E3E4;                   // Reg 0: default test value
            for (integer i = 1; i < 24; i++) begin
                custom_registers[i] <= 32'h00000000;               // Reg 1-23: general purpose, clear
            end
        end
    endtask

    // =========================================================================
    // DNA VERIFICATION SYSTEM INITIALIZATION TASK
    // =========================================================================
    task dna_verification_initialvalues;                           // non automatic
        begin
            // DNA verification registers initialization (reg 24-31)
            custom_registers[24] <= dna_value[31:0];               // DNA lower 32 bits (RO)
            custom_registers[25] <= {7'b0, dna_value[56:32]};      // DNA upper 25 bits (RO)
            custom_registers[26] <= 32'h0;                         // Encrypted random (FPGA gen)
            custom_registers[27] <= 32'h0;                         // Decryption result (SW write)
            custom_registers[28] <= 32'h0;                         // Verification status (FPGA)
            custom_registers[29] <= 32'h0;                         // TLP control (FPGA)
            custom_registers[30] <= 32'h0;                         // Control command (SW write)
            custom_registers[31] <= 32'h0;                         // System state (FPGA)

            // DNA verification system state variable initialization
            current_random <= 32'h0;                               // Clear current random
            encrypted_random <= 32'h0;                             // Clear encrypted cache
            verification_active <= 1'b0;                           // Verification not active
            tlp_control_enabled <= 1'b0;                           // TLP control disabled
            dna_seed <= dna_value[31:0] ^ dna_value[56:32];        // Compute DNA seed
            random_counter <= 32'h0;                               // Clear random counter
            random_ready <= 1'b0;                                  // Random not ready

            // Anti-brute-force variable initialization
            failed_attempts_counter <= 8'h0;                       // Clear fail counter
            activation_permanently_blocked <= 1'b0;                // Activation not blocked
        end
    endtask
    
    wire                _cmd_timer_inactivity_enable    = rw[16];
    wire    [31:0]      _cmd_timer_inactivity_ticks     = rw[95:64];
    wire    [15:0]      _cmd_send_count_dword           = rw[63:32];
    wire                _cmd_send_count_enable          = rw[17] & (_cmd_send_count_dword != 16'h0000);
    
    always @ ( posedge clk )
         _pcie_core_config <= rw[207:128];
    assign dpcie.pcie_rst_core                          = _pcie_core_config[72];
    assign dpcie.pcie_rst_subsys                        = _pcie_core_config[73];
    assign dshadow2fifo.cfgtlp_en                       = _pcie_core_config[74];
    assign dshadow2fifo.cfgtlp_zero                     = _pcie_core_config[75];
    assign dshadow2fifo.cfgtlp_filter                   = _pcie_core_config[76];
    assign dshadow2fifo.bar_en                          = _pcie_core_config[77];
    assign dshadow2fifo.cfgtlp_wren                     = _pcie_core_config[78];
    assign dshadow2fifo.alltlp_filter                   = _pcie_core_config[79];
    assign dpcie.drp_en                                 = rw[RWPOS_DRP_WR_EN] | rw[RWPOS_DRP_RD_EN];
    assign dpcie.drp_we                                 = rw[RWPOS_DRP_WR_EN];
    assign dpcie.drp_addr                               = rw[224+:9];
    assign dpcie.drp_di                                 = rw[208+:16];
    
    // ------------------------------------------------------------------------
    // COMMAND / CONTROL FIFO: STATE MACHINE / LOGIC FOR READ/WRITE AND OTHER HOUSEKEEPING TASKS
    // ------------------------------------------------------------------------
 
    wire [63:0] cmd_rx_dout;
    wire        cmd_rx_valid;
    wire        cmd_rx_rd_en_drp = rwi_drp_rd_en | rwi_drp_wr_en | rw[RWPOS_DRP_RD_EN] | rw[RWPOS_DRP_WR_EN];
    wire        cmd_rx_rd_en = tickcount64[1] & ( ~rw[RWPOS_WAIT_COMPLETE] | ~cmd_rx_rd_en_drp);
    
    fifo_64_64_clk1_fifocmd i_fifo_cmd_rx(
        .clk            ( clk                       ),
        .srst           ( rst                       ),
        .rd_en          ( cmd_rx_rd_en              ),
        .dout           ( cmd_rx_dout               ),
        .din            ( dcom.com_dout             ),
        .wr_en          ( _cmd_rx_wren              ),
        .full           (                           ),
        .empty          (                           ),
        .valid          ( cmd_rx_valid              )
    );

    integer i_write;
    wire [15:0] in_cmd_address_byte = cmd_rx_dout[31:16];
    wire [17:0] in_cmd_address_bit  = {in_cmd_address_byte[14:0], 3'b000};
    wire [15:0] in_cmd_value        = {cmd_rx_dout[48+:8], cmd_rx_dout[56+:8]};
    wire [15:0] in_cmd_mask         = {cmd_rx_dout[32+:8], cmd_rx_dout[40+:8]};
    wire        f_rw                = in_cmd_address_byte[15];
    wire        f_shadowcfgspace    = in_cmd_address_byte[14];
    wire [15:0] in_cmd_data_in      = (in_cmd_address_bit < (f_rw ? $bits(rw) : $bits(ro))) ? (f_rw ? rw[in_cmd_address_bit+:16] : ro[in_cmd_address_bit+:16]) : 16'h0000;
    wire        in_cmd_read         = cmd_rx_valid & cmd_rx_dout[12] & ~f_shadowcfgspace;
    wire        in_cmd_write        = cmd_rx_valid & cmd_rx_dout[13] & ~f_shadowcfgspace & f_rw;
    // -------------------------------------------------------------------------
    // CUSTOM REGISTER READ/WRITE COMMAND PARSER LOGIC
    // -------------------------------------------------------------------------
    wire [4:0]  custom_reg_num      = in_cmd_address_byte[5:1];    // Register number (0-31)
    wire        custom_reg_sel      = in_cmd_address_byte[0];      // Byte offset (0=low16, 1=high16)
    wire        in_cmd_custom_read  = cmd_rx_valid & cmd_rx_dout[14] & ~cmd_rx_dout[13] & ~cmd_rx_dout[12] & ~f_shadowcfgspace;
    wire        in_cmd_custom_write = cmd_rx_valid & cmd_rx_dout[15] & ~cmd_rx_dout[14] & ~cmd_rx_dout[13] & ~cmd_rx_dout[12] & ~f_shadowcfgspace;

    assign dshadow2fifo.rx_rden     = cmd_rx_valid & cmd_rx_dout[12] &  f_shadowcfgspace;
    assign dshadow2fifo.rx_wren     = cmd_rx_valid & cmd_rx_dout[13] &  f_shadowcfgspace & f_rw;
    assign dshadow2fifo.rx_be       = {(in_cmd_mask[7:0] > 0 ? ~in_cmd_address_byte[1] : 1'b0), (in_cmd_mask[15:8] > 0 ? ~in_cmd_address_byte[1] : 1'b0), (in_cmd_mask[7:0] > 0 ? in_cmd_address_byte[1] : 1'b0), (in_cmd_mask[15:8] > 0 ? in_cmd_address_byte[1] : 1'b0)};
    assign dshadow2fifo.rx_addr     = in_cmd_address_byte[11:2];
    assign dshadow2fifo.rx_addr_lo  = in_cmd_address_byte[1];
    assign dshadow2fifo.rx_data     = {in_cmd_value[7:0], in_cmd_value[15:8], in_cmd_value[7:0], in_cmd_value[15:8]};
    
    initial begin
        pcileech_fifo_ctl_initialvalues();
        //custom_registers_initialvalues(); //test task : test customreg[0] r/w
        dna_verification_initialvalues();
    end

    always @ ( posedge clk )
        if ( rst ) begin
            pcileech_fifo_ctl_initialvalues();
            //custom_registers_initialvalues();     // Initialize custom registers
            dna_verification_initialvalues();       // Initialize DNA verification
        end else if ( dna_match )
            begin
                // SHADOW CONFIG SPACE RESPONSE
                if ( dshadow2fifo.tx_valid )
                    begin
                        _cmd_tx_wr_en       <= 1'b1;
                        _cmd_tx_din[31:16]  <= {4'b1100, dshadow2fifo.tx_addr, dshadow2fifo.tx_addr_lo, 1'b0};
                        _cmd_tx_din[15:0]   <= dshadow2fifo.tx_addr_lo ? dshadow2fifo.tx_data[15:0] : dshadow2fifo.tx_data[31:16];                        
                    end
                // READ config
                else if ( in_cmd_read )
                    begin
                        _cmd_tx_wr_en       <= 1'b1;
                        _cmd_tx_din[31:16]  <= in_cmd_address_byte;
                        _cmd_tx_din[15:0]   <= {in_cmd_data_in[7:0], in_cmd_data_in[15:8]};
                    end
                // -----------------------------------------------------------------
                // CUSTOM REGISTER READ LOGIC
                // -----------------------------------------------------------------
                else if ( in_cmd_custom_read )
                    begin
                        _cmd_tx_wr_en       <= 1'b1;
                        _cmd_tx_din[33:32]  <= 2'b00;                  // Route as TLP
                        _cmd_tx_din[31:16]  <= in_cmd_address_byte;    // Echo address
                        case(custom_reg_sel) // Byte offset selection
                            1'b0: _cmd_tx_din[15:0] <= {custom_registers[custom_reg_num][15:8], custom_registers[custom_reg_num][7:0]};     // Lower 16 bits
                            1'b1: _cmd_tx_din[15:0] <= {custom_registers[custom_reg_num][31:24], custom_registers[custom_reg_num][23:16]};  // Upper 16 bits
                        endcase
                    end

                // SEND COUNT ACTION
                else if ( ~_cmd_tx_almost_full & ~in_cmd_write & _cmd_send_count_enable )
                    begin
                        _cmd_tx_wr_en       <= 1'b1;
                        _cmd_tx_din[31:16]  <= 16'hfffe;
                        _cmd_tx_din[15:0]   <= _cmd_send_count_dword;
                        rw[63:32]           <= _cmd_send_count_dword - 1;
                    end
                // INACTIVITY TIMER ACTION
                else if ( ~_cmd_tx_almost_full & ~in_cmd_write & _cmd_timer_inactivity_enable & (_cmd_timer_inactivity_ticks + _cmd_timer_inactivity_base < tickcount64) )
                    begin
                        _cmd_tx_wr_en       <= 1'b1;
                        _cmd_tx_din[31:16]  <= 16'hffff;
                        _cmd_tx_din[15:0]   <= 16'hcede;
                        rw[16]              <= 1'b0;
                    end
                else
                    _cmd_tx_wr_en <= 1'b0;

                // WRITE config
                if ( tickcount64 < 8 )
                    pcileech_fifo_ctl_initialvalues();
                else if ( in_cmd_write )
                    for ( i_write = 0; i_write < 16; i_write = i_write + 1 )
                        begin
                            if ( in_cmd_mask[i_write] )
                                rw[in_cmd_address_bit+i_write] <= in_cmd_value[i_write];
                        end

                // -----------------------------------------------------------------
                // CUSTOM REGISTER WRITE LOGIC
                // -----------------------------------------------------------------
                if ( in_cmd_custom_write )
                    begin
                        case(custom_reg_sel) // Byte offset selection
                            1'b0: custom_registers[custom_reg_num][15:0] <= in_cmd_value;    // Write lower 16 bits
                            1'b1: custom_registers[custom_reg_num][31:16] <= in_cmd_value;   // Write upper 16 bits
                        endcase
                    end
                    
                // -----------------------------------------------------------------
                // DNA ENCRYPTION CODE AUTO-GENERATION LOGIC
                // -----------------------------------------------------------------
                // Phase 1: DNA validity check + random number generation
                // Condition: reg[26]==0 && DNA!=0 && random not ready && stable (>64 cycles)
                if (custom_registers[26] == 32'h0 && dna_value != 57'h0 && !random_ready && tickcount64 > 64) begin
                    random_counter <= random_counter + 1'b1;                                    // Update counter for uniqueness
                    current_random <= dna_seed ^ tickcount64[31:0] ^ (random_counter + 1'b1);   // Generate plaintext random
                    random_ready <= 1'b1;                                                       // Mark random ready
                end

                // Phase 2: Encryption and storage
                // Condition: reg[26]==0 && random ready
                else if (custom_registers[26] == 32'h0 && random_ready) begin
                    // XOR encrypt current_random with DNA and store to reg[26]
                    // Custom XOR encryption algorithm
                    custom_registers[26] <= current_random ^
                                            ((dna_value[56:32] & 25'h1FFFFFF) << 11) ^
                                            dna_value[31:0] ^
                                            ((dna_value[56:32] & 25'h1FFFFFF) >> 19);
                end

                // -----------------------------------------------------------------
                // DNA VERIFICATION CONTROL LOGIC
                // -----------------------------------------------------------------

                // Start verification: Software writes reg[30]=1 to trigger
                if (custom_registers[30] == 32'h1 && !verification_active && !activation_permanently_blocked) begin
                    verification_active <= 1'b1;              // Activate verification
                    custom_registers[31] <= 32'h2;            // Set state to complete (encrypted value exists)
                end

                // Execute verification: Check software decryption result
                if (verification_active &&
                    custom_registers[27][31:16] != 16'h0 && custom_registers[27][15:0] != 16'h0) begin

                    // If TLP already enabled, return success to avoid accidental disable
                    if (tlp_control_enabled) begin
                        // TLP enabled: Skip verification, maintain state
                        custom_registers[28] <= 32'h1;        // Verification status = success
                        custom_registers[29] <= 32'h1;        // TLP control = enabled
                    end
                    // Execute actual verification only when TLP not enabled
                    else if (custom_registers[27] == current_random) begin
                        // Verification SUCCESS: Software correctly decrypted
                        custom_registers[28] <= 32'h1;        // Verification status = success
                        custom_registers[29] <= 32'h1;        // Enable TLP control
                        tlp_control_enabled <= 1'b1;          // Allow normal TLP transmission
                    end else begin
                        // Verification FAILED: Decryption mismatch
                        custom_registers[28] <= 32'h0;        // Verification status = failed
                        custom_registers[29] <= 32'h0;        // Disable TLP control
                        tlp_control_enabled <= 1'b0;          // Block TLP transmission

                        // Anti-brute-force: Increment fail counter
                        failed_attempts_counter <= failed_attempts_counter + 1'b1;
                        if (failed_attempts_counter >= MAX_FAILED_ATTEMPTS - 1) begin
                            activation_permanently_blocked <= 1'b1;  // Permanently block
                        end
                    end

                    // Verification complete: Cleanup state
                    verification_active <= 1'b0;              // End verification
                    custom_registers[30] <= 32'h0;            // Clear control command
                    end


                // UPDATE INACTIVITY TIMER BASE
                if ( dcom.com_din_wr_en | ~dcom.com_din_ready )
                    _cmd_timer_inactivity_base <= tickcount64;  
                    
                // DRP READ/WRITE                        
                if ( dpcie.drp_rdy )
                    begin
                        rwi_drp_rd_en   <= 1'b0;
                        rwi_drp_wr_en   <= 1'b0;
                        rwi_drp_data    <= dpcie.drp_do;
                    end
                else if ( rw[RWPOS_DRP_RD_EN] | rw[RWPOS_DRP_WR_EN] )
                    begin
                        rw[RWPOS_DRP_RD_EN] <= 1'b0;
                        rw[RWPOS_DRP_WR_EN] <= 1'b0;
                        rwi_drp_rd_en <= rwi_drp_rd_en | rw[RWPOS_DRP_RD_EN];
                        rwi_drp_wr_en <= rwi_drp_wr_en | rw[RWPOS_DRP_WR_EN];
                    end      
            
            end

    // ----------------------------------------------------
    // GLOBAL SYSTEM RESET:  ( provided via STARTUPE2 primitive )
    // ----------------------------------------------------
`ifdef ENABLE_STARTUPE2

    STARTUPE2 #(
      .PROG_USR         ( "FALSE"           ),
      .SIM_CCLK_FREQ    ( 0.0               )
    ) i_STARTUPE2 (
      .CFGCLK           (                   ), // ->
      .CFGMCLK          (                   ), // ->
      .EOS              (                   ), // ->
      .PREQ             (                   ), // ->
      .CLK              ( clk               ), // <-
      .GSR              ( rw[RWPOS_GLOBAL_SYSTEM_RESET] | rst_cfg_reload ), // <- GLOBAL SYSTEM RESET
      .GTS              ( 1'b0              ), // <-
      .KEYCLEARB        ( 1'b0              ), // <-
      .PACK             ( 1'b0              ), // <-
      .USRCCLKO         ( 1'b0              ), // <-
      .USRCCLKTS        ( 1'b0              ), // <-
      .USRDONEO         ( 1'b1              ), // <-
      .USRDONETS        ( 1'b1              )  // <-
    );


`endif /* ENABLE_STARTUPE2 */

endmodule

// =============================================================================
// DNA READ MODULE
// =============================================================================
// Reads the 57-bit hardware DNA value from Xilinx DNA_PORT primitive.
// The DNA value is unique to each FPGA device.
// =============================================================================
module dna_check (
    input  clk,
    output i_enable,
    output reg  [56:0] GetDNA 
);

    reg         running;
    reg [6:0]   current_bit;
    reg [56:0]  read_dna;
    wire        dna_bit;

    reg         dna_shift;
    reg         dna_read;
    reg         first;
    reg         dna_ready;
    reg         dna_ready_pipe; // Read complete signal timing control

    initial begin
        dna_shift   <= 0;
        dna_read    <= 0;
        current_bit <= 0;
        first       <= 1;
        running     <= 1;
        read_dna    <= 0;
        dna_ready   <= 0;
        dna_ready_pipe <= 0;
    end
    
    DNA_PORT #(
        .SIM_DNA_VALUE  (57'h0123456789ABCDE)   // Simulation DNA value
    ) dna (
        .DOUT   ( dna_bit   ),
        .CLK    ( clk       ),
        .DIN    ( 0         ),
        .READ   ( dna_read  ),
        .SHIFT  ( dna_shift )
    );

    always @(posedge clk) begin
        if (running) begin
            if (~dna_shift) begin
                if (first) begin
                    dna_read <= 1;
                    first <= 0;
                end else begin
                    dna_read <= 0;
                    dna_shift <= 1;
                end
            end

            if (~dna_read & dna_shift) begin
                current_bit <= current_bit + 1;
                read_dna <= {read_dna[55:0], dna_bit};

                if (current_bit == 56) begin
                    running <= 0;
                    dna_shift <= 0;
                    dna_ready_pipe <= 1; 
                    GetDNA <= GetDNA == 57'h0 ? {read_dna[55:0], dna_bit} : GetDNA; 
                end
            end
        end
        dna_ready <= dna_ready_pipe; //Fixed DNA reading timing issues.
    end

    assign i_enable = dna_ready;

endmodule

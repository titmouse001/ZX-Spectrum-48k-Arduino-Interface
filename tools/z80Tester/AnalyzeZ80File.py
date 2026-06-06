# WARNING: AI SLOP
# This code was LLM-generated

import os

def decode_hardware_mode(version, hw_mode, modify_hw):
    if version == 2:
        mapping = {
            0: "48K Spectrum", 
            1: "48K Spectrum + Interface 1", 
            2: "SamRam", 
            3: "128K Spectrum", 
            4: "128K Spectrum + Interface 1"
        }
    else:  # Version 3
        mapping = {
            0: "48K Spectrum", 
            1: "48K Spectrum + Interface 1", 
            2: "SamRam", 
            3: "48K Spectrum + M.G.T.", 
            4: "128K Spectrum", 
            5: "128K Spectrum + Interface 1", 
            6: "128K Spectrum + M.G.T.", 
            7: "Spectrum +3", 
            8: "Spectrum +3 (Alternative Token)",
            9: "Pentagon 128K", 
            10: "Scorpion 256K", 
            11: "Didaktik-Kompakt",
            12: "Spectrum +2", 
            13: "Spectrum +2A",
            14: "Timex TC2048",
            15: "Timex TC2068",
            128: "Timex TS2068"
        }
    
    machine = mapping.get(hw_mode, f"Unknown Hardware Profile ({hw_mode})")
    
    if modify_hw:
        if "48K" in machine or hw_mode == 14:
            machine += " -> [Modified to 16K Machine]"
        elif "128K" in machine or hw_mode == 12:
            machine += " -> [Modified to Spectrum +2]"
        elif "+3" in machine or hw_mode == 13:
            machine += " -> [Modified to Spectrum +2A]"
            
    return machine

def analyze_z80_file(filepath):
    if not os.path.exists(filepath):
        print(f"Error: File '{filepath}' not found.")
        return

    file_size = os.path.getsize(filepath)

    with open(filepath, "rb") as f:
        # 1. Parse the Base 30-Byte Header (Common to all versions)
        base_header_offset = f.tell()
        base_header = f.read(30)
        if len(base_header) < 30:
            print("Error: File is too small to be a valid .z80 snapshot.")
            return

        # Core Z80 CPU Registers
        reg_a  = base_header[0]
        reg_f  = base_header[1]
        reg_bc = int.from_bytes(base_header[2:4], 'little')
        reg_hl = int.from_bytes(base_header[4:6], 'little')
        pc     = int.from_bytes(base_header[6:8], 'little')
        sp     = int.from_bytes(base_header[8:10], 'little')
        reg_i  = base_header[10]
        reg_r  = base_header[11] & 0x7F
        
        flags1 = base_header[12]
        if flags1 == 255: 
            flags1 = 1
            
        r_bit7         = flags1 & 0x01
        border_color   = (flags1 >> 1) & 0x07
        samrom_v1      = bool(flags1 & 0x10)
        compressed_v1  = bool(flags1 & 0x20)
        full_r_register = reg_r | (r_bit7 << 7)

        reg_de   = int.from_bytes(base_header[13:15], 'little')
        reg_bc_p = int.from_bytes(base_header[15:17], 'little')
        reg_de_p = int.from_bytes(base_header[17:19], 'little')
        reg_hl_p = int.from_bytes(base_header[19:21], 'little')
        reg_a_p  = base_header[21]
        reg_f_p  = base_header[22]
        reg_iy   = int.from_bytes(base_header[23:25], 'little')
        reg_ix   = int.from_bytes(base_header[25:27], 'little')
        iff1     = "EI (Enabled)" if base_header[27] != 0 else "DI (Disabled)"
        iff2     = base_header[28]
        
        flags2         = base_header[29]
        interrupt_mode = flags2 & 0x03
        issue2_emul    = bool(flags2 & 0x04)
        double_int     = bool(flags2 & 0x08)
        
        video_sync_val = (flags2 >> 4) & 0x03
        video_sync     = {1: "High", 3: "Low"}.get(video_sync_val, "Normal")
        
        joystick_val   = (flags2 >> 6) & 0x03
        joystick_type  = {0: "Cursor/Protek/AGF", 1: "Kempston", 2: "Sinclair 2 Left", 3: "Sinclair 2 Right"}.get(joystick_val)

        # Determine Format Version Block
        version = 1
        ext_len = 0
        ext_len_offset = f.tell()
        
        if pc == 0:
            ext_len_bytes = f.read(2)
            if len(ext_len_bytes) == 2:
                ext_len = int.from_bytes(ext_len_bytes, 'little')
                if ext_len == 23:
                    version = 2
                elif ext_len in (54, 55):
                    version = 3
                else:
                    version = f"Unknown Extended Type (Length: {ext_len})"

        # Print Executive Metadata Report
        print("=======================================================================")
        print(f" SOURCE FILE DIAGNOSTIC REPORT: {os.path.basename(filepath)}")
        print("=======================================================================")
        print(f"File Size on Disk : {file_size} bytes")
        print(f"Detected Format   : Version {version}")
        
        print("\n-----------------------------------------------------------------------")
        print(f" Z80 CPU PRIMARY REGISTER DATA [File Offsets: 0x{base_header_offset:02X} - 0x{base_header_offset+29:02X}]")
        print("-----------------------------------------------------------------------")
        print(f" BC: 0x{reg_bc:04X} [Offset: 0x02] | DE: 0x{reg_de:04X} [Offset: 0x13] | HL: 0x{reg_hl:04X} [Offset: 0x04]")
        print(f" SP: 0x{sp:04X} [Offset: 0x08] | IX: 0x{reg_ix:04X} [Offset: 0x25] | IY: 0x{reg_iy:04X} [Offset: 0x23]")
        print(f"  A: 0x{reg_a:02X}   [Offset: 0x00] |  F: 0x{reg_f:02X}   [Offset: 0x01] |  I: 0x{reg_i:02X}   [Offset: 0x0A] | R: 0x{full_r_register:02X} [Offset: 0x0B/0x0C]")
        print(f" Alternate Bank -> BC': 0x{reg_bc_p:04X} [Offset: 0x0F] | DE': 0x{reg_de_p:04X} [Offset: 0x11] | HL': 0x{reg_hl_p:04X} [Offset: 0x13]")
        print(f"                ->  A': 0x{reg_a_p:02X}   [Offset: 0x15] |  F': 0x{reg_f_p:02X}   [Offset: 0x16]")
        print(f" Interrupts     -> State: {iff1} [Offset: 0x1B] | IFF2: {iff2} [Offset: 0x1C] | Mode: IM {interrupt_mode} [Offset: 0x1D]")

        print("\n-----------------------------------------------------------------------")
        print(f" HARDWARE ENVIRONMENT ENVIRONMENT SETTINGS [File Offset: 0x{base_header_offset+12:02X} / 0x{base_header_offset+29:02X}]")
        print("-----------------------------------------------------------------------")
        print(f" Border Color     : {border_color} [Offset: 0x0C, Bits 1-3]")
        print(f" ULA Keyboard     : {'Issue 2 Emulation' if issue2_emul else 'Issue 3 Emulation'} [Offset: 0x1D, Bit 2]")
        print(f" Video Sync       : {video_sync} [Offset: 0x1D, Bits 4-5]")
        print(f" Interrupt Freq   : {'Double Frequency' if double_int else 'Normal (50Hz / Frame)'} [Offset: 0x1D, Bit 3]")
        print(f" Active Joystick  : {joystick_type} [Offset: 0x1D, Bits 6-7]")

        # 2. Process Extended V2/V3 Meta Structural Blocks
        if version in (2, 3):
            ext_header_start = f.tell()
            ext_header_data = f.read(ext_len)
            
            print("\n-----------------------------------------------------------------------")
            print(f" EXTENDED HEADER METADATA BLOCK [File Offsets: 0x{ext_len_offset:02X} - 0x{ext_header_start+ext_len-1:02X}]")
            print("-----------------------------------------------------------------------")
            print(f" Extension Length : {ext_len} bytes [Offset: 0x{ext_len_offset:02X}]")

            if len(ext_header_data) < ext_len:
                print(f"[Error] Extended header truncated: expected {ext_len} bytes, read {len(ext_header_data)}")
                return

            real_pc   = int.from_bytes(ext_header_data[0:2], 'little')
            hw_mode   = ext_header_data[2]
            port_7ffd = ext_header_data[3]
            if1_paged = ext_header_data[4] == 0xFF
            
            flags37   = ext_header_data[5]
            r_emul    = bool(flags37 & 0x01)
            ldir_emul = bool(flags37 & 0x02)
            ay_in_use = bool(flags37 & 0x04)
            fuller_bx = bool(flags37 & 0x40) if ay_in_use else False
            modify_hw = bool(flags37 & 0x80)

            port_fffd = ext_header_data[6]
            ay_registers = list(ext_header_data[7:23])

            machine_profile = decode_hardware_mode(version, hw_mode, modify_hw)

            print(f" Actual PC Address: 0x{real_pc:04X} [Offset: 0x{ext_header_start:02X}]")
            print(f" Decoded Hardware : {machine_profile} [Offset: 0x{ext_header_start+2:02X}]")
            print(f" Interface 1 ROM  : {'Paged In' if if1_paged else 'Disabled'} [Offset: 0x{ext_header_start+4:02X}]")
            print(f" Engine Options   : R-Emulation: {r_emul} | LDIR-Emulation: {ldir_emul} [Offset: 0x{ext_header_start+5:02X}]")
            print(f" Last Out 0x7FFD  : 0x{port_7ffd:02X} (Memory Pagination Port) [Offset: 0x{ext_header_start+3:02X}]")

            if ay_in_use:
                print("\n-----------------------------------------------------------------------")
                print(f" AY-3-8912 PSG AUDIO CHIP METADATA [File Offsets: 0x{ext_header_start+6:02X} - 0x{ext_header_start+22:02X}]")
                print("-----------------------------------------------------------------------")
                print(f" Audio Expansion  : {'Fuller Audio Box Emulation' if fuller_bx else 'Standard AY Sound Engine'} [Offset: 0x{ext_header_start+5:02X}, Bit 6]")
                print(f" Active Register  : Selected Target Register Index -> {port_fffd} [Offset: 0x{ext_header_start+6:02X}]")
                print(f" Current Sound Register State Dump [Offsets: 0x{ext_header_start+7:02X} - 0x{ext_header_start+22:02X}]:")
                print(f"  R0-R3  (Ch A/B Fine/Coarse Pitch): {ay_registers[0:4]}")
                print(f"  R4-R5  (Ch C Fine/Coarse Pitch)  : {ay_registers[4:6]}")
                print(f"  R6     (Noise Generator Period)  : {ay_registers[6]}")
                print(f"  R7     (Mixer Enable State)      : 0x{ay_registers[7]:02X}")
                print(f"  R8-R10 (Ch A/B/C Volume Levels)  : {ay_registers[8:11]}")
                print(f"  R11-R12(Envelope Fine/Coarse)    : {ay_registers[11:13]}")
                print(f"  R13    (Envelope Shape Cycle)    : {ay_registers[13]}")
                print(f"  R14-R15(I/O Ports State Matrix)  : {ay_registers[14:16]}")

            # 3. Process Version 3 Only Structures
            if version == 3 and ext_len >= 54:
                t_state_low  = int.from_bytes(ext_header_data[23:25], 'little')
                t_state_high = ext_header_data[25]
                mgt_paged    = ext_header_data[27] == 0xFF
                mf_paged     = ext_header_data[28] == 0xFF
                rom_0_8191   = "ROM" if ext_header_data[29] == 0xFF else "RAM"
                rom_8192_    = "ROM" if ext_header_data[30] == 0xFF else "RAM"
                
                mgt_val      = ext_header_data[51]
                mgt_type     = {0: "Disciple + Epson", 1: "Disciple + HP", 16: "Plus D"}.get(mgt_val, f"Unknown ({mgt_val})")
                
                print("\n-----------------------------------------------------------------------")
                print(f" VERSION 3 EXTENDED SYSTEM ARCHITECTURE [File Offsets: 0x{ext_header_start+23:02X} - 0x{ext_header_start+53:02X}]")
                print("-----------------------------------------------------------------------")
                print(f" T-State Counter  : High Modulo: {t_state_high} [Offset: 0x{ext_header_start+25:02X}] | Low Counter: {t_state_low} [Offset: 0x{ext_header_start+23:02X}]")
                print(f" Extra Hardware   : MGT Rom: {'Active' if mgt_paged else 'Inactive'} [Offset: 0x{ext_header_start+27:02X}] (Type: {mgt_type} [Offset: 0x{ext_header_start+51:02X}])")
                print(f" Multiface Rom    : {'Paged In (Warning: Crash Risk)' if mf_paged else 'Inactive'} [Offset: 0x{ext_header_start+28:02X}]")
                print(f" Base Memory Maps : 0-8191 Slot: {rom_0_8191} [Offset: 0x{ext_header_start+29:02X}] | 8192-16383 Slot: {rom_8192_} [Offset: 0x{ext_header_start+30:02X}]")
                
                if ext_len == 55:
                    port_1ffd = ext_header_data[54]
                    print(f" Last Out 0x1FFD  : 0x{port_1ffd:02X} (+3 Memory Control Port) [Offset: 0x{ext_header_start+54:02X}]")

            # 4. Map the Memory Blocks Layout Iteratively
            print("\n-----------------------------------------------------------------------")
            print(" SNAPSHOT MEMORY BLOCKS COMPONENT EXTRACTION")
            print("-----------------------------------------------------------------------")
            block_idx = 0
            while f.tell() < file_size:
                block_start_offset = f.tell()
                blk_header = f.read(3)
                if len(blk_header) < 3:
                    break
                
                comp_len = blk_header[0] | (blk_header[1] << 8)
                page_num = blk_header[2]
                
                is_comp = comp_len != 0xFFFF
                disk_size = 16384 if comp_len == 0xFFFF else comp_len
                payload_offset = block_start_offset + 3
                
                print(f" Bank Block #{block_idx:02d} -> "
                      f"Header Info @ [Offset: 0x{block_start_offset:06X}] | "
                      f"Memory Page ID: {page_num:2d} | "
                      f"Status: {'Compressed RLE' if is_comp else 'Raw Uncompressed'} | "
                      f"Payload Data @ [Offset: 0x{payload_offset:06X} - 0x{payload_offset+disk_size-1:06X}] ({disk_size:5d} bytes)")
                
                f.seek(disk_size, 1) # Advance stream safely over page payload data
                block_idx += 1
                
        else:
            # Layout processing for Classic Version 1 Snapshots
            print(f" Actual PC Address: 0x{pc:04X}")
            print(f" Decoded Hardware : Implicitly 48K Sinclair Spectrum Machine")
            print("\n-----------------------------------------------------------------------")
            print(" SNAPSHOT MEMORY BLOCKS COMPONENT EXTRACTION")
            print("-----------------------------------------------------------------------")
            ram_payload = file_size - 30
            payload_start_offset = f.tell()
            print(f" Structure Type   : Linear 48KB RAM Dump")
            print(f" Allocation Type  : {'Compressed RLE Byte Stream' if compressed_v1 else 'Raw Uncompressed Base Memory'}")
            print(f" Payload Range    : [Offset: 0x{payload_start_offset:02X} - 0x{file_size-1:02X}] ({ram_payload} data bytes remaining to EOF)")
            if samrom_v1:
                print(" Active Subsystem : SamRom Paged In")

        print("=======================================================================\n")

analyze_z80_file("../../Z80-Firmware/TestFiles/z80/48k/GYRSCOPE.z80")

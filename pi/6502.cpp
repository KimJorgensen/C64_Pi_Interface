/*****************************************************
 * Based on
 * Fake6502 CPU emulator core v1.1 *******************
 * (c)2011 Mike Chambers (miker00lz@gmail.com)       *
 *****************************************************/

#include <stdio.h>
#include <stdint.h>

// 6502 defines
#define FLAG_CARRY     0x01
#define FLAG_ZERO      0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL   0x08
#define FLAG_BREAK     0x10
#define FLAG_CONSTANT  0x20
#define FLAG_OVERFLOW  0x40
#define FLAG_SIGN      0x80

#define BASE_STACK     0x100

// flag modifier macros
#define setcarry() cpu.status |= FLAG_CARRY
#define clearcarry() cpu.status &= (~FLAG_CARRY)
#define setzero() cpu.status |= FLAG_ZERO
#define clearzero() cpu.status &= (~FLAG_ZERO)
#define setinterrupt() cpu.status |= FLAG_INTERRUPT
#define clearinterrupt() cpu.status &= (~FLAG_INTERRUPT)
#define setdecimal() cpu.status |= FLAG_DECIMAL
#define cleardecimal() cpu.status &= (~FLAG_DECIMAL)
#define setoverflow() cpu.status |= FLAG_OVERFLOW
#define clearoverflow() cpu.status &= (~FLAG_OVERFLOW)
#define setsign() cpu.status |= FLAG_SIGN
#define clearsign() cpu.status &= (~FLAG_SIGN)

// flag calculation macros
#define zerocalc(n) \
    if ((n) & 0x00FF) clearzero();\
        else setzero()

#define signcalc(n) \
    if ((n) & 0x0080) setsign();\
        else clearsign()

#define carrycalc(n) \
    if ((n) & 0xFF00) setcarry();\
        else clearcarry()

#define overflowcalc(n, m, o)   /* n = result, m = accumulator, o = memory */ \
    if (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080) setoverflow();\
        else clearoverflow()

struct mos6502_state
{
    // 6502 bus signals
    bool write;
    uint16_t addr;
    uint8_t data;

    // 6502 CPU registers
    uint16_t pc;
    uint8_t sp, a, x, y, status;

    // internal variables
    uint8_t opcode, cycle;
    uint16_t temp;
};

static mos6502_state cpu;

static void reset6502()
{
    cpu.write = false;
    cpu.addr = 0xFFFC;  // Reset vector
    cpu.data = 0;
    cpu.opcode = 0;     // BRK
    cpu.cycle = 5;      // last cycles of BRK is similar to RESET
    cpu.temp = 0;

    cpu.pc = 0;
    cpu.a = 0;
    cpu.x = 0;
    cpu.y = 0;
    cpu.sp = 0xfd;
    cpu.status = 0;
}

#define set_value(value)    \
    zerocalc(value);        \
    signcalc(value)

#define set_a(value)        \
    cpu.a = value;          \
    set_value(cpu.a)

#define set_x(value)        \
    cpu.x = value;          \
    set_value(cpu.x)

#define set_y(value)        \
    cpu.y = value;          \
    set_value(cpu.y)

#define inc()               \
    cpu.temp++;             \
    set_value(cpu.temp);    \
    cpu.data = cpu.temp

#define dec()               \
    cpu.temp--;             \
    set_value(cpu.temp);    \
    cpu.data = cpu.temp

#define cmp_register(reg)       \
    cpu.temp = reg - cpu.data;  \
    signcalc(cpu.temp);         \
    zerocalc(cpu.temp);         \
    if (cpu.temp & 0xFF00)      \
        clearcarry();           \
    else                        \
        setcarry()

#define cmp_a()                 \
    cmp_register(cpu.a);        \
    cpu.addr = ++cpu.pc

#define cmp_x()                 \
    cmp_register(cpu.x);        \
    cpu.addr = ++cpu.pc;

#define cmp_y()                 \
    cmp_register(cpu.y);        \
    cpu.addr = ++cpu.pc;

// BCD implementation by Mike B.
// http://forum.6502.org/viewtopic.php?f=2&t=2052#p37758
#define adc_a()                                 \
    cpu.temp = cpu.a + cpu.data +               \
      (cpu.status & FLAG_CARRY);                \
    overflowcalc(cpu.temp, cpu.a, cpu.data);    \
    set_value(cpu.temp);                        \
    if (cpu.status & FLAG_DECIMAL)              \
        cpu.temp += ((((cpu.temp + 0x66) ^      \
        (uint16_t)cpu.a ^ cpu.data) >> 3) &     \
        0x22) * 3;                              \
    carrycalc(cpu.temp);                        \
    cpu.a = cpu.temp;                           \
    cpu.addr = ++cpu.pc;

#define sbc_a()                     \
    cpu.data = ~cpu.data;           \
    if (cpu.status & FLAG_DECIMAL)  \
        cpu.data -= 0x66;           \
    adc_a()

#define or_a()                      \
    set_a(cpu.a | cpu.data);        \
    cpu.addr = ++cpu.pc

#define and_a()                     \
    set_a(cpu.a & cpu.data);        \
    cpu.addr = ++cpu.pc

#define xor_a()                     \
    set_a(cpu.a ^ cpu.data);        \
    cpu.addr = ++cpu.pc

#define asl()                       \
    cpu.temp <<= 1;                 \
    carrycalc(cpu.temp);            \
    set_value(cpu.temp);            \
    cpu.data = cpu.temp

#define lsr()                       \
    if (cpu.temp & 0x0001)          \
        setcarry();                 \
    else clearcarry();              \
    cpu.temp >>= 1;                 \
    set_value(cpu.temp);            \
    cpu.data = cpu.temp

#define rol()                       \
    cpu.temp = (cpu.temp << 1) |    \
        (cpu.status & FLAG_CARRY);  \
    carrycalc(cpu.temp);            \
    set_value(cpu.temp);            \
    cpu.data = cpu.temp

#define ror()                       \
    cpu.temp |= ((cpu.status &      \
        FLAG_CARRY) << 8);          \
    if (cpu.temp & 0x0001)          \
        setcarry();                 \
    else clearcarry();              \
    cpu.temp >>= 1;                 \
    set_value(cpu.temp);            \
    cpu.data = cpu.temp

#define branch_if(cond)             \
    cpu.temp = (int8_t)cpu.data;    \
    cpu.addr = ++cpu.pc;            \
    if (cond)                       \
    {                               \
        /* no branch taken */       \
        cpu.cycle += 2;             \
    }

#define branch_if_set(flag)         \
    branch_if(cpu.status & flag)

#define branch_if_clear(flag)       \
    branch_if((cpu.status & flag) == 0)

#define next_opcode()       \
    cpu.opcode = cpu.data;  \
    cpu.cycle = 0;          \
    cpu.addr = ++cpu.pc

static void step6502_cycle1()
{
    switch (cpu.opcode)
    {
        case 0x00: // BRK 7
            cpu.write = true;
            cpu.addr = BASE_STACK + cpu.sp;
            cpu.data = ++cpu.pc >> 8;
            break;

        case 0x08: // PHP 3
            cpu.write = true;
            cpu.addr = BASE_STACK + cpu.sp;
            --cpu.sp;
            cpu.data = cpu.status | FLAG_BREAK|FLAG_CONSTANT;
            break;

        case 0x48: // PHA 3
            cpu.write = true;
            cpu.addr = BASE_STACK + cpu.sp;
            --cpu.sp;
            cpu.data = cpu.a;
            break;

        case 0x20: // JSR abs 6
            cpu.temp = cpu.data;
            ++cpu.pc;
            cpu.addr = BASE_STACK + cpu.sp;
            break;

        case 0x28: // PLP 4
        case 0x40: // RTI 6
        case 0x60: // RTS 6
        case 0x68: // PLA 4
            cpu.addr = BASE_STACK + cpu.sp;
            break;

        case 0x09: // ORA imm 2
            or_a();
            break;

        case 0x0a: // ASL 2
            cpu.temp = cpu.a << 1;
            set_a(cpu.temp);
            carrycalc(cpu.temp);
            break;

        case 0x0b: // (ANC imm 2)
            and_a();
            if (cpu.data & 0x80) setcarry();
            else clearcarry();
            break;

        case 0x18: // CLC 2
            clearcarry();
            break;

        case 0x1a: // (NOP 2):
            break;

        case 0x29: // AND imm 2
            and_a();
            break;

        case 0x2a: // ROL 2
            cpu.temp = (cpu.a << 1) | (cpu.status & FLAG_CARRY);
            set_a(cpu.temp);
            carrycalc(cpu.temp);
            break;

        case 0x38: // SEC 2
            setcarry();
            break;

        case 0x49: // EOR imm 2
            xor_a();
            break;

        case 0x4a: // LSR 2
            if (cpu.a & 0x01) setcarry();
            else clearcarry();

            set_a(cpu.a >> 1);
            cpu.data = cpu.temp;
            break;

        case 0x58: // CLI 2
            clearinterrupt();
            break;

        case 0x69: // ADC imm 2
            adc_a();
            break;

        case 0x6A: // ROR 2
            cpu.temp = cpu.a | ((cpu.status & FLAG_CARRY) << 8);
            if (cpu.temp & 0x0001) setcarry();
            else clearcarry();

            cpu.temp >>= 1;
            set_a(cpu.temp);
            cpu.data = cpu.temp;
            break;

        case 0x78: // SEI 2
            setinterrupt();
            break;

        case 0x84: // STY zp 3
            cpu.write = true;
            cpu.addr = cpu.data;
            cpu.data = cpu.y;
            break;

        case 0x85: // STA zp 3
            cpu.write = true;
            cpu.addr = cpu.data;
            cpu.data = cpu.a;
            break;

        case 0x86: // STX zp 3
            cpu.write = true;
            cpu.addr = cpu.data;
            cpu.data = cpu.x;
            break;

        case 0x87: // (SAX zp 3)
            cpu.write = true;
            cpu.addr = cpu.data;
            cpu.data = cpu.a & cpu.x;
            break;

        case 0x88: // DEY 2
            set_y(cpu.y-1);
            break;

        case 0x8a: // TXA 2
            set_a(cpu.x);
            break;

        case 0x98: // TYA 2
            set_a(cpu.y);
            break;

        case 0x9a: // TXS 2
            cpu.sp = cpu.x;
            break;

        case 0xa0: // LDY imm 2
            set_y(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xa2: // LDX imm 2
            set_x(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xa8: // TAY 2
            set_y(cpu.a);
            break;

        case 0xa9: // LDA imm 2
            set_a(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xaa: // TAX 2
            set_x(cpu.a);
            break;

        case 0xb8: // CLV 2
            clearoverflow();
            break;

        case 0xba: // TSX 2
            set_x(cpu.sp);
            break;

        case 0xc0: // CPY imm 2
            cmp_y();
            break;

        case 0xc2: // (NOP imm 2)
            // NOP
            cpu.addr = ++cpu.pc;
            break;

        case 0xc8: // INY 2
            set_y(cpu.y+1);
            break;

        case 0xc9: // CMP imm 2
            cmp_a();
            break;

        case 0xca: // DEX 2
            set_x(cpu.x-1);
            break;

        case 0xcb: // (AXS imm 2)
            cmp_register((cpu.a & cpu.x));
            cpu.x = cpu.temp;
            cpu.addr = ++cpu.pc;
            break;

        case 0xd8: // CLD 2
            cleardecimal();
            break;

        case 0xf8: // SED 2
            setdecimal();
            break;

        case 0xe0: // CPX imm 2
            cmp_x();
            break;

        case 0xe8: // INX 2
            set_x(cpu.x+1);
            break;

        case 0xe9: // SBC imm 2
            sbc_a();
            break;

        case 0xea: // NOP 2
            // NOP
            break;

        case 0x10: // BPL rel 2*
            branch_if_set(FLAG_SIGN);
            break;

        case 0x30: // BMI rel 2*
            branch_if_clear(FLAG_SIGN);
            break;

        case 0x50: // BVC rel 2*
            branch_if_set(FLAG_OVERFLOW);
            break;

        case 0x70: // BVS rel 2*
            branch_if_clear(FLAG_OVERFLOW);
            break;

        case 0x90: // BCC rel 2*
            branch_if_set(FLAG_CARRY);
            break;

        case 0xb0: // BCS rel 2*
            branch_if_clear(FLAG_CARRY);
            break;

        case 0xd0: // BNE rel 2*
            branch_if_set(FLAG_ZERO);
            break;

        case 0xf0: // BEQ rel 2*
            branch_if_clear(FLAG_ZERO);
            break;

        case 0x04: // (NOP zp 3)
        case 0x05: // ORA zp 3
        case 0x06: // ASL zp 5
        case 0x11: // ORA izy 5*
        case 0x15: // ORA zpx 4
        case 0x16: // ASL zpx 6
        case 0x21: // AND izx 6
        case 0x24: // BIT zp 3
        case 0x25: // AND zp 3
        case 0x26: // ROL zp 5
        case 0x31: // AND izy 5*
        case 0x35: // AND zpx 4
        case 0x45: // EOR zp 3
        case 0x46: // LSR zp 5
        case 0x51: // EOR izy 5*
        case 0x55: // EOR zpx 4
        case 0x56: // LSR zpx 6
        case 0x65: // ADC zp 3
        case 0x66: // ROR zp 5
        case 0x71: // ADC izy 5*
        case 0x75: // ADC zpx 4
        case 0x76: // ROR zpx 6
        case 0x91: // STA izy 6
        case 0x94: // STY zpx 4
        case 0x95: // STA zpx 4
        case 0x97: // (SAX zpy 4)
        case 0xa1: // LDA izx 6
        case 0xa4: // LDY zp 3
        case 0xa5: // LDA zp 3
        case 0xa6: // LDX zp 3
        case 0xa7: // (LAX zp 3)
        case 0xb1: // LDA izy 5*
        case 0xb4: // LDY zpx 4
        case 0xb5: // LDA zpx 4
        case 0xb6: // LDX zpy 4
        case 0xc1: // CMP izx 6
        case 0xc3: // (DCP izx 8)
        case 0xc4: // CPY zp 3
        case 0xc5: // CMP zp 3
        case 0xc6: // DEC zp 5
        case 0xd1: // CMP izy 5*
        case 0xd5: // CMP zpx 4
        case 0xd6: // DEC zpx 6
        case 0xe4: // CPX zp 3
        case 0xe5: // SBC zp 3
        case 0xe6: // INC zp 5
        case 0xe7: // (ISC zp 5)
        case 0xf1: // SBC izy 5*
        case 0xf5: // SBC zpx 4
        case 0xf6: // INC zpx 6
            cpu.addr = cpu.data;
            break;

        case 0x0d: // ORA abs 4
        case 0x0e: // ASL abs 6
        case 0x19: // ORA aby 4*
        case 0x1d: // ORA abx 4*
        case 0x1e: // ASL abx 7
        case 0x2c: // BIT abs 4
        case 0x2d: // AND abs 4
        case 0x2e: // ROL abs 6
        case 0x39: // AND aby 4*
        case 0x3c: // (NOP abx 4*)
        case 0x3d: // AND abx 4*
        case 0x3e: // ROL abx 7
        case 0x4c: // JMP abs 3
        case 0x4d: // EOR abs 4
        case 0x4e: // LSR abs 6
        case 0x59: // EOR aby 4*
        case 0x5d: // EOR abx 4*
        case 0x5e: // LSR abx 7
        case 0x6c: // JMP ind 5
        case 0x6d: // ADC abs 4
        case 0x6e: // ROR abs 6
        case 0x79: // ADC aby 4*
        case 0x7d: // ADC abx 4*
        case 0x7e: // ROR abx 7
        case 0x81: // STA izx 6
        case 0x8c: // STY abs 4
        case 0x8d: // STA abs 4
        case 0x8e: // STX abs 4
        case 0x99: // STA aby 5
        case 0x9d: // STA abx 5
        case 0xac: // LDY abs 4
        case 0xad: // LDA abs 4
        case 0xae: // LDX abs 4
        case 0xaf: // (LAX abs 4)
        case 0xb9: // LDA aby 4*
        case 0xbc: // LDY abx 4*
        case 0xbd: // LDA abx 4*
        case 0xbe: // LDX aby 4*
        case 0xcc: // CPY abs 4
        case 0xcd: // CMP abs 4
        case 0xce: // DEC abs 6
        case 0xd9: // CMP aby 4*
        case 0xdd: // CMP abx 4*
        case 0xde: // DEC abx 7
        case 0xec: // CPX abs 4
        case 0xed: // SBC abs 4
        case 0xee: // INC abs 6
        case 0xf9: // SBC aby 4*
        case 0xfc: // (NOP abx 4*)
        case 0xfd: // SBC abx 4*
        case 0xfe: // INC abx 7
        case 0xff: // (ISC abx 7)
            cpu.temp = cpu.data;
            cpu.addr = ++cpu.pc;
            break;

        default:
            printf("Unhandled opcode %02x cycle: %d\n", cpu.opcode, cpu.cycle);
            break;
    }
}

static void step6502_cycle2()
{
    switch (cpu.opcode)
    {
        case 0x00: // BRK 7
            cpu.addr = BASE_STACK + --cpu.sp;
            cpu.data = cpu.pc;
            break;

        case 0x04: // (NOP zp 3)
            cpu.addr = ++cpu.pc;
            break;

        case 0x05: // ORA zp 3
            or_a();
            break;

        case 0x06: // ASL zp 5
        case 0x26: // ROL zp 5
        case 0x46: // LSR zp 5
        case 0x66: // ROR zp 5
        case 0xc6: // DEC zp 5
        case 0xe6: // INC zp 5
        case 0xe7: // (ISC zp 5)
            cpu.temp = cpu.data;
            cpu.write = true;
            break;

        case 0x08: // PHP 3
        case 0x48: // PHA 3
            cpu.write = false;
            cpu.addr = cpu.pc;
            break;

        case 0x0d: // ORA abs 4
        case 0x0e: // ASL abs 6
        case 0x2c: // BIT abs 4
        case 0x2d: // AND abs 4
        case 0x2e: // ROL abs 6
        case 0x4d: // EOR abs 4
        case 0x4e: // LSR abs 6
        case 0x6c: // JMP ind 5
        case 0x6d: // ADC abs 4
        case 0x6e: // ROR abs 6
        case 0xac: // LDY abs 4
        case 0xad: // LDA abs 4
        case 0xae: // LDX abs 4
        case 0xaf: // (LAX abs 4)
        case 0xcc: // CPY abs 4
        case 0xcd: // CMP abs 4
        case 0xce: // DEC abs 6
        case 0xec: // CPX abs 4
        case 0xed: // SBC abs 4
        case 0xee: // INC abs 6
            cpu.addr = (cpu.data << 8) | (uint8_t)cpu.temp;
            break;

        case 0x10: // BPL rel 2*
        case 0x30: // BMI rel 2*
        case 0x50: // BVC rel 2*
        case 0x70: // BVS rel 2*
        case 0x90: // BCC rel 2*
        case 0xb0: // BCS rel 2*
        case 0xd0: // BNE rel 2*
        case 0xf0: // BEQ rel 2*
            cpu.temp += (cpu.pc & 0x00FF);
            cpu.pc = (cpu.pc & 0xFF00) | (uint8_t)cpu.temp;
            cpu.addr = cpu.pc;

            // Add 1 cycle if page boundary is crossed
            if ((cpu.temp & 0xFF00) == 0)
            {
                cpu.cycle++;
            }
            break;

        case 0x11: // ORA izy 5*
        case 0x31: // AND izy 5*
        case 0x51: // EOR izy 5*
        case 0x71: // ADC izy 5*
        case 0x91: // STA izy 6
        case 0xb1: // LDA izy 5*
        case 0xd1: // CMP izy 5*
        case 0xf1: // SBC izy 5*
            cpu.temp = cpu.data;
            cpu.addr = (uint8_t)(cpu.addr + 1);
            break;

        case 0x15: // ORA zpx 4
        case 0x16: // ASL zpx 6
        case 0x21: // AND izx 6
        case 0x35: // AND zpx 4
        case 0x55: // EOR zpx 4
        case 0x56: // LSR zpx 6
        case 0x75: // ADC zpx 4
        case 0x76: // ROR zpx 6
        case 0x81: // STA izx 6
        case 0xa1: // LDA izx 6
        case 0xb4: // LDY zpx 4
        case 0xb5: // LDA zpx 4
        case 0xc1: // CMP izx 6
        case 0xc3: // (DCP izx 8)
        case 0xd5: // CMP zpx 4
        case 0xd6: // DEC zpx 6
        case 0xf5: // SBC zpx 4
        case 0xf6: // INC zpx 6
            cpu.addr = (uint8_t)(cpu.addr + cpu.x);
            break;

        case 0x19: // ORA aby 4*
        case 0x39: // AND aby 4*
        case 0x59: // EOR aby 4*
        case 0x79: // ADC aby 4*
        case 0xb9: // LDA aby 4*
        case 0xbe: // LDX aby 4*
        case 0xd9: // CMP aby 4*
        case 0xf9: // SBC aby 4*
            cpu.temp += cpu.y;
            cpu.addr = (cpu.data << 8) | (uint8_t)cpu.temp;

            // Add 1 cycle if page boundary is crossed
            if ((cpu.temp & 0xFF00) == 0)
            {
                cpu.cycle++;
            }
            break;

        case 0x1d: // ORA abx 4*
        case 0x3c: // (NOP abx 4*)
        case 0x3d: // AND abx 4*
        case 0x5d: // EOR abx 4*
        case 0x7d: // ADC abx 4*
        case 0xbc: // LDY abx 4*
        case 0xbd: // LDA abx 4*
        case 0xdd: // CMP abx 4*
        case 0xfc: // (NOP abx 4*)
        case 0xfd: // SBC abx 4*
            cpu.temp += cpu.x;
            cpu.addr = (cpu.data << 8) | (uint8_t)cpu.temp;

            // Add 1 cycle if page boundary is crossed
            if ((cpu.temp & 0xFF00) == 0)
            {
                cpu.cycle++;
            }
            break;

        case 0x1e: // ASL abx 7
        case 0x3e: // ROL abx 7
        case 0x5e: // LSR abx 7
        case 0x7e: // ROR abx 7
        case 0x9d: // STA abx 5
        case 0xde: // DEC abx 7
        case 0xfe: // INC abx 7
        case 0xff: // (ISC abx 7)
            cpu.temp += cpu.x;
            cpu.addr = (cpu.data << 8) | (uint8_t)cpu.temp;
            break;

        case 0x20: // JSR abs 6
            cpu.write = true;
            cpu.data = cpu.pc >> 8;
            break;

        case 0x24: // BIT zp 3
            zerocalc(cpu.a & cpu.data);
            cpu.status = (cpu.status & 0x3F) | (cpu.data & 0xC0);
            cpu.addr = ++cpu.pc;
            break;

        case 0x25: // AND zp 3
            and_a();
            break;

        case 0x28: // PLP 4
        case 0x68: // PLA 4
            cpu.addr = BASE_STACK + ++cpu.sp;
            cpu.temp = cpu.data;
            break;

        case 0x40: // RTI 6
        case 0x60: // RTS 6
            cpu.addr = BASE_STACK + ++cpu.sp;
            break;

        case 0x45: // EOR zp 3
            xor_a();
            break;

        case 0x4c: // JMP abs 3
            cpu.pc = (cpu.data << 8) | (uint8_t)cpu.temp;
            cpu.addr = cpu.pc;
            break;

        case 0x65: // ADC zp 3
            adc_a();
            break;

        case 0x84: // STY zp 3
        case 0x85: // STA zp 3
        case 0x86: // STX zp 3
        case 0x87: // (SAX zp 3)
            cpu.write = false;
            cpu.addr = ++cpu.pc;
            break;

        case 0x8c: // STY abs 4
            cpu.write = true;
            cpu.addr = (cpu.data << 8) | (uint8_t)cpu.temp;
            cpu.data = cpu.y;
            break;

        case 0x8d: // STA abs 4
            cpu.write = true;
            cpu.addr = (cpu.data << 8) | (uint8_t)cpu.temp;
            cpu.data = cpu.a;
            break;

        case 0x8e: // STX abs 4
            cpu.write = true;
            cpu.addr = (cpu.data << 8) | (uint8_t)cpu.temp;
            cpu.data = cpu.x;
            break;

        case 0x94: // STY zpx 4
            cpu.write = true;
            cpu.addr = (uint8_t)(cpu.addr + cpu.x);
            cpu.data = cpu.y;
            break;

        case 0x95: // STA zpx 4
            cpu.write = true;
            cpu.addr = (uint8_t)(cpu.addr + cpu.x);
            cpu.data = cpu.a;
            break;

        case 0x97: // (SAX zpy 4)
            cpu.write = true;
            cpu.addr = (uint8_t)(cpu.addr + cpu.y);
            cpu.data = cpu.a & cpu.x;
            break;

        case 0x99: // STA aby 5
            cpu.temp += cpu.y;
            cpu.addr = (cpu.data << 8) | (uint8_t)cpu.temp;
            break;

        case 0xa4: // LDY zp 3
            set_y(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xa5: // LDA zp 3
            set_a(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xa6: // LDX zp 3
            set_x(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xa7: // (LAX zp 3)
            cpu.a = cpu.data;
            set_x(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xb6: // LDX zpy 4
            cpu.addr = (uint8_t)(cpu.addr + cpu.y);
            break;

        case 0xc4: // CPY zp 3
            cmp_y();
            break;

        case 0xc5: // CMP zp 3
            cmp_a();
            break;

        case 0xe4: // CPX zp 3
            cmp_x();
            break;

        case 0xe5: // SBC zp 3
            sbc_a();
            break;

        default:
            next_opcode();
            break;
    }
}

static void step6502_cycle3()
{
    switch (cpu.opcode)
    {
        case 0x00: // BRK 7
            cpu.addr = BASE_STACK + --cpu.sp;
            --cpu.sp;
            cpu.data = cpu.status | FLAG_BREAK|FLAG_CONSTANT;
            break;

        case 0x06: // ASL zp 5
            asl();
            break;

        case 0x0d: // ORA abs 4
        case 0x15: // ORA zpx 4
            or_a();
            break;

        case 0x0e: // ASL abs 6
        case 0x16: // ASL zpx 6
        case 0x2e: // ROL abs 6
        case 0x4e: // LSR abs 6
        case 0x56: // LSR zpx 6
        case 0x6e: // ROR abs 6
        case 0x76: // ROR zpx 6
        case 0xce: // DEC abs 6
        case 0xd6: // DEC zpx 6
        case 0xee: // INC abs 6
        case 0xf6: // INC zpx 6
            cpu.temp = cpu.data;
            cpu.write = true;
            break;

        case 0x10: // BPL rel 2*
        case 0x30: // BMI rel 2*
        case 0x50: // BVC rel 2*
        case 0x70: // BVS rel 2*
        case 0x90: // BCC rel 2*
        case 0xb0: // BCS rel 2*
        case 0xd0: // BNE rel 2*
        case 0xf0: // BEQ rel 2*
            cpu.pc += (cpu.temp & 0xFF00);
            cpu.addr = cpu.pc;
            break;

        case 0x11: // ORA izy 5*
        case 0x31: // AND izy 5*
        case 0x51: // EOR izy 5*
        case 0x71: // ADC izy 5*
        case 0xb1: // LDA izy 5*
        case 0xd1: // CMP izy 5*
        case 0xf1: // SBC izy 5*
            cpu.temp += cpu.y;
            cpu.addr = (cpu.data << 8) | (uint8_t)cpu.temp;

            // Add 1 cycle if page boundary is crossed
            if ((cpu.temp & 0xFF00) == 0)
            {
                cpu.cycle++;
            }
            break;

        case 0x19: // ORA aby 4*
        case 0x1d: // ORA abx 4*
        case 0x1e: // ASL abx 7
        case 0x39: // AND aby 4*
        case 0x3c: // (NOP abx 4*)
        case 0x3d: // AND abx 4*
        case 0x3e: // ROL abx 7
        case 0x59: // EOR aby 4*
        case 0x5d: // EOR abx 4*
        case 0x5e: // LSR abx 7
        case 0x79: // ADC aby 4*
        case 0x7d: // ADC abx 4*
        case 0x7e: // ROR abx 7
        case 0xb9: // LDA aby 4*
        case 0xbc: // LDY abx 4*
        case 0xbd: // LDA abx 4*
        case 0xbe: // LDX aby 4*
        case 0xd9: // CMP aby 4*
        case 0xdd: // CMP abx 4*
        case 0xde: // DEC abx 7
        case 0xf9: // SBC aby 4*
        case 0xfc: // (NOP abx 4*)
        case 0xfd: // SBC abx 4*
        case 0xfe: // INC abx 7
        case 0xff: // (ISC abx 7)
            cpu.addr += (cpu.temp & 0xFF00);
            break;

        case 0x20: // JSR abs 6
            cpu.addr = BASE_STACK + --cpu.sp;
            --cpu.sp;
            cpu.data = cpu.pc;
            break;

        case 0x21: // AND izx 6
        case 0x81: // STA izx 6
        case 0xa1: // LDA izx 6
        case 0xc1: // CMP izx 6
        case 0xc3: // (DCP izx 8)
            cpu.temp = cpu.data;
            cpu.addr = (uint8_t)(cpu.addr + 1);
            break;

        case 0x26: // ROL zp 5
            rol();
            break;

        case 0x28: // PLP 4
            cpu.status = cpu.data;
            cpu.addr = cpu.pc;
            break;

        case 0x2c: // BIT abs 4
            zerocalc(cpu.a & cpu.data);
            cpu.status = (cpu.status & 0x3F) | (cpu.data & 0xC0);
            cpu.addr = ++cpu.pc;
            break;

        case 0x2d: // AND abs 4
        case 0x35: // AND zpx 4
            and_a();
            break;

        case 0x40: // RTI 6
            cpu.addr = BASE_STACK + ++cpu.sp;
            cpu.status = cpu.data;
            break;

        case 0x46: // LSR zp 5
            lsr();
            break;

        case 0x4d: // EOR abs 4
        case 0x55: // EOR zpx 4
            xor_a();
            break;

        case 0x60: // RTS 6
            cpu.addr = BASE_STACK + ++cpu.sp;
            cpu.temp = cpu.data;
            break;

        case 0x66: // ROR zp 5
            ror();
            break;

        case 0x68: // PLA 4
            set_a(cpu.data);
            cpu.addr = cpu.pc;
            break;

        case 0x6c: // JMP ind 5
            cpu.temp = cpu.data;
            cpu.addr++;
            break;

        case 0x6d: // ADC abs 4
        case 0x75: // ADC zpx 4
            adc_a();
            break;

        case 0x8c: // STY abs 4
        case 0x8d: // STA abs 4
        case 0x8e: // STX abs 4
        case 0x94: // STY zpx 4
        case 0x95: // STA zpx 4
        case 0x97: // (SAX zpy 4)
            cpu.write = false;
            cpu.addr = ++cpu.pc;
            break;

        case 0x91: // STA izy 6
            cpu.temp += cpu.y;
            cpu.addr = (cpu.data << 8) | (uint8_t)cpu.temp;
            break;

        case 0x99: // STA aby 5
        case 0x9d: // STA abx 5
            cpu.write = true;
            cpu.addr += (cpu.temp & 0xFF00);
            cpu.data = cpu.a;
            break;

        case 0xac: // LDY abs 4
        case 0xb4: // LDY zpx 4
            set_y(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xad: // LDA abs 4
        case 0xb5: // LDA zpx 4
            set_a(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xae: // LDX abs 4
        case 0xb6: // LDX zpy 4
            set_x(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xaf: // (LAX abs 4)
            cpu.a = cpu.data;
            set_x(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xc6: // DEC zp 5
            dec();
            break;

        case 0xcc: // CPY abs 4
            cmp_y();
            break;

        case 0xcd: // CMP abs 4
        case 0xd5: // CMP zpx 4
            cmp_a();
            break;

        case 0xe6: // INC zp 5
        case 0xe7: // (ISC zp 5)
            inc();
            break;

        case 0xec: // CPX abs 4
            cmp_x();
            break;

        case 0xed: // SBC abs 4
        case 0xf5: // SBC zpx 4
            sbc_a();
            break;

        default:
            next_opcode();
            break;
    }
}

static void step6502_cycle4()
{
    switch (cpu.opcode)
    {
        case 0x00: // BRK 7
            cpu.write = false;
            cpu.addr = 0xfffe;  // IRQ/BRK vector
            cpu.status |= FLAG_BREAK|FLAG_INTERRUPT;
            break;

        case 0x06: // ASL zp 5
        case 0x26: // ROL zp 5
        case 0x46: // LSR zp 5
        case 0x66: // ROR zp 5
        case 0x99: // STA aby 5
        case 0x9d: // STA abx 5
        case 0xc6: // DEC zp 5
        case 0xe6: // INC zp 5
            cpu.write = false;
            cpu.addr = ++cpu.pc;
            break;

        case 0x0e: // ASL abs 6
        case 0x16: // ASL zpx 6
            asl();
            break;

        case 0x11: // ORA izy 5*
        case 0x31: // AND izy 5*
        case 0x51: // EOR izy 5*
        case 0x71: // ADC izy 5*
        case 0xb1: // LDA izy 5*
        case 0xd1: // CMP izy 5*
        case 0xf1: // SBC izy 5*
            cpu.addr += (cpu.temp & 0xFF00);
            break;

        case 0x19: // ORA aby 4*
        case 0x1d: // ORA abx 4*
            or_a();
            break;

        case 0x1e: // ASL abx 7
        case 0x3e: // ROL abx 7
        case 0x5e: // LSR abx 7
        case 0x7e: // ROR abx 7
        case 0xde: // DEC abx 7
        case 0xfe: // INC abx 7
        case 0xff: // (ISC abx 7)
            cpu.temp = cpu.data;
            cpu.write = true;
            break;

        case 0x20: // JSR abs 6
            cpu.write = false;
            cpu.addr = cpu.pc;
            break;

        case 0x21: // AND izx 6
        case 0xa1: // LDA izx 6
        case 0xc1: // CMP izx 6
        case 0xc3: // (DCP izx 8)
            cpu.addr = (cpu.data << 8) | (uint8_t)cpu.temp;
            break;

        case 0x2e: // ROL abs 6
            rol();
            break;

        case 0x39: // AND aby 4*
        case 0x3d: // AND abx 4*
            and_a();
            break;

        case 0x3c: // (NOP abx 4*)
        case 0xfc: // (NOP abx 4*)
            // NOP
            cpu.addr = ++cpu.pc;
            break;

        case 0x40: // RTI 6
            cpu.addr = BASE_STACK + ++cpu.sp;
            cpu.temp = cpu.data;
            break;

        case 0x4e: // LSR abs 6
        case 0x56: // LSR zpx 6
            lsr();
            break;

        case 0x59: // EOR aby 4*
        case 0x5d: // EOR abx 4*
            xor_a();
            break;

        case 0x60: // RTS 6
        case 0x6c: // JMP ind 5
            cpu.pc = (cpu.data << 8) | (uint8_t)cpu.temp;
            cpu.addr = cpu.pc;
            break;

        case 0x6e: // ROR abs 6
        case 0x76: // ROR zpx 6
            ror();
            break;

        case 0x79: // ADC aby 4*
        case 0x7d: // ADC abx 4*
            adc_a();
            break;

        case 0x81: // STA izx 6
            cpu.write = true;
            cpu.addr = (cpu.data << 8) | (uint8_t)cpu.temp;
            cpu.data = cpu.a;
            break;

        case 0x91: // STA izy 6
            cpu.write = true;
            cpu.addr += (cpu.temp & 0xFF00);
            cpu.data = cpu.a;
            break;

        case 0xb9: // LDA aby 4*
        case 0xbd: // LDA abx 4*
            set_a(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xbc: // LDY abx 4*
            set_y(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xbe: // LDX aby 4*
            set_x(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xce: // DEC abs 6
        case 0xd6: // DEC zpx 6
            dec();
            break;

        case 0xd9: // CMP aby 4*
        case 0xdd: // CMP abx 4*
            cmp_a();
            break;

        case 0xe7: // (ISC zp 5)
            sbc_a();
            cpu.write = false;
            break;

        case 0xee: // INC abs 6
        case 0xf6: // INC zpx 6
            inc();
            break;

        case 0xf9: // SBC aby 4*
        case 0xfd: // SBC abx 4*
            sbc_a();
            break;

        default:
            next_opcode();
            break;
    }
}

static void step6502_cycle5()
{
    switch (cpu.opcode)
    {
        case 0x00: // BRK 7
            cpu.temp = cpu.data;
            ++cpu.addr;
            break;

        case 0x0e: // ASL abs 6
        case 0x16: // ASL zpx 6
        case 0x2e: // ROL abs 6
        case 0x4e: // LSR abs 6
        case 0x56: // LSR zpx 6
        case 0x6e: // ROR abs 6
        case 0x76: // ROR zpx 6
        case 0x81: // STA izx 6
        case 0x91: // STA izy 6
        case 0xce: // DEC abs 6
        case 0xd6: // DEC zpx 6
        case 0xee: // INC abs 6
        case 0xf6: // INC zpx 6
            cpu.write = false;
            cpu.addr = ++cpu.pc;
            break;

        case 0x11: // ORA izy 5*
            or_a();
            break;

        case 0x1e: // ASL abx 7
            asl();
            break;

        case 0x20: // JSR abs 6
        case 0x40: // RTI 6
            cpu.pc = (cpu.data << 8) | (uint8_t)cpu.temp;
            cpu.addr = cpu.pc;
            break;

        case 0x21: // AND izx 6
        case 0x31: // AND izy 5*
            and_a();
            break;

        case 0x3e: // ROL abx 7
            rol();
            break;

        case 0x51: // EOR izy 5*
            xor_a();
            break;

        case 0x5e: // LSR abx 7
            lsr();
            break;

        case 0x60: // RTS 6
            cpu.addr = ++cpu.pc;
            break;

        case 0x71: // ADC izy 5*
            adc_a();
            break;

        case 0x7e: // ROR abx 7
            ror();
            break;

        case 0xa1: // LDA izx 6
        case 0xb1: // LDA izy 5*
            set_a(cpu.data);
            cpu.addr = ++cpu.pc;
            break;

        case 0xc1: // CMP izx 6
        case 0xd1: // CMP izy 5*
            cmp_a();
            break;

        case 0xc3: // (DCP izx 8)
            cpu.temp = cpu.data;
            cpu.write = true;
            break;

        case 0xde: // DEC abx 7
            dec();
            break;

        case 0xf1: // SBC izy 5*
            sbc_a();
            break;

        case 0xfe: // INC abx 7
        case 0xff: // (ISC abx 7)
            inc();
            break;

        default:
            next_opcode();
            break;
    }
}

static void step6502_cycle6()
{
    switch (cpu.opcode)
    {
        case 0x00: // BRK 7
            cpu.pc = (cpu.data << 8) | (uint8_t)cpu.temp;
            cpu.addr = cpu.pc;
            break;

        case 0x1e: // ASL abx 7
        case 0x3e: // ROL abx 7
        case 0x5e: // LSR abx 7
        case 0x7e: // ROR abx 7
        case 0xde: // DEC abx 7
        case 0xfe: // INC abx 7
            cpu.write = false;
            cpu.addr = ++cpu.pc;
            break;

        case 0xc3: // (DCP izx 8)
            dec();
            cmp_register(cpu.a);
            break;

        case 0xff: // (ISC abx 7)
            sbc_a();
            cpu.write = false;
            cpu.addr = ++cpu.pc;
            break;

        default:
            next_opcode();
            break;
    }
}

static void step6502_cycle7()
{
    switch (cpu.opcode)
    {
        case 0xc3: // (DCP izx 8)
            cpu.write = false;
            cpu.addr = ++cpu.pc;
            break;

        default:
            next_opcode();
            break;
    }
}

static void step6502()
{
    switch (cpu.cycle)
    {
        case 1:
            step6502_cycle1();
            break;

        case 2:
            step6502_cycle2();
            break;

        case 3:
            step6502_cycle3();
            break;

        case 4:
            step6502_cycle4();
            break;

        case 5:
            step6502_cycle5();
            break;

        case 6:
            step6502_cycle6();
            break;

        case 7:
            step6502_cycle7();
            break;

        default:
            next_opcode();
            break;
    }

    cpu.cycle++;
}

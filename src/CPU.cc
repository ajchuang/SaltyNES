/*
Copyright (c) 2012-2017 Matthew Brennan Jones <matthew.brennan.jones@gmail.com>
Copyright (c) 2006-2011 Jamie Sanders
A NES emulator in WebAssembly. Based on vNES.
Licensed under GPLV3 or later
Hosted at: https://github.com/workhorsy/SaltyNES
*/

/*
This class emulates the Ricoh 2A03 CPU used in the NES. This is the core of the
emulator. During emulation, this is run in a loop that decodes and executes
instructions and invokes emulation of the PPU and pAPU.
*/

#include "SaltyNES.h"

CPU::CPU() : enable_shared_from_this<CPU>() {
}

shared_ptr<CPU> CPU::Init(shared_ptr<NES> nes) {
  this->nes = nes;
  this->mmap = nullptr;
  this->mem = nullptr;

  // CPU Registers:
  this->REG_ACC_NEW = 0;
  this->REG_X_NEW = 0;
  this->REG_Y_NEW = 0;
  this->REG_STATUS_NEW = 0;
  this->REG_PC_NEW = 0;
  this->REG_SP = 0;

  // Status flags:
  this->F_CARRY_NEW = 0;
  this->F_ZERO_NEW = 0;
  this->F_INTERRUPT_NEW = 0;
  this->F_DECIMAL_NEW = 0;
  this->F_BRK_NEW = 0;
  this->F_NOTUSED_NEW = 0;
  this->F_OVERFLOW_NEW = 0;
  this->F_SIGN_NEW = 0;

  // Interrupt notification:
  this->irqRequested = false;
  this->irqType = 0;

  // Misc vars:
  this->cyclesToHalt = 0;
  this->stopRunning = false;
  this->crash = false;
  return shared_from_this();
}

CPU::~CPU() {
  nes   = nullptr;
  mmap  = nullptr;
}

// Initialize:
void CPU::init() {
  CpuInfo::initOpData();

  // Get Memory Mapper:
  this->mmap = nes->getMemoryMapper();

  // Reset crash flag:
  crash = false;

  // Set flags:
  F_BRK_NEW = 1;
  F_NOTUSED_NEW = 1;
  F_INTERRUPT_NEW = 1;
  irqRequested = false;
}

void CPU::stateLoad(ByteBuffer* buf) {
  if (buf->readByte() == 1) {
    // Version 1
    // Registers:
    setStatus(buf->readInt());
    REG_ACC_NEW = buf->readInt();
    REG_PC_NEW  = buf->readInt();
    REG_SP      = buf->readInt();
    REG_X_NEW   = buf->readInt();
    REG_Y_NEW   = buf->readInt();

    // Cycles to halt:
    cyclesToHalt = buf->readInt();
  }
}

void CPU::stateSave(ByteBuffer* buf) {

  // Save info version:
  buf->putByte(static_cast<uint16_t>(1));

  // Save registers:
  buf->putInt(getStatus());
  buf->putInt(REG_ACC_NEW);
  buf->putInt(REG_PC_NEW );
  buf->putInt(REG_SP     );
  buf->putInt(REG_X_NEW  );
  buf->putInt(REG_Y_NEW  );

  // Cycles to halt:
  buf->putInt(cyclesToHalt);
}

void CPU::reset() {
  REG_ACC_NEW = 0;
  REG_X_NEW = 0;
  REG_Y_NEW = 0;

  irqRequested = false;
  irqType = 0;

  // Reset Stack pointer:
  REG_SP = 0x01FF;

  // Reset Program counter:
  REG_PC_NEW = 0x8000-1;

  // Reset Status register:
  REG_STATUS_NEW = 0x28;
  setStatus(0x28);

  // Reset crash flag:
  crash = false;

  // Set flags:
  F_CARRY_NEW = 0;
  F_DECIMAL_NEW = 0;
  F_INTERRUPT_NEW = 1;
  F_OVERFLOW_NEW = 0;
  F_SIGN_NEW = 0;
  F_ZERO_NEW = 0;

  F_NOTUSED_NEW = 1;
  F_BRK_NEW = 1;

  cyclesToHalt = 0;
}

void CPU::start() {
  stopRunning = false;

  // Registers:
  REG_ACC   = REG_ACC_NEW;
  REG_X     = REG_X_NEW;
  REG_Y     = REG_Y_NEW;
  REG_STATUS  = REG_STATUS_NEW;
  REG_PC    = REG_PC_NEW;

  // Status flags:
  F_CARRY   = F_CARRY_NEW;
  F_ZERO  = (F_ZERO_NEW == 0);
  F_INTERRUPT = F_INTERRUPT_NEW;
  F_DECIMAL   = F_DECIMAL_NEW;
  F_NOTUSED   = F_NOTUSED_NEW;
  F_BRK   = F_BRK_NEW;
  F_OVERFLOW  = F_OVERFLOW_NEW;
  F_SIGN  = F_SIGN_NEW;

  // Misc. variables
  opinf = 0;
  opaddr = 0;
  addrMode = 0;
  addr = 0;
  palCnt = 0;
  cycleCount = 0;
  cycleAdd = 0;
  temp = 0;
  add = 0;
}

void CPU::stop() {
  stopRunning = true;

  // Save registers:
  REG_ACC_NEW   = REG_ACC;
  REG_X_NEW   = REG_X;
  REG_Y_NEW   = REG_Y;
  REG_STATUS_NEW  = REG_STATUS;
  REG_PC_NEW  = REG_PC;

  // Save Status flags:
  F_CARRY_NEW   = F_CARRY;
  F_ZERO_NEW  = (F_ZERO==0?1:0);
  F_INTERRUPT_NEW = F_INTERRUPT;
  F_DECIMAL_NEW   = F_DECIMAL;
  F_BRK_NEW   = F_BRK;
  F_NOTUSED_NEW   = F_NOTUSED;
  F_OVERFLOW_NEW  = F_OVERFLOW;
  F_SIGN_NEW  = F_SIGN;
}

void CPU::emulate_frame() {
  while (!emulate()) {
  }
}

void CPU::handle_irq() {
  if (not irqRequested)
    return;

  const int tempx = status_reg();

  REG_PC_NEW = REG_PC;
  F_INTERRUPT_NEW = F_INTERRUPT;

  switch(irqType) {
    case 0: { // Normal IRQ:
      if (F_INTERRUPT != 0) {
        ////System.out.println("Interrupt was masked.");
        break;
      }
      doIrq(tempx);
      ////System.out.println("Did normal IRQ. I="+F_INTERRUPT);
      break;
    }
    case 1: { // NMI
      doNonMaskableInterrupt(tempx);
      break;
    }
    case 2: { // reset
      doResetInterrupt();
      break;
    }
    default: { // unknown IRQ
      break;
    }
  }

  REG_PC = REG_PC_NEW;
  F_INTERRUPT = F_INTERRUPT_NEW;
  F_BRK = F_BRK_NEW;
  irqRequested = false;
}

int CPU::calculate_addr(const int addr_mode) {
  switch(addr_mode) {
    case 0: { // ZERO page mode
      // Use the address given after the opcode, but without high byte.
      addr = load(opaddr + 2);
      break;
    }
    case 1: { // Relative mode.
      addr = load(opaddr + 2);
      addr += (REG_PC - 256 * (addr >= 0x80));
      break;
    }
    case 2: { // Ignore. Address is implied in instruction.
      break;
    }
    case 3: { // Absolute mode. Use the two bytes following the opcode as an address.
      addr = load16bit(opaddr + 2);
      break;
    }
    case 4: { // Accumulator mode. The address is in the accumulator register.
      addr = REG_ACC;
      break;
    }
    case 5: { // Immediate mode. The value is given after the opcode.
      addr = REG_PC;
      break;
    }
    case 6: {
      // Zero Page Indexed mode, X as index. Use the address given after the opcode, then add the
      // X register to it to get the final address.
      addr = (load(opaddr + 2) + REG_X) & 0xFF;
      break;
    }
    case 7: {
      // Zero Page Indexed mode, Y as index. Use the address given after the opcode, then add the
      // Y register to it to get the final address.
      addr = (load(opaddr + 2) + REG_Y) & 0xFF;
      break;
    }
    case 8: {
      // Absolute Indexed Mode, X as index. Same as zero page indexed, but with the high byte.
      addr = load16bit(opaddr + 2);
      cycleAdd = ((addr & 0xFF00) != ((addr + REG_X) & 0xFF00));
      addr += REG_X;
      break;
    }
    case 9: {
      // Absolute Indexed Mode, Y as index. Same as zero page indexed, but with the high byte.
      addr = load16bit(opaddr + 2);
      cycleAdd = ((addr & 0xFF00) != ((addr + REG_Y) & 0xFF00));
      addr += REG_Y;
      break;
    }
    case 10: {
      // Pre-indexed Indirect mode. Find the 16-bit address starting at the given location plus
      // the current X register. The value is the contents of that address.
      addr = load(opaddr + 2);
      cycleAdd = ((addr & 0xFF00) != ((addr + REG_X) & 0xFF00));
      addr = ((addr + REG_X) & 0xFF);
      addr = load16bit(addr);
      break;

    }
    case 11: {
      // Post-indexed Indirect mode. Find the 16-bit address contained in the given location
      // (and the one following). Add to that address the contents of the Y register. Fetch the value
      // stored at that adress.
      addr = load16bit(load(opaddr + 2));
      cycleAdd = ((addr & 0xFF00) != ((addr + REG_Y) & 0xFF00));
      addr += REG_Y;
      break;
    }
    case 12: {
      // Indirect Absolute mode. Find the 16-bit address contained at the given location.
      addr = load16bit(opaddr + 2); // Find op
      const int offset = (addr & 0xFF00) | (((addr & 0xFF) + 1) & 0xFF);
      if (addr < 0x1FFF) {
        addr = (*mem)[addr] + ((*mem)[offset] << 8);// Read from address given in op
      } else {
        addr = mmap->load(addr) + (mmap->load(offset) << 8);
      }
      break;
    }
    default: {
      break;
    }
  }

  addr &= 0xffff;
  return addr;
}

bool CPU::exec_inst() {
  switch (opinf & 0xFF) {
    case 0: { // ADC
      // Add with carry.
      temp = REG_ACC + load(addr) + F_CARRY;
      F_OVERFLOW = ((!(((REG_ACC ^ load(addr)) & 0x80)!=0) && (((REG_ACC ^ temp) & 0x80))!=0)?1:0);
      F_CARRY = (temp > 255);
      F_SIGN = (temp >> 7) & 1;
      F_ZERO = REG_ACC = (temp & 0xFF);
      cycleCount += cycleAdd;
      break;
    }
    case 1: { // AND
      // AND memory with accumulator.
      REG_ACC = REG_ACC & load(addr);
      F_SIGN = (REG_ACC >> 7) & 1;
      F_ZERO = REG_ACC;
      cycleCount += (cycleAdd * (addrMode != 11)); // PostIdxInd = 11
      break;
    }
    case 2: { // * ASL *
      // Shift left one bit
      if (addrMode == 4) { // ADDR_ACC = 4
        F_CARRY = IS_SET(REG_ACC, 7); // (REG_ACC>>7)&1;
        REG_ACC = (REG_ACC << 1) & 0xff;
        F_SIGN = IS_SET(REG_ACC, 7); // (REG_ACC>>7)&1;
        F_ZERO = REG_ACC;
      } else {
        temp = load(addr);
        F_CARRY = IS_SET(temp, 7); // (temp >> 7) & 1;
        temp = (temp << 1) & 0xff;
        F_SIGN = IS_SET(temp, 7); // (temp >> 7) & 1;
        F_ZERO = temp;
        write(addr, static_cast<uint16_t>(temp));
      }
      break;
    }
    case 3: { // * BCC *
      // Branch on carry clear
      if (F_CARRY == 0) {
        cycleCount += (((opaddr & 0xFF00) != (addr & 0xFF00)) + 1);
        REG_PC = addr;
      }
      break;
    }
    case 4: { // * BCS *
      // Branch on carry set
      if (F_CARRY == 1) {
        cycleCount += (((opaddr & 0xFF00) != (addr & 0xFF00)) + 1);
        REG_PC = addr;
      }
      break;
    }
    case 5: { // * BEQ *
      // Branch on zero
      if (F_ZERO == 0) {
        cycleCount += (((opaddr & 0xFF00) != (addr & 0xFF00)) + 1);
        REG_PC = addr;
      }
      break;
    }
    case 6: { // * BIT *
      temp = load(addr);
      F_SIGN = (temp >> 7) & 1;
      F_OVERFLOW = (temp >> 6) & 1;
      temp &= REG_ACC;
      F_ZERO = temp;
      break;
    }
    case 7: { // * BMI *
      // Branch on negative result
      if (F_SIGN == 1) {
        ++cycleCount;
        REG_PC = addr;
      }
      break;
    }
    case 8: { // * BNE *
      // Branch on not zero
      if (F_ZERO != 0) {
        cycleCount += (((opaddr & 0xFF00) != (addr & 0xFF00)) + 1);
        REG_PC = addr;
      }
      break;
    }
    case 9: { // * BPL *
      // Branch on positive result
      if (F_SIGN == 0) {
        cycleCount += (((opaddr&0xFF00)!=(addr&0xFF00)) + 1);
        REG_PC = addr;
      }
      break;
    }
    case 10: { // * BRK *
      REG_PC += 2;
      push((REG_PC >> 8) & 0xff);
      push(REG_PC & 0xff);
      F_BRK = 1;
      push(status_reg());
      F_INTERRUPT = 1;
      REG_PC = (load16bit(0xFFFE) - 1);
      break;
    }
    case 11: { // * BVC *
      // Branch on overflow clear
      if (F_OVERFLOW == 0) {
        cycleCount += (((opaddr & 0xFF00) != (addr & 0xFF00)) + 1);
        REG_PC = addr;
      }
      break;
    }
    case 12: { // * BVS *
      // Branch on overflow set
      if (F_OVERFLOW == 1) {
        cycleCount += (((opaddr & 0xFF00) != (addr & 0xFF00)) + 1);
        REG_PC = addr;
      }
      break;
    }
    case 13: {  // * CLC *
      // Clear carry flag
      F_CARRY = 0;
      break;
    }
    case 14: {  // * CLD *
      // Clear decimal flag
      F_DECIMAL = 0;
      break;
    }
    case 15: {  // * CLI *
      // Clear interrupt flag
      F_INTERRUPT = 0;
      break;
    }
    case 16: {  // * CLV *
      // Clear overflow flag
      F_OVERFLOW = 0;
      break;
    }
    case 17: {  // * CMP *
      // Compare memory and accumulator:
      temp = REG_ACC - load(addr);
      F_CARRY = (temp >= 0);
      F_SIGN = (temp >> 7) & 1;
      F_ZERO = temp & 0xFF;
      cycleCount += cycleAdd;
      break;
    }
    case 18: {  // * CPX *
      // Compare memory and index X:
      temp = REG_X - load(addr);
      F_CARRY = (temp >= 0);
      F_SIGN = (temp >> 7) & 1;
      F_ZERO = temp & 0xFF;
      break;
    }
    case 19: {  // * CPY *
      // Compare memory and index Y:
      temp = REG_Y - load(addr);
      F_CARRY = (temp >= 0);
      F_SIGN = (temp >> 7) & 1;
      F_ZERO = temp & 0xFF;
      break;
    }
    case 20: {  // * DEC *
      // Decrement memory by one:
      F_ZERO = (load(addr) - 1) & 0xFF;
      F_SIGN = (F_ZERO >> 7) & 1;
      write(addr, static_cast<uint16_t>(F_ZERO));
      break;
    }
    case 21: {  // * DEX *
      // Decrement index X by one:
      REG_X = (REG_X - 1) & 0xFF;
      F_SIGN = (REG_X >> 7) & 1;
      F_ZERO = REG_X;
      break;
    }
    case 22: { // * DEY *
      // Decrement index Y by one:
      REG_Y = (REG_Y - 1) & 0xFF;
      F_SIGN = (REG_Y >> 7) & 1;
      F_ZERO = REG_Y;
      break;
    }
    case 23: { // * EOR *
      // XOR Memory with accumulator, store in accumulator:
      REG_ACC = (load(addr) ^ REG_ACC) & 0xFF;
      F_SIGN = (REG_ACC >> 7) & 1;
      F_ZERO = REG_ACC;
      cycleCount += cycleAdd;
      break;
    }
    case 24: {  // * INC *
      // Increment memory by one:
      F_ZERO = (load(addr) + 1) & 0xFF;
      F_SIGN = (F_ZERO >> 7) & 1;
      write(addr, static_cast<uint16_t>(F_ZERO));
      break;
    }
    case 25: { // * INX *
      // Increment index X by one:
      REG_X = ((REG_X + 1) & 0xFF);
      F_SIGN = (REG_X >> 7) & 1;
      F_ZERO = REG_X;
      break;
    }
    case 26: {  // * INY *
      // Increment index Y by one:
      REG_Y = ((REG_Y + 1) & 0xff);
      F_SIGN = (REG_Y >> 7) & 1;
      F_ZERO = REG_Y;
      break;
    }
    case 27: {  // * JMP *
      // Jump to new location:
      REG_PC = addr - 1;
      break;
    }
    case 28: { // * JSR *
      // Jump to new location, saving return address.
      // Push return address on stack:
      push((REG_PC >> 8) & 0xff);
      push(REG_PC & 0xff);
      REG_PC = addr - 1;
      break;
    }
    case 29: {  // * LDA *
      // Load accumulator with memory:
      F_ZERO = REG_ACC = load(addr);
      F_SIGN = (REG_ACC >> 7) & 1;
      cycleCount += cycleAdd;
      break;
    }
    case 30: {  // * LDX *
      // Load index X with memory:
      F_ZERO = REG_X = load(addr);
      F_SIGN = (REG_X >> 7) & 1;
      cycleCount += cycleAdd;
      break;
    }
    case 31: {  // * LDY *
      // Load index Y with memory:
      F_ZERO = REG_Y = load(addr);
      F_SIGN = (REG_Y >> 7) & 1;
      cycleCount += cycleAdd;
      break;
    }
    case 32: {  // * LSR *
      // Shift right one bit:
      if (addrMode == 4) { // ADDR_ACC
        temp = (REG_ACC & 0xFF);
        F_CARRY = temp & 1;
        temp >>= 1;
        REG_ACC = temp;
      } else {
        temp = load(addr) & 0xFF;
        F_CARRY = temp & 1;
        temp >>= 1;
        write(addr, static_cast<uint16_t>(temp));
      }
      F_SIGN = 0;
      F_ZERO = temp;
      break;
    }
    case 33: {  // * NOP *
      break;
    }
    case 34: {  // * ORA *
      // OR memory with accumulator, store in accumulator.
      REG_ACC = F_ZERO = temp = (load(addr) | REG_ACC) & 255;
      F_SIGN = (temp >> 7) & 1;
      cycleCount += cycleAdd * (addrMode != 11); // PostIdxInd = 11
      break;
    }
    case 35: {  // * PHA *
      // Push accumulator on stack
      push(REG_ACC);
      break;
    }
    case 36: {  // * PHP *
      // Push processor status on stack
      F_BRK = 1;
      push(status_reg());
      break;
    }
    case 37: {  // * PLA *
      // Pop accumulator from stack
      F_ZERO = REG_ACC = pull();
      F_SIGN = IS_SET(REG_ACC, 7); //(REG_ACC >> 7) & 1;
      break;
    }
    case 38: {  // * PLP *
      // Pull processor status from stack
      temp = pull();
      status_reg(temp);
      F_NOTUSED = 1;
      break;
    }
    case 39: {  // * ROL *
      // Rotate one bit left
      if (addrMode == 4) { // ADDR_ACC = 4
        temp = REG_ACC;
        add = F_CARRY;
        F_CARRY = IS_SET(temp, 7); //(temp>>7)&1;
        temp = ((temp << 1) & 0xFF) + add;
        REG_ACC = temp;
      } else {
        temp = load(addr);
        add = F_CARRY;
        F_CARRY = IS_SET(temp, 7);
        temp = ((temp << 1) & 0xFF) + add;
        write(addr, static_cast<uint16_t>(temp));
      }

      F_SIGN = IS_SET(temp, 7); //(temp>>7)&1;
      F_ZERO = temp;
      break;
    }
    case 40: {  // * ROR *
      // Rotate one bit right
      if (addrMode == 4) { // ADDR_ACC = 4
        add = F_CARRY << 7;
        F_CARRY = IS_SET(REG_ACC, 1);// & 1;
        temp = (REG_ACC >> 1) + add;
        REG_ACC = temp;
      } else {
        temp = load(addr);
        add = F_CARRY << 7;
        F_CARRY = temp & 1;

        temp = (temp >> 1) + add;
        write(addr, static_cast<uint16_t>(temp));
      }
      F_SIGN = IS_SET(temp, 7); //(temp >> 7) & 1;
      F_ZERO = temp;
      break;
    }
    case 41:{ // * RTI *
      // Return from interrupt. Pull status and PC from stack.
      temp = pull();
      status_reg(temp);
      REG_PC = pull() + (pull() << 8);
      if (REG_PC == 0xFFFF) {
        return false;
      }
      --REG_PC;
      F_NOTUSED = 1;
      break;
    }
    case 42: {   // * RTS *
      // Return from subroutine. Pull PC from stack.
      REG_PC = pull() + (pull() << 8);
      if (REG_PC == 0xFFFF) {
        return false;
      }
      break;
    }
    case 43: {  // * SBC *
      temp = REG_ACC - load(addr) - (1 - F_CARRY);
      F_SIGN = IS_SET(temp, 7); //(temp >> 7) & 1;
      REG_ACC = F_ZERO = temp & 0xFF;
      F_OVERFLOW = ((((REG_ACC^temp)&0x80)!=0 && ((REG_ACC^load(addr))&0x80)!=0)?1:0);
      F_CARRY = (temp >= 0);
      cycleCount += cycleAdd * (addrMode != 11); // PostIdxInd = 11
      break;
    }
    case 44: { // * SEC *
      // Set carry flag
      F_CARRY = 1;
      break;
    }
    case 45: {  // * SED *
      // Set decimal mode
      F_DECIMAL = 1;
      break;
    }
    case 46: {  // * SEI *
      // Set interrupt disable status
      F_INTERRUPT = 1;
      break;
    }
    case 47: {  // * STA *
      // Store accumulator in memory
      write(addr, static_cast<uint16_t>(REG_ACC));
      break;
    }
    case 48: { // * STX *
      // Store index X in memory
      write(addr, static_cast<uint16_t>(REG_X));
      break;
    }
    case 49: {  // * STY *
      // Store index Y in memory:
      write(addr, static_cast<uint16_t>(REG_Y));
      break;
    }
    case 50: {  // * TAX *
      // Transfer accumulator to index X:
      F_ZERO = REG_X = REG_ACC;
      F_SIGN = IS_SET(REG_ACC, 7); //(REG_ACC >> 7) & 1;
      break;
    }
    case 51: {  // * TAY *
      // Transfer accumulator to index Y:
      F_ZERO = REG_Y = REG_ACC;
      F_SIGN = IS_SET(REG_ACC, 7); //(REG_ACC >> 7) & 1;
      break;
    }
    case 52: {  // * TSX *
      // Transfer stack pointer to index X:
      F_ZERO = REG_X = (REG_SP - 0x0100);
      F_SIGN = IS_SET(REG_SP, 7); //(REG_SP >> 7) & 1;
      break;
    }
    case 53: {  // * TXA *
      // Transfer index X to accumulator:
      F_ZERO = REG_ACC = REG_X;
      F_SIGN = IS_SET(REG_X, 7); //(REG_X >> 7) & 1;
      break;
    }
    case 54: {  // * TXS *
      // Transfer index X to stack pointer:
      REG_SP = (REG_X + 0x0100);
      stackWrap();
      break;
    }
    case 55: {  // * TYA *
      // Transfer index Y to accumulator:
      F_ZERO = REG_ACC = REG_Y;
      F_SIGN = IS_SET(REG_Y, 7); // >> 7) & 1;
      break;
    }
    default: {  // * ??? *
      // Illegal opcode!
      if (!crash) {
        crash = true;
        stopRunning = true;
        stringstream out;
        out << "Game crashed, invalid opcode at address $";
        out << std::hex << static_cast<int>(opaddr);
        printf("%s\n", out.str().c_str());
      }
      break;
    }
  } // end of switch

  return true;
}

// Emulates cpu instructions until screen is drawn.
bool CPU::emulate() {
  // NES Memory
  // (when memory mappers switch ROM banks
  // this will be written to, no need to
  // update reference):
  mem = &nes->cpuMem->mem;

  // References to other parts of NES:
  shared_ptr<MapperDefault> mmap = nes->memMapper;
  shared_ptr<PPU>      ppu  = nes->ppu;
  shared_ptr<PAPU>     papu = nes->papu;

  if (this->nes->_is_paused) {
    return false;
  }

  // Check interrupts:
  handle_irq();

  const uint16_t z = mmap->load(REG_PC + 1);
  opinf = CpuInfo::opdata[z];
  cycleCount = (opinf >> 24);
  cycleAdd = 0;

  // Find address mode:
  addrMode = ((opinf >> 8) & 0xFF);

  // Increment PC by number of op bytes:
  opaddr = REG_PC;
  REG_PC += ((opinf >> 16) & 0xFF);

  // calculate addr(for operands) from addressing mode
  // the addr will be smaller than 0xffff
  addr = calculate_addr(addrMode);

  // ----------------------------------------------------------------------------------------------------
  // Decode & execute instruction:
  // ----------------------------------------------------------------------------------------------------
  if (not exec_inst()) {
    return false;
  }

  if (Globals::palEmulation) {
    ++palCnt;
    if (palCnt == 5) {
      palCnt = 0;
      ++cycleCount;
    }
  }

  ppu->cycles = cycleCount * 3;
  const bool did_render = ppu->emulateCycles();

  if (Globals::enableSound) {
    papu->clockFrameCounter(cycleCount);
  }

  return did_render;
}

int CPU::load(int addr) {
  return addr < 0x2000 ? (*mem)[addr & 0x7FF] : mmap->load(addr);
}

int CPU::load16bit(int addr) {
  return
      addr < 0x1FFF ?
          (*mem)[addr & 0x7FF] | ((*mem)[(addr + 1) & 0x7FF] << 8) :
          mmap->load(addr)     | (mmap->load(addr + 1) << 8);
}

void CPU::write(int addr, uint16_t val) {
  if (addr < 0x2000) {
    (*mem)[addr & 0x7FF] = val;
  } else {
    mmap->write(addr, val);
  }
}

void CPU::requestIrq(int type) {
  if (irqRequested) {
    if (type == IRQ_NORMAL) {
      return;
    }
  }
  irqRequested = true;
  irqType = type;
}

void CPU::push(int value) {
  mmap->write(REG_SP, static_cast<uint16_t>(value));
  --REG_SP;
  REG_SP = 0x0100 | (REG_SP & 0xFF);
}

void CPU::stackWrap() {
  REG_SP = 0x0100 | (REG_SP & 0xFF);
}

uint16_t CPU::pull() {
  ++REG_SP;
  REG_SP = 0x0100 | (REG_SP & 0xFF);
  return mmap->load(REG_SP);
}

bool CPU::pageCrossed(int addr1, int addr2) {
  return ((addr1&0xFF00)!=(addr2&0xFF00));
}

void CPU::haltCycles(int cycles) {
  cyclesToHalt += cycles;
}

void CPU::doNonMaskableInterrupt(int status) {
  int temp = mmap->load(0x2000); // Read PPU status.
  if ((temp & 128) != 0) { // Check whether VBlank Interrupts are enabled

    ++REG_PC_NEW;
    push((REG_PC_NEW >> 8) & 0xFF);
    push(REG_PC_NEW & 0xFF);
    //F_INTERRUPT_NEW = 1;
    push(status);
    REG_PC_NEW = mmap->load(0xFFFA) | (mmap->load(0xFFFB) << 8);
    --REG_PC_NEW;
  }
}

void CPU::doResetInterrupt() {
  REG_PC_NEW = mmap->load(0xFFFC) | (mmap->load(0xFFFD) << 8);
  --REG_PC_NEW;
}

void CPU::doIrq(int status) {
  ++REG_PC_NEW;
  push((REG_PC_NEW >> 8) & 0xFF);
  push(REG_PC_NEW & 0xFF);
  push(status);
  F_INTERRUPT_NEW = 1;
  F_BRK_NEW = 0;
  REG_PC_NEW = mmap->load(0xFFFE) | (mmap->load(0xFFFF) << 8);
  --REG_PC_NEW;
}

int CPU::getStatus() {
  return
      ((F_CARRY_NEW & 0x01)   | 
       (F_ZERO_NEW      << 1) | 
       (F_INTERRUPT_NEW << 2) |
       (F_DECIMAL_NEW   << 3) |
       (F_BRK_NEW       << 4) |
       (F_NOTUSED_NEW   << 5) |
       (F_OVERFLOW_NEW  << 6) |
       (F_SIGN_NEW      << 7));
}

void CPU::setStatus(int st) {
  F_CARRY_NEW     = (st   )&1;
  F_ZERO_NEW      = (st>>1)&1;
  F_INTERRUPT_NEW = (st>>2)&1;
  F_DECIMAL_NEW   = (st>>3)&1;
  F_BRK_NEW       = (st>>4)&1;
  F_NOTUSED_NEW   = (st>>5)&1;
  F_OVERFLOW_NEW  = (st>>6)&1;
  F_SIGN_NEW      = (st>>7)&1;
}

void CPU::setCrashed(bool value) {
  this->crash = value;
}

void CPU::setMapper(shared_ptr<MapperDefault> mapper) {
  mmap = mapper;
}

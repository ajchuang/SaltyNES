#include "SaltyNES.h"

// Documentation:
// In addition to normal 8 KiB of CHR-RAM and 8 KiB of battery-backed 
// WRAM at $6000-$7FFF, it has an additional non-backed 4 KiB of WRAM
// at $5000-$5FFF. 640 KiB of PRG-ROM data is spread over two chips,
// 512 KiB providing the data for 8 KiB PRG-ROM banks $00-$3F, and 128
// KiB providing the data for 8 KiB PRG-ROM banks $40-$4F, for a total 
// of 640 KiB of PRG-ROM. Special consideration is needed for the two 
// fixed MMC3 banks, which are located in the second chip, meaning that
// they are bank numbers $4E and $4F, respectively
Mapper198::Mapper198() : MapperDefault() {
  std::fill(wram.begin(), wram.end(), 0);
  std::fill(prg_ram0.begin(), prg_ram0.end(), 0);
  std::fill(prg_ram1.begin(), prg_ram1.end(), 0);
}

shared_ptr<MapperDefault> Mapper198::Init(shared_ptr<NES> nes) {
  reset();
  this->base_init(nes);
  return shared_from_this();
}

uint16_t Mapper198::load(int address) {
  if (0x5000 <= address && address < 0x6000) {
    mlog("load[0x%x]", address);
    return (wram[address] & 0xff) | (wram[address + 1] & 0xff);
  }
  return this->base_load(address);
}

void Mapper198::write(int address, short value) {
  mlog("write[0x%x] = %d", address, (int)value);
  if (0x5000 <= address && address < 0x6000) {
    wram[address]     = value & 0xff;
    wram[address + 1] = (value >> 8) & 0xff;
  }
  return this->base_write(address, value);
}

void Mapper198::loadROM(ROM* rom) {
  if (! rom->isValid()) {
    printf("VRC2: Invalid ROM! Unable to load.");
    return;
  }

  mlog("loading ROM");
}

int Mapper198::syncH(int scanline) {
  mlog("scanline: %d", scanline);
  return 0;
}

void Mapper198::reset() {
  mlog("reset");
}

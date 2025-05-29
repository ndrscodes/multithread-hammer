#include "DRAMAddr.hpp"
#include "asmjit/x86/x86assembler.h"
#include <asmjit/asmjit.h>
#include <vector>

typedef size_t (*HammerFunc)(void);

class Jitter {
private:
  asmjit::JitRuntime rt;
  HammerFunc fn = nullptr;
  size_t refresh_threshold;
  void jit_ref_sync(asmjit::x86::Assembler &assembler, DRAMAddr sync_bank);
public:
  Jitter(size_t refresh_threshold);
  HammerFunc jit(std::vector<volatile char *> &addresses, size_t acts, bool sync_each_iteration);
  void clean();
  ~Jitter();
};

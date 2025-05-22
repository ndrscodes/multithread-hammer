#include "DRAMAddr.hpp"
#include <asmjit/asmjit.h>
#include <vector>

typedef void (*HammerFunc)(void);

class Jitter {
private:
  asmjit::JitRuntime rt;
  HammerFunc fn = nullptr;
public:
  HammerFunc jit(std::vector<DRAMAddr> &addresses, size_t repetitions);
  void clean();
  ~Jitter();
};

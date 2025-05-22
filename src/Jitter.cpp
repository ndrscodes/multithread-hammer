#include "Jitter.hpp"
#include "DRAMAddr.hpp"
#include "asmjit/core/codeholder.h"
#include "asmjit/core/globals.h"
#include "asmjit/x86/x86assembler.h"
#include "asmjit/x86/x86operand.h"
#include <cstdint>
#include <set>
#include <vector>

const size_t SERIALIZE_EACH_N = 2;

HammerFunc Jitter::jit(std::vector<DRAMAddr> &addresses, size_t repetitions) {
  asmjit::CodeHolder h;
  asmjit::x86::Assembler assembler(&h);
  std::vector<volatile char *> ptrs(addresses.size());
  for(int i = 0; i < addresses.size(); i++) {
    ptrs[i] = (volatile char *)addresses[i].to_virt();
  }

  //we should mostly use rax and rcx as they are caller-saved.
  std::set<volatile char *> used_ptrs;
  for(int i = 0; i < repetitions; i++) {
    for(int j = 0; j < ptrs.size(); j++) {
      //move the pointer to rax
      assembler.mov(asmjit::x86::rax, (uint64_t)ptrs[i]);
      //we only need to flush addresses which have already been accessed
      bool flushed = false;
      if(used_ptrs.contains(ptrs[j])) {
        //flush the corresponding line from the cache
        assembler.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
      }
      //serialize instructions on every nth access;
      if(j % SERIALIZE_EACH_N == 1) {
        assembler.mfence();
      }
      //dereference the pointer, causing a memory access
      assembler.mov(asmjit::x86::rcx, asmjit::x86::ptr(asmjit::x86::rax));
    }
  }

  asmjit::Error error = rt.add(&fn, &h);
  if(error) {
    printf("unable to jit pattern due to error code %d (%s).\n", error, asmjit::DebugUtils::errorAsString(error));
  }

  return fn;
}

void Jitter::clean() {
  rt.reset();
}

Jitter::~Jitter() {
  rt.reset();
  fn = nullptr;
}

#include "Jitter.hpp"
#include "DRAMAddr.hpp"
#include "DRAMConfig.hpp"
#include "asmjit/core/codeholder.h"
#include "asmjit/core/globals.h"
#include "asmjit/x86/x86assembler.h"
#include "asmjit/x86/x86operand.h"
#include <cstdint>
#include <set>
#include <vector>

const size_t SERIALIZE_EACH_N = 2;

Jitter::Jitter(size_t refresh_threshold) {
  this->refresh_threshold = refresh_threshold;
}

void Jitter::jit_ref_sync(asmjit::x86::Assembler &assembler, DRAMAddr sync_bank) {
  auto start = assembler.newLabel();
  uint64_t sync_addr = (uint64_t)sync_bank.to_virt();
  assembler.bind(start);
 
  //first flush it from the cache
  assembler.mov(asmjit::x86::rax, sync_addr);
  assembler.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
  
  assembler.lfence();
  //stores cycles in EAX
  assembler.rdtscp();
  assembler.lfence();
 
  //store cycles in ECX for comparing it later
  assembler.mov(asmjit::x86::ecx, asmjit::x86::eax);
  
  //now we hammer
  assembler.mov(asmjit::x86::rax, sync_addr);
  assembler.mov(asmjit::x86::rdx, asmjit::x86::ptr(asmjit::x86::rax));

  //take another measurement
  assembler.lfence();
  assembler.rdtscp();
  assembler.lfence();

  //subtract the previous timestamp from the current
  assembler.sub(asmjit::x86::eax, asmjit::x86::ecx);

  //we jump to start if we were below refresh_threshold cycles
  assembler.cmp(asmjit::x86::edx, refresh_threshold);
  assembler.jb(start);
}

HammerFunc Jitter::jit(std::vector<DRAMAddr> &addresses) {
  asmjit::CodeHolder h;
  h.init(rt.environment(), rt.cpuFeatures());
  asmjit::x86::Assembler assembler(&h);
  std::vector<volatile char *> ptrs(addresses.size());
  for(int i = 0; i < addresses.size(); i++) {
    if(addresses[i].actual_column() == DRAMConfig::get().columns()) {
      ptrs[i] = nullptr;
    }
    ptrs[i] = (volatile char *)addresses[i].to_virt();
  }

  std::set<volatile char *> used_ptrs;

  used_ptrs.insert(ptrs.front()); 
  jit_ref_sync(assembler, addresses.back());

  for(int j = 0; j < ptrs.size(); j++) {
    if(ptrs[j] == nullptr) {
      assembler.mfence();
      assembler.nop();
    }
    //move the pointer to rax
    assembler.mov(asmjit::x86::rax, (uint64_t)ptrs[j]);
    //we only need to flush addresses which have already been accessed
    bool flushed = false;
    if(used_ptrs.contains(ptrs[j])) {
      //flush the corresponding line from the cache
      assembler.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
    } else {
      used_ptrs.insert(ptrs[j]);
    }
    //serialize instructions on every nth access;
    if(j % SERIALIZE_EACH_N == 1) {
      assembler.mfence();
    }
    //dereference the pointer, causing a memory access
    assembler.mov(asmjit::x86::rcx, asmjit::x86::ptr(asmjit::x86::rax));
  }

  jit_ref_sync(assembler, addresses.back());

  assembler.ret();

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

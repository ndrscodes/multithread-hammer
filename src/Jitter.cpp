#include "Jitter.hpp"
#include "DRAMAddr.hpp"
#include "asmjit/core/codeholder.h"
#include "asmjit/core/globals.h"
#include "asmjit/x86/x86assembler.h"
#include "asmjit/x86/x86operand.h"
#include <cstdint>
#include <set>
#include <vector>

const size_t SERIALIZE_EACH_N = 16;

Jitter::Jitter(size_t refresh_threshold) {
  this->refresh_threshold = refresh_threshold;
}

void Jitter::jit_ref_sync(asmjit::x86::Assembler &assembler, DRAMAddr sync_bank) {
  assembler.push(asmjit::x86::rdi);
  assembler.mov(asmjit::x86::rdi, 0);

  auto start = assembler.newLabel();
  uint64_t sync_addr = (uint64_t)sync_bank.to_virt();
  assembler.bind(start);
 
  //first flush it from the cache
  assembler.mov(asmjit::x86::rax, sync_addr);
  assembler.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
  
  assembler.mfence();
  assembler.lfence();
  //stores cycles in EAX
  assembler.rdtscp();
  assembler.lfence();
 
  //store cycles in r11 for comparing it later
  assembler.mov(asmjit::x86::r11, asmjit::x86::eax);
  
  //now we hammer
  assembler.mov(asmjit::x86::rax, sync_addr);
  assembler.mov(asmjit::x86::rdx, asmjit::x86::ptr(asmjit::x86::rax));

  //take another measurement
  assembler.mfence();
  assembler.rdtscp();
  assembler.lfence();

  //subtract the previous timestamp from the current
  assembler.sub(asmjit::x86::rax, asmjit::x86::r11);
  assembler.inc(asmjit::x86::rdi);

  assembler.cmp(asmjit::x86::rdi, 4);
  assembler.jb(start);
  //we jump to start if we were below refresh_threshold cycles
  assembler.cmp(asmjit::x86::rax, refresh_threshold);
  assembler.jb(start);

  assembler.pop(asmjit::x86::rdi);
}

HammerFunc Jitter::jit(std::vector<volatile char *> &addresses, std::vector<volatile char *> &non_accessed_rows, size_t acts, bool sync_each_iteration) {
  asmjit::CodeHolder h;
  h.init(rt.environment(), rt.cpuFeatures());
  asmjit::x86::Assembler assembler(&h);

  std::set<volatile char *> used_ptrs;

  used_ptrs.insert(addresses.front()); 

  assembler.mov(asmjit::x86::rsi, 0);
  jit_ref_sync(assembler, DRAMAddr((void *)non_accessed_rows.back()));

  auto loop_start = assembler.newLabel();
  assembler.bind(loop_start);
  if(sync_each_iteration) {
    jit_ref_sync(assembler, DRAMAddr((void *)non_accessed_rows.back()));
  }

  //get the current timestamp and push it to the stack
  assembler.mfence();
  assembler.rdtscp();
  assembler.lfence();
  assembler.shl(asmjit::x86::rdx, 32);
  assembler.or_(asmjit::x86::rdx, asmjit::x86::rax);
  assembler.mov(asmjit::x86::r10, asmjit::x86::rdx);

  bool should_fence = true;
  for(int j = 0; j < addresses.size(); j++) {
    if(addresses[j] == nullptr) {
      if(should_fence) {
        assembler.mfence();
        should_fence = false;
      } else {
        assembler.nop();
      }
      assembler.nop();
      continue;
    }
    should_fence = true;
    //move the pointer to rax
    assembler.mov(asmjit::x86::rax, (uint64_t)addresses[j]);
    //we only need to flush addresses which have already been accessed
    bool flushed = false;
    if(used_ptrs.contains(addresses[j])) {
      //flush the corresponding line from the cache
      assembler.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
    } else {
      used_ptrs.insert(addresses[j]);
    }
    //serialize instructions on every nth access;
    if(j % SERIALIZE_EACH_N == 1) {
      assembler.mfence();
    }
    //dereference the pointer, causing a memory access
    assembler.mov(asmjit::x86::rcx, asmjit::x86::ptr(asmjit::x86::rax));
    assembler.inc(asmjit::x86::rsi);
  }

  assembler.cmp(asmjit::x86::rsi, acts);
  assembler.jb(loop_start);

  //get another timestamp
  assembler.mfence();
  assembler.rdtscp();
  assembler.lfence();
  assembler.shl(asmjit::x86::rdx, 32);
  assembler.or_(asmjit::x86::rdx, asmjit::x86::rax);
  assembler.push(asmjit::x86::rdx);

  jit_ref_sync(assembler, DRAMAddr((void *)non_accessed_rows.back()));

  //pop newest timestamp to rax, oldest to rdx
  assembler.pop(asmjit::x86::rax);
  //subtract oldest from newest in rax, which is our return value
  assembler.sub(asmjit::x86::rax, asmjit::x86::r10);

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

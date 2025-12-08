#ifndef __EXCEPTION_HANDLER_HPP__
#define __EXCEPTION_HANDLER_HPP__

#include <3ds.h>
#include <string>

void DefaultExceptionHandler(ERRF_ExceptionInfo* info, CpuRegisters* regs);
void* allocateHandlerStack(u64 size);

#endif
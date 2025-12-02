#ifndef __EXCEPTION_HANDLER_HPP__
#define __EXCEPTION_HANDLER_HPP__

#include <3ds.h>
#include <string>

void ExceptionHandlerLogInfo(ERRF_ExceptionInfo* info, CpuRegisters* regs);
std::string ExceptionHandlerFormatInfo(ERRF_ExceptionInfo* info, CpuRegisters* regs);

void DefaultExceptionHandler(ERRF_ExceptionInfo* info, CpuRegisters* regs);
void* allocateHandlerStack(u64 size);

#endif
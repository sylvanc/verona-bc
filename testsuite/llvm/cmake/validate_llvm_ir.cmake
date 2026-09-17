file(GLOB llvm_ir_files "${OUTPUT_DIR}/*.ll")
list(LENGTH llvm_ir_files llvm_ir_count)
if(NOT llvm_ir_count EQUAL 1)
  message(FATAL_ERROR
    "Expected exactly one LLVM IR artifact, found ${llvm_ir_count}")
endif()

list(GET llvm_ir_files 0 llvm_ir)
file(READ "${llvm_ir}" emitted_llvm_ir)

if(NOT emitted_llvm_ir MATCHES "target datalayout = \"[^\"]+\"")
  message(FATAL_ERROR "LLVM IR does not declare a target data layout")
endif()

if(NOT emitted_llvm_ir MATCHES "target triple = \"[^\"]+\"")
  message(FATAL_ERROR "LLVM IR does not declare a target triple")
endif()

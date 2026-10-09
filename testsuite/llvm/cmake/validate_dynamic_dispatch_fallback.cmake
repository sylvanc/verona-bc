include("${CMAKE_CURRENT_LIST_DIR}/validate_llvm_ir.cmake")

if(NOT emitted_llvm_ir MATCHES "call [^\r\n]*@vrt_object_lookup\\(")
  message(FATAL_ERROR "Large dynamic dispatch does not use runtime lookup")
endif()

if(emitted_llvm_ir MATCHES "call [^\r\n]*@vrt_object_class_id\\(")
  message(FATAL_ERROR "Large dynamic dispatch unexpectedly reads class IDs")
endif()

if(emitted_llvm_ir MATCHES "switch i[0-9]+")
  message(FATAL_ERROR "Large dynamic dispatch unexpectedly emits a switch")
endif()
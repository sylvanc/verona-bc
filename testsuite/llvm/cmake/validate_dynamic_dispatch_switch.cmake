include("${CMAKE_CURRENT_LIST_DIR}/validate_llvm_ir.cmake")

if(NOT emitted_llvm_ir MATCHES
   "call [^\r\n]*@vrt_object_class_id\\(")
  message(FATAL_ERROR "Dynamic dispatch does not read the object class ID")
endif()

if(NOT emitted_llvm_ir MATCHES "switch i[0-9]+")
  message(FATAL_ERROR "Dynamic dispatch does not contain an LLVM switch")
endif()

if(NOT emitted_llvm_ir MATCHES
   "@verona_fn_Increment_invoke\\.descriptor")
  message(FATAL_ERROR "Increment dispatch descriptor is unavailable")
endif()

if(NOT emitted_llvm_ir MATCHES
   "@verona_fn_Double_invoke\\.descriptor")
  message(FATAL_ERROR "Double dispatch descriptor is unavailable")
endif()

if(NOT emitted_llvm_ir MATCHES " = phi [^\r\n]+")
  message(FATAL_ERROR "Dynamic dispatch descriptors are not merged by a phi")
endif()

if(NOT emitted_llvm_ir MATCHES "call [^\r\n]*@vrt_object_lookup\\(")
  message(FATAL_ERROR "Dynamic dispatch switch has no runtime fallback")
endif()
#include "thread.h"

#include "array.h"
#include "cown.h"
#include "function_signature.h"
#include "object.h"
#include "program.h"

#include <iostream>

#define INLINE SNMALLOC_FAST_PATH_LAMBDA

namespace vbci
{
  template<typename>
  struct always_false : std::false_type
  {};

  // Forward declarations
  template<typename F, typename... Args>
  static void process(Thread& self, F fun, Args... args);

  struct ArgReg
  {
    Register& reg;

    void operator=(Register&& v)
    {
      reg = std::move(v);
    }

    void operator=(const Register& v)
    {
      reg = v.copy_reg();
    }
  };

  struct GlobalRead
  {
    const Value& global;
  };

  struct Global
  {
    Value& global;

    void operator=(Value&& v)
    {
      global = std::move(v);
    }
  };

  struct ConstantBase
  {};
  template<typename T_>
  struct Constant : ConstantBase
  {
    using T = T_;
    T value;

    Constant(T v) : value(v) {}

    // Implicit conversion to T
    operator T() const
    {
      return value;
    }
  };

  struct Operands
  {
    template<typename F, typename... Args>
    SNMALLOC_FAST_PATH static void
    call_function_and_continue(F fun, Args... args)
    {
      fun(std::forward<Args>(args)...);
    }

    template<typename F, typename... Args>
    SNMALLOC_FAST_PATH static void
    register_ref(Thread& self, F fun, Args... args)
    {
      size_t reg_index = self.leb();
      Register& reg = self.frame->local(reg_index);
      self.trace_instruction(" Local=", reg_index, " (", reg, ")");
      process<F, Args..., Register&>(
        self, fun, std::forward<Args>(args)..., reg);
      LOG(Trace) << "Updated " << reg_index << " to " << reg;
    }

    template<typename F, typename... Args>
    SNMALLOC_FAST_PATH static void
    register_borrow(Thread& self, F fun, Args... args)
    {
      size_t reg_index = self.leb();
      const Register& reg = self.frame->local(reg_index);
      self.trace_instruction(" Local=", reg_index, " (", reg, ")");
      process<F, Args..., const Register&>(
        self, fun, std::forward<Args>(args)..., reg);
    }

    template<typename F, typename... Args>
    SNMALLOC_FAST_PATH static void
    register_move(Thread& self, F fun, Args... args)
    {
      size_t reg_index = self.leb();
      Register& reg = self.frame->local(reg_index);
      self.trace_instruction(" Local=", reg_index, " (", reg, ")");
      process<F, Args..., Register>(
        self, fun, std::forward<Args>(args)..., std::move(reg));
    }

    template<typename F, typename... Args>
    SNMALLOC_FAST_PATH static void
    load_global_read_param(Thread& self, F fun, Args... args)
    {
      size_t global_index = self.leb();
      self.trace_instruction(" GlobalRead=", global_index);
      GlobalRead g{self.program->global(global_index)};
      process<F, Args..., GlobalRead>(
        self, fun, std::forward<Args>(args)..., g);
    }

    template<typename F, typename... Args>
    SNMALLOC_FAST_PATH static void
    load_global_param(Thread& self, F fun, Args... args)
    {
      size_t global_index = self.leb();
      self.trace_instruction(" Global=", global_index);
      Global g{self.program->global(global_index)};
      process<F, Args..., Global>(self, fun, std::forward<Args>(args)..., g);
    }

    template<typename T, typename F, typename... Args>
    SNMALLOC_FAST_PATH static void
    load_constant_param(Thread& self, F fun, Args... args)
    {
      size_t constant_value = self.leb();
      self.trace_instruction(" Constant=", constant_value);
      Constant<T> c{static_cast<T>(constant_value)};
      process<F, Args..., Constant<T>>(
        self, fun, std::forward<Args>(args)..., c);
    }

    template<typename F, typename... Args>
    SNMALLOC_FAST_PATH static void
    load_class_param(Thread& self, F fun, Args... args)
    {
      size_t class_id = self.leb();
      auto& cls = self.program->cls(class_id);
      self.trace_instruction(" Class=", class_id);
      process<F, Args..., Class&>(self, fun, std::forward<Args>(args)..., cls);
    }

    template<typename F, typename... Args>
    SNMALLOC_FAST_PATH static void
    load_function_ptr_param(Thread& self, F fun, Args... args)
    {
      size_t func_id = self.leb();
      Function* func = self.program->function(func_id);
      self.trace_instruction(
        " FunctionPtr=", func_id, "(", self.program->di_function(func), ")");
      process<F, Args..., Function*>(
        self, fun, std::forward<Args>(args)..., func);
    }

    // Tail-recursive process passing the current parameter index
    template<typename F, typename... Args>
    SNMALLOC_FAST_PATH static void process(Thread& self, F fun, Args... args)
    {
      constexpr size_t current_param = sizeof...(Args);

      if constexpr (param_count_v<F> == current_param)
      {
#ifndef NDEBUG
        self.instruction_log.dump_and_reset(logging::detail::LogLevel::Trace);
#endif
        call_function_and_continue<F, Args...>(
          fun, std::forward<Args>(args)...);
      }
      else
      {
        using T = param_type_t<F, current_param>;
        if constexpr (std::is_same_v<T, const Register&>)
        {
          register_borrow<F, Args...>(self, fun, std::forward<Args>(args)...);
        }
        else if constexpr (std::is_same_v<T, Register&>)
        {
          register_ref<F, Args...>(self, fun, std::forward<Args>(args)...);
        }
        else if constexpr (std::is_same_v<T, Register>)
        {
          register_move<F, Args...>(self, fun, std::forward<Args>(args)...);
        }
        else if constexpr (std::is_same_v<T, GlobalRead>)
        {
          load_global_read_param<F, Args...>(
            self, fun, std::forward<Args>(args)...);
        }
        else if constexpr (std::is_same_v<T, Global>)
        {
          load_global_param<F, Args...>(self, fun, std::forward<Args>(args)...);
        }
        else if constexpr (std::is_base_of_v<
                             ConstantBase,
                             std::remove_reference_t<T>>)
        {
          load_constant_param<
            typename param_type_t<F, current_param>::T,
            F,
            Args...>(self, fun, std::forward<Args>(args)...);
        }
        else if constexpr (std::is_same_v<T, Thread&>)
        {
          process<F, Args..., Thread&>(
            self, fun, std::forward<Args>(args)..., self);
        }
        else if constexpr (std::is_same_v<T, Class&>)
        {
          load_class_param<F, Args...>(self, fun, std::forward<Args>(args)...);
        }
        else if constexpr (std::is_same_v<T, Frame&>)
        {
          process<F, Args..., Frame&>(
            self, fun, std::forward<Args>(args)..., *self.frame);
        }
        else if constexpr (std::is_same_v<T, Program&>)
        {
          process<F, Args..., Program&>(
            self, fun, std::forward<Args>(args)..., *self.program);
        }
        else if constexpr (std::is_same_v<T, Stack&>)
        {
          process<F, Args..., Stack&>(
            self, fun, std::forward<Args>(args)..., self.stack);
        }
        else if constexpr (std::is_same_v<T, ArgReg>)
        {
          self.trace_instruction(" ArgReg=", self.args);
          ArgReg arg{self.frame->arg(self.args++)};
          process<F, Args..., ArgReg>(
            self, fun, std::forward<Args>(args)..., arg);
        }
        else if constexpr (std::is_same_v<T, Function*>)
        {
          load_function_ptr_param<F, Args...>(
            self, fun, std::forward<Args>(args)...);
        }
        else
        {
          static_assert(
            always_false<T>::value,
            "Unsupported parameter type in Operands::process");
          abort();
        }
      }
    }
  };

  Register Thread::run_async(uint32_t type_id, Function* func)
  {
    auto result = Cown::create(type_id);

    if (!Program::get().subtype(func->return_type, result->content_type_id()))
      throw Value(Error::BadType);

    auto b =
      verona::rt::BehaviourCore::make(1, run_behavior, sizeof(Value) * 2);
    new (&b->get_slots()[0]) verona::rt::Slot(result);
    new (&b->get_body<Value>()[0]) Value(func);
    new (&b->get_body<Value>()[1]) Value();
    verona::rt::BehaviourCore::schedule_many(&b, 1);

    // Safe to convert to use Register this only contains a cown pointer, so
    // there is no requirement for a stack_rc.
    return Register(Value(result, false));
  }

  std::pair<Function*, PC> Thread::debug_info()
  {
    auto& t = get();

    if (t.frame)
      return {t.frame->func, t.current_pc};
    else
      return {t.behavior, t.current_pc};
  }

  Thread::Thread() : program(&Program::get()), frame(nullptr), args(0)
  {
    frames.reserve(16);
    locals.resize(1024);
  }

  Thread& Thread::get()
  {
    thread_local Thread thread;
    return thread;
  }

  void Thread::run_behavior(verona::rt::Work* work)
  {
    get().thread_run_behavior(work);
  }

  void Thread::thread_run_behavior(verona::rt::Work* work)
  {
    assert(!frame);
    assert(!args);
    auto b = verona::rt::BehaviourCore::from_work(work);
    auto values = b->get_body<Value>();
    behavior = values[0].function();
    auto& closure = values[1];

    if (!closure.is_invalid())
    {
      if (closure.is_header())
      {
        auto r = closure.get_header()->region();

        // Clear the parent region, as it's no longer acquired by the behaviour.
        if (r)
          r->clear_parent();
      }

      locals.at(args++) = std::move(closure);
    }

    // Populate cown arguments.
    auto slots = b->get_slots();
    auto num_cowns = b->get_count();
    auto result = static_cast<Cown*>(slots[0].cown());

    for (size_t i = 1; i < num_cowns; i++)
    {
      auto cown = static_cast<Cown*>(slots[i].cown());
      // TODO: The locals needs an RC here, we should look how we can remove
      // that.  The verona runtime "behaviour" keeps this alive, but the
      // Register would only receive a borrow, rather than a move. The current
      // implementation considers all registers to have an RC associated. if we
      // don't perform this inc, then the register invalidation code would break
      // this.
      // Perhaps needs a dynamic borrowed register reference?
      cown->inc();
      locals.at(args++) = Value(cown, slots[i].is_read_only());
    }

    auto ret = thread_run(behavior);
    try
    {
      // Store the function return value in the result cown.
      result->store<true>(std::move(ret));
    }
    catch (Value& error_value)
    {
      // If we fail to store into the result cown, we need to store the error
      // instead.  It is always valid to store the error.
      result->store<true>(Register(std::move(error_value)));
    }

    verona::rt::BehaviourCore::finished(work);
  }

  Register Thread::thread_run(Function* func)
  {
    auto depth = frames.size();
    pushframe(func, 0, CallType::Catch);

    while (depth != frames.size())
      step();

    return std::move(locals.at(0));
  }

  std::ostream& operator<<(std::ostream& os, Op op)
  {
    switch (op)
    {
      case Op::Global:
        return os << "Global";
      case Op::Const:
        return os << "Const";
      case Op::String:
        return os << "String";
      case Op::Convert:
        return os << "Convert";
      case Op::New:
        return os << "New";
      case Op::Stack:
        return os << "Stack";
      case Op::Heap:
        return os << "Heap";
      case Op::Region:
        return os << "Region";
      case Op::NewArray:
        return os << "NewArray";
      case Op::NewArrayConst:
        return os << "NewArrayConst";
      case Op::StackArray:
        return os << "StackArray";
      case Op::StackArrayConst:
        return os << "StackArrayConst";
      case Op::HeapArray:
        return os << "HeapArray";
      case Op::HeapArrayConst:
        return os << "HeapArrayConst";
      case Op::RegionArray:
        return os << "RegionArray";
      case Op::RegionArrayConst:
        return os << "RegionArrayConst";
      case Op::Copy:
        return os << "Copy";
      case Op::Move:
        return os << "Move";
      case Op::Drop:
        return os << "Drop";
      case Op::RegisterRef:
        return os << "RegisterRef";
      case Op::FieldRefMove:
        return os << "FieldRefMove";
      case Op::FieldRefCopy:
        return os << "FieldRefCopy";
      case Op::ArrayRefMove:
        return os << "ArrayRefMove";
      case Op::ArrayRefCopy:
        return os << "ArrayRefCopy";
      case Op::ArrayRefMoveConst:
        return os << "ArrayRefMoveConst";
      case Op::ArrayRefCopyConst:
        return os << "ArrayRefCopyConst";
      case Op::Load:
        return os << "Load";
      case Op::StoreMove:
        return os << "StoreMove";
      case Op::StoreCopy:
        return os << "StoreCopy";
      case Op::LookupStatic:
        return os << "LookupStatic";
      case Op::LookupDynamic:
        return os << "LookupDynamic";
      case Op::LookupFFI:
        return os << "LookupFFI";
      case Op::ArgMove:
        return os << "ArgMove";
      case Op::ArgCopy:
        return os << "ArgCopy";
      case Op::CallStatic:
        return os << "CallStatic";
      case Op::CallDynamic:
        return os << "CallDynamic";
      case Op::SubcallStatic:
        return os << "SubcallStatic";
      case Op::SubcallDynamic:
        return os << "SubcallDynamic";
      case Op::TryStatic:
        return os << "TryStatic";
      case Op::TryDynamic:
        return os << "TryDynamic";
      case Op::FFI:
        return os << "FFI";
      case Op::WhenStatic:
        return os << "WhenStatic";
      case Op::WhenDynamic:
        return os << "WhenDynamic";
      case Op::Typetest:
        return os << "Typetest";
      case Op::TailcallStatic:
        return os << "TailcallStatic";
      case Op::TailcallDynamic:
        return os << "TailcallDynamic";
      case Op::Return:
        return os << "Return";
      case Op::Raise:
        return os << "Raise";
      case Op::Throw:
        return os << "Throw";
      case Op::Cond:
        return os << "Cond";
      case Op::Jump:
        return os << "Jump";
      case Op::Add:
        return os << "Add";
      case Op::Sub:
        return os << "Sub";
      case Op::Mul:
        return os << "Mul";
      case Op::Div:
        return os << "Div";
      case Op::Mod:
        return os << "Mod";
      case Op::Pow:
        return os << "Pow";
      case Op::And:
        return os << "And";
      case Op::Or:
        return os << "Or";
      case Op::Xor:
        return os << "Xor";
      case Op::Shl:
        return os << "Shl";
      case Op::Shr:
        return os << "Shr";
      case Op::Eq:
        return os << "Eq";
      case Op::Ne:
        return os << "Ne";
      case Op::Lt:
        return os << "Lt";
      case Op::Le:
        return os << "Le";
      case Op::Gt:
        return os << "Gt";
      case Op::Ge:
        return os << "Ge";
      case Op::Min:
        return os << "Min";
      case Op::Max:
        return os << "Max";
      case Op::LogBase:
        return os << "LogBase";
      case Op::Atan2:
        return os << "Atan2";
      case Op::Neg:
        return os << "Neg";
      case Op::Not:
        return os << "Not";
      case Op::Abs:
        return os << "Abs";
      case Op::Ceil:
        return os << "Ceil";
      case Op::Floor:
        return os << "Floor";
      case Op::Exp:
        return os << "Exp";
      case Op::Log:
        return os << "Log";
      case Op::Sqrt:
        return os << "Sqrt";
      case Op::Cbrt:
        return os << "Cbrt";
      case Op::IsInf:
        return os << "IsInf";
      case Op::IsNaN:
        return os << "IsNaN";
      case Op::Sin:
        return os << "Sin";
      case Op::Cos:
        return os << "Cos";
      case Op::Tan:
        return os << "Tan";
      case Op::Asin:
        return os << "Asin";
      case Op::Acos:
        return os << "Acos";
      case Op::Atan:
        return os << "Atan";
      case Op::Sinh:
        return os << "Sinh";
      case Op::Cosh:
        return os << "Cosh";
      case Op::Tanh:
        return os << "Tanh";
      case Op::Asinh:
        return os << "Asinh";
      case Op::Acosh:
        return os << "Acosh";
      case Op::Atanh:
        return os << "Atanh";
      case Op::Bits:
        return os << "Bits";
      case Op::Len:
        return os << "Len";
      case Op::Ptr:
        return os << "Ptr";
      case Op::Read:
        return os << "Read";
      case Op::Const_E:
        return os << "Const_E";
      case Op::Const_Pi:
        return os << "Const_Pi";
      case Op::Const_Inf:
        return os << "Const_Inf";
      case Op::Const_NaN:
        return os << "Const_NaN";
      default:
        return os << "Unknown";
    }
  }

  void Thread::step()
  {
    assert(frame);
    current_pc = frame->pc;
    auto op = leb<Op>();
    auto process = [this](auto f) INLINE { Operands::process(*this, f); };

    trace_instruction("OP:", op);
    try
    {
      switch (op)
      {
        case Op::Global:
        {
          process([](Register& dst, Global g)
                    INLINE { dst = g.global.copy_reg(); });
          break;
        }

        case Op::Const:
        {
          process([](Thread& self, Register& dst, Constant<ValueType> t)
                    INLINE {
                      switch (t)
                      {
                        case ValueType::None:
                          dst = Value::none();
                          break;

                        case ValueType::Bool:
                        {
                          auto value = self.leb<bool>();
                          dst = Value(value);
                          break;
                        }

                        case ValueType::I8:
                        {
                          auto value = self.leb<int8_t>();
                          dst = Value(value);
                          break;
                        }

                        case ValueType::I16:
                        {
                          auto value = self.leb<int16_t>();
                          dst = Value(value);
                          break;
                        }

                        case ValueType::I32:
                        {
                          auto value = self.leb<int32_t>();
                          dst = Value(value);
                          break;
                        }

                        case ValueType::I64:
                        {
                          auto value = self.leb<int64_t>();
                          dst = Value(value);
                          break;
                        }

                        case ValueType::U8:
                        {
                          auto value = self.leb<uint8_t>();
                          dst = Value(value);
                          break;
                        }

                        case ValueType::U16:
                        {
                          auto value = self.leb<uint16_t>();
                          dst = Value(value);
                          break;
                        }

                        case ValueType::U32:
                        {
                          auto value = self.leb<uint32_t>();
                          dst = Value(value);
                          break;
                        }

                        case ValueType::U64:
                        {
                          auto value = self.leb<uint64_t>();
                          dst = Value(value);
                          break;
                        }

                        case ValueType::ILong:
                        {
                          auto value = self.leb<int64_t>();
                          dst = Value::from_ffi(t, value);
                          break;
                        }
                        case ValueType::ISize:
                        {
                          auto value = self.leb<int64_t>();
                          dst = Value::from_ffi(t, value);
                          break;
                        }

                        case ValueType::ULong:
                        {
                          auto value = self.leb<uint64_t>();
                          dst = Value::from_ffi(t, value);
                          break;
                        }

                        case ValueType::USize:
                        {
                          auto value = self.leb<uint64_t>();
                          dst = Value::from_ffi(t, value);
                          break;
                        }

                        case ValueType::F32:
                        {
                          auto value = self.leb<float>();
                          dst = Value(value);
                          break;
                        }

                        case ValueType::F64:
                        {
                          auto value = self.leb<double>();
                          dst = Value(value);
                          break;
                        }

                        default:
                          throw Value(Error::BadConversion);
                      }
                    });
          break;
        }

        case Op::String:
        {
          process(
            [](Program& program, Register& dst, Constant<size_t> string_id)
              INLINE { dst = Value(program.get_string(string_id)); });
          break;
        }

        case Op::Convert:
        {
          process([](Register& dst, Constant<ValueType> t, const Register& src)
                    INLINE { dst = src.convert(t); });
          break;
        }

        case Op::New:
        {
          process([](Register& dst, Class& cls, Thread& self, Frame& frame)
                    INLINE {
                      if (cls.singleton)
                      {
                        dst = Value(cls.singleton);
                        return;
                      }

                      self.check_args(cls.fields);
                      dst = Value(&frame.region.object(cls)->init(frame, cls));
                    });
          break;
        }

        case Op::Stack:
        {
          process([](
                    Register& dst,
                    Class& cls,
                    Thread& self,
                    Frame& frame,
                    Stack& stack) INLINE {
            if (cls.singleton)
            {
              dst = Value(cls.singleton);
              return;
            }

            self.check_args(cls.fields);
            auto mem = stack.alloc(cls.size);
            auto obj =
              &Object::create(mem, cls, frame.frame_id)->init(frame, cls);
            frame.push_finalizer(obj);
            dst = Value(obj);
          });
          break;
        }

        case Op::Heap:
        {
          process([](
                    Register& dst,
                    const Register& region_loc,
                    Class& cls,
                    Thread& self,
                    Frame& frame) INLINE {
            auto region = region_loc.region();

            if (cls.singleton)
            {
              dst = Value(cls.singleton);
              return;
            }

            self.check_args(cls.fields);
            dst = Value(&region->object(cls)->init(frame, cls));
          });
          break;
        }

        case Op::Region:
        {
          process([](
                    Register& dst,
                    Constant<RegionType> region_type,
                    Class& cls,
                    Thread& self,
                    Frame& frame) INLINE {
            if (cls.singleton)
            {
              throw Value(Error::BadRegionEntryPoint);
            }

            self.check_args(cls.fields);
            auto region = Region::create(region_type);
            dst = Value(&region->object(cls)->init(frame, cls));
          });
          break;
        }

        case Op::NewArray:
        {
          process([](
                    Register& dst,
                    const Register& size,
                    Constant<size_t> type_id,
                    Frame& frame) INLINE {
            dst = Value(frame.region.array(type_id, size.get_size()));
          });
          break;
        }

        case Op::NewArrayConst:
        {
          process([](
                    Register& dst,
                    Constant<size_t> type_id,
                    Constant<size_t> size,
                    Frame& frame)
                    INLINE { dst = Value(frame.region.array(type_id, size)); });
          break;
        }

        case Op::StackArray:
        {
          process([](
                    Register& dst,
                    Stack& stack,
                    const Register& size,
                    Constant<size_t> type_id,
                    Frame& frame) INLINE {
            dst = Value(stack.array(frame.frame_id, type_id, size.get_size()));
          });
          break;
        }

        case Op::StackArrayConst:
        {
          process([](
                    Register& dst,
                    Constant<size_t> type_id,
                    Constant<size_t> size,
                    Stack& stack,
                    Frame& frame) INLINE {
            dst = Value(stack.array(frame.frame_id, type_id, size));
          });
          break;
        }

        case Op::HeapArray:
        {
          process([](
                    Register& dst,
                    const Register& region_loc,
                    const Register& size,
                    Constant<size_t> type_id) INLINE {
            auto region = region_loc.region();
            dst = Value(region->array(type_id, size.get_size()));
          });
          break;
        }

        case Op::HeapArrayConst:
        {
          process([](
                    Register& dst,
                    const Register& region_loc,
                    Constant<size_t> type_id,
                    Constant<size_t> size) INLINE {
            auto region = region_loc.region();
            dst = Value(region->array(type_id, size));
          });
          break;
        }

        case Op::RegionArray:
        {
          process([](
                    Register& dst,
                    Constant<RegionType> region_type,
                    const Register& size,
                    Constant<size_t> type_id) INLINE {
            auto region = Region::create(region_type);
            dst = Value(region->array(type_id, size.get_size()));
          });
          break;
        }

        case Op::RegionArrayConst:
        {
          process([](
                    Register& dst,
                    Constant<RegionType> region_type,
                    Constant<size_t> type_id,
                    Constant<size_t> size) INLINE {
            auto region = Region::create(region_type);
            dst = Value(region->array(type_id, size));
          });
          break;
        }

        case Op::Copy:
        {
          process([](Register& dst, const Register& src)
                    INLINE { dst = src.copy_reg(); });
          break;
        }

        case Op::Move:
        {
          process([](Register& dst, Register src)
                    INLINE { dst = std::move(src); });
          break;
        }

        case Op::Drop:
        {
          process([](Register) INLINE {});
          break;
        }

        case Op::RegisterRef:
        {
          process([](Register& dst, Register& src, Frame& frame)
                    INLINE { dst = Value(src, frame.frame_id); });
          break;
        }

        case Op::FieldRefMove:
        {
          process([](Register& dst, Register src, Constant<size_t> field_id)
                    INLINE { dst = src.ref(true, field_id); });
          break;
        }

        case Op::FieldRefCopy:
        {
          process([](Register& dst, Register& src, Constant<size_t> field_id)
                    INLINE { dst = src.ref(false, field_id); });
          break;
        }

        case Op::ArrayRefMove:
        {
          process([](Register& dst, Register src, const Register& idx)
                    INLINE { dst = src.arrayref(true, idx.get_size()); });
          break;
        }

        case Op::ArrayRefCopy:
        {
          process([](Register& dst, const Register& src, const Register& idx)
                    INLINE { dst = src.arrayref(false, idx.get_size()); });
          break;
        }

        case Op::ArrayRefMoveConst:
        {
          process([](Register& dst, Register src, Constant<size_t> idx)
                    INLINE { dst = src.arrayref(true, idx); });
          break;
        }

        case Op::ArrayRefCopyConst:
        {
          process([](Register& dst, const Register& src, Constant<size_t> idx)
                    INLINE { dst = src.arrayref(false, idx); });
          break;
        }

        case Op::Load:
        {
          process([](Register& dst, const Register& src)
                    INLINE { dst = src.load(); });
          break;
        }

        case Op::StoreMove:
        {
          process([](Register& dst, const Register& ref, Register src)
                    INLINE { dst = ref.store<true>(std::move(src)); });
          break;
        }

        case Op::StoreCopy:
        {
          process([](Register& dst, const Register& ref, const Register& src)
                    INLINE { dst = ref.store<false>(src); });
          break;
        }

        case Op::LookupStatic:
        {
          process([](Register& dst, Function* func)
                    INLINE { dst = Value(func); });
          break;
        }

        case Op::LookupDynamic:
        {
          process(
            [](Register& dst, const Register& src, Constant<size_t> method_id)
              INLINE {
                auto f = src.method(method_id);

                if (!f)
                  throw Value(Error::MethodNotFound);

                dst = Value(f);
              });
          break;
        }

        case Op::LookupFFI:
        {
          process(
            [](Register& dst, Constant<size_t> symbol_id, Program& program)
              INLINE { dst = Value(program.symbol(symbol_id).raw_pointer()); });
          break;
        }

        case Op::ArgMove:
        {
          process([](Register src, ArgReg dst)
                    INLINE { dst = std::move(src); });
          break;
        }

        case Op::ArgCopy:
        {
          process([](const Register& src, ArgReg dst) INLINE { dst = src; });
          break;
        }

        case Op::CallStatic:
        {
          process([](Constant<size_t> dst_id, Function* func, Thread& self)
                    INLINE { self.pushframe(func, dst_id, CallType::Call); });
          break;
        }

        case Op::CallDynamic:
        {
          process(
            [](Constant<size_t> dst_id, const Register& func, Thread& self)
              INLINE {
                self.pushframe(func.function(), dst_id, CallType::Call);
              });
          break;
        }

        case Op::SubcallStatic:
        {
          process(
            [](Constant<size_t> dst_id, Function* func, Thread& self)
              INLINE { self.pushframe(func, dst_id, CallType::Subcall); });
          break;
        }

        case Op::SubcallDynamic:
        {
          process(
            [](Constant<size_t> dst_id, const Register& func, Thread& self)
              INLINE {
                self.pushframe(func.function(), dst_id, CallType::Subcall);
              });
          break;
        }

        case Op::TryStatic:
        {
          process([](Constant<size_t> dst_id, Function* func, Thread& self)
                    INLINE { self.pushframe(func, dst_id, CallType::Catch); });
          break;
        }

        case Op::TryDynamic:
        {
          process(
            [](Constant<size_t> dst_id, const Register& func, Thread& self)
              INLINE {
                self.pushframe(func.function(), dst_id, CallType::Catch);
              });
          break;
        }

        case Op::FFI:
        {
          process([](
                    Register& dst,
                    Constant<size_t> symbol_id,
                    Thread& self,
                    Frame& frame,
                    Program& program) INLINE {
            auto num_args = self.args;
            auto& symbol = program.symbol(symbol_id);
            auto& params = symbol.params();
            auto& paramvals = symbol.paramvals();
            self.check_args(params, symbol.varargs());

            // A Value must be passed as a pointer, not as a struct, since it
            // is a C++ non-trivally constructed type.

            auto& ffi_arg_addrs = self.ffi_arg_addrs;
            auto& ffi_arg_vals = self.ffi_arg_vals;

            if (ffi_arg_addrs.size() < num_args)
            {
              ffi_arg_addrs.resize(num_args);
              ffi_arg_vals.resize(num_args);
            }

            for (size_t i = 0; i < num_args; i++)
            {
              auto& arg = frame.arg(i);
              ffi_arg_vals.at(i) = &arg;

              if (i < params.size())
              {
                if (paramvals.at(i) == ValueType::Invalid)
                  ffi_arg_addrs.at(i) = &ffi_arg_vals.at(i);
                else
                  ffi_arg_addrs.at(i) = arg.to_ffi();
              }
              else
              {
                auto rep = program.layout_type_id(arg.type_id());
                symbol.varparam(rep.second);

                if (rep.first == ValueType::Invalid)
                  ffi_arg_addrs.at(i) = &ffi_arg_vals.at(i);
                else
                  ffi_arg_addrs.at(i) = arg.to_ffi();
              }
            }

            auto ret = symbol.call(ffi_arg_addrs);

            if (
              !ret.is_error() && !program.subtype(ret.type_id(), symbol.ret()))
              throw Value(Error::BadType);

            dst = std::move(ret);
            frame.drop_args(num_args);
          });
          break;
        }

        case Op::WhenStatic:
        {
          process([](
                    Register& dst,
                    Constant<size_t> type_id,
                    Constant<size_t> func_id,
                    Thread& self,
                    Program& program) INLINE {
            auto func = program.function(func_id);
            self.queue_behavior(dst, type_id, func);
          });
          break;
        }

        case Op::WhenDynamic:
        {
          process([](
                    Register& dst,
                    Constant<size_t> type_id,
                    const Register& func,
                    Thread& self) INLINE {
            self.queue_behavior(dst, type_id, func.function());
          });
          break;
        }

        case Op::Typetest:
        {
          process([](
                    Register& dst,
                    const Register& src,
                    Constant<size_t> type_id,
                    Program& program) INLINE {
            dst = Value(program.subtype(src.type_id(), type_id));
          });
          break;
        }

        case Op::TailcallStatic:
        {
          process([](Constant<size_t> func_id, Program& program, Thread& self)
                    INLINE { self.tailcall(program.function(func_id)); });
          break;
        }

        case Op::TailcallDynamic:
        {
          process([](const Register& func, Thread& self)
                    INLINE { self.tailcall(func.function()); });
          break;
        }

        case Op::Return:
        {
          process([](Register ret, Thread& self) INLINE {
            self.popframe(std::move(ret), Condition::Return);
          });
          break;
        }

        case Op::Raise:
        {
          process([](Register ret, Thread& self) INLINE {
            self.popframe(std::move(ret), Condition::Raise);
          });
          break;
        }

        case Op::Throw:
        {
          process([](Register ret, Thread& self) INLINE {
            self.popframe(std::move(ret), Condition::Throw);
          });
          break;
        }

        case Op::Cond:
        {
          process([](
                    const Register& cond,
                    Constant<size_t> on_true,
                    Constant<size_t> on_false,
                    Thread& self) INLINE {
            if (cond.get_bool())
              self.branch(on_true);
            else
              self.branch(on_false);
          });
          break;
        }

        case Op::Jump:
        {
          process([](Constant<size_t> target, Thread& self)
                    INLINE { self.branch(target); });
          break;
        }

#define do_binop(opname) \
  { \
    process([](Register& dst, const Register& lhs, const Register& rhs) \
              INLINE { dst = lhs.op_##opname(rhs); }); \
    break; \
  }
        case Op::Add:
          do_binop(add);
        case Op::Sub:
          do_binop(sub);
        case Op::Mul:
          do_binop(mul);
        case Op::Div:
          do_binop(div);
        case Op::Mod:
          do_binop(mod);
        case Op::Pow:
          do_binop(pow);
        case Op::And:
          do_binop(and);
        case Op::Or:
          do_binop(or);
        case Op::Xor:
          do_binop(xor);
        case Op::Shl:
          do_binop(shl);
        case Op::Shr:
          do_binop(shr);
        case Op::Eq:
          do_binop(eq);
        case Op::Ne:
          do_binop(ne);
        case Op::Lt:
          do_binop(lt);
        case Op::Le:
          do_binop(le);
        case Op::Gt:
          do_binop(gt);
        case Op::Ge:
          do_binop(ge);
        case Op::Min:
          do_binop(min);
        case Op::Max:
          do_binop(max);
        case Op::LogBase:
          do_binop(logbase);
        case Op::Atan2:
          do_binop(atan2);

#define do_unop(opname) \
  { \
    process([](Register& dst, const Register& src) \
              INLINE { dst = src.op_##opname(); }); \
    break; \
  }
        case Op::Neg:
          do_unop(neg);
        case Op::Not:
          do_unop(not );
        case Op::Abs:
          do_unop(abs);
        case Op::Ceil:
          do_unop(ceil);
        case Op::Floor:
          do_unop(floor);
        case Op::Exp:
          do_unop(exp);
        case Op::Log:
          do_unop(log);
        case Op::Sqrt:
          do_unop(sqrt);
        case Op::Cbrt:
          do_unop(cbrt);
        case Op::IsInf:
          do_unop(isinf);
        case Op::IsNaN:
          do_unop(isnan);
        case Op::Sin:
          do_unop(sin);
        case Op::Cos:
          do_unop(cos);
        case Op::Tan:
          do_unop(tan);
        case Op::Asin:
          do_unop(asin);
        case Op::Acos:
          do_unop(acos);
        case Op::Atan:
          do_unop(atan);
        case Op::Sinh:
          do_unop(sinh);
        case Op::Cosh:
          do_unop(cosh);
        case Op::Tanh:
          do_unop(tanh);
        case Op::Asinh:
          do_unop(asinh);
        case Op::Acosh:
          do_unop(acosh);
        case Op::Atanh:
          do_unop(atanh);
        case Op::Bits:
          do_unop(bits);
        case Op::Len:
          do_unop(len);
        case Op::Read:
          do_unop(read);

        case Op::Ptr:
          // TODO unsure about this one, does it really make sense?
          {
            process([](Register& dst, Register& src)
                      INLINE { dst = src.op_ptr(); });
            break;
          }

#define do_const(opname) \
  { \
    process([](Register& dst) INLINE { dst = Value::opname(); }); \
    break; \
  }
        case Op::Const_E:
          do_const(e);
        case Op::Const_Pi:
          do_const(pi);
        case Op::Const_Inf:
          do_const(inf);
        case Op::Const_NaN:
          do_const(nan);

        default:
          throw Value(Error::UnknownOpcode);
      }
    }
    catch (Value& v)
    {
      popframe(Register(std::move(v)), Condition::Throw);
    }
  }

  std::ostream& operator<<(std::ostream& os, CallType calltype)
  {
    switch (calltype)
    {
      case CallType::Call:
        return os << "Call";
      case CallType::Subcall:
        return os << "Subcall";
      case CallType::Catch:
        return os << "Catch";
      default:
        return os << "Unknown";
    }
  }

  void Thread::pushframe(Function* func, size_t dst, CallType calltype)
  {
    if (!func)
      throw Value(Error::MethodNotFound);

    LOG(Trace) << "Call " << program->di_function(func) << " and calltype "
               << calltype;

    check_args(func->param_types);

    // Set how we will handle non-local returns in the current frame.
    Location frame_id = loc::Stack;
    size_t base = 0;
    size_t finalize_base = 0;

    if (frame)
    {
      frame->calltype = calltype;
      frame_id = frame->frame_id + loc::FrameInc;
      base = frame->base + frame->func->registers;
      finalize_base = frame->finalize_top;
    }

    // Make sure there's enough register space.
    auto req_stack_size = base + func->registers;

    while (locals.size() < req_stack_size)
      locals.resize(locals.size() * 2);

    frames.emplace_back(
      func,
      frame_id,
      stack.save(),
      locals,
      base,
      finalize,
      finalize_base,
      func->labels.at(0),
      dst,
      CallType::Call);

    frame = &frames.back();
  }

  void Thread::popframe(Register ret_, Condition condition)
  {
    // Create temp register to keep return alive across dropping of the frame.
    Register ret{std::move(ret_)};

    // Save the destination register.
    auto dst = frame->dst;

    // Clear any unused arguments.
    frame->drop_args(args);

    // Check for stack escapes.
    auto retloc = ret.location();

    if (retloc == frame->frame_id)
    {
      // The return value can't be stack allocated in this frame.
      ret = Value(Error::BadStackEscape);
      condition = Condition::Throw;
    }
    else if (
      loc::is_region(retloc) && loc::to_region(retloc)->is_frame_local() &&
      (loc::to_region(retloc)->get_parent() == frame->frame_id))
    {
      if (frames.size() > 1)
      {
        // Drag the frame-local allocation to the previous frame.
        auto& prev_frame = frames.at(frames.size() - 2);

        if (!drag_allocation(&prev_frame.region, ret.get_header()))
        {
          ret = Value(Error::BadStackEscape);
          condition = Condition::Throw;
        }
      }
      else
      {
        // Drag the frame-local allocation to a fresh region.
        auto r = Region::create(RegionType::RegionRC);

        if (!drag_allocation(r, ret.get_header()))
        {
          ret = Value(Error::BadStackEscape);
          condition = Condition::Throw;
          r->free_region();
        }
      }
    }

    switch (condition)
    {
      case Condition::Return:
        if (
          !ret.is_error() &&
          !program->subtype(ret.type_id(), frame->func->return_type))
        {
          ret = Value(Error::BadType);
          condition = Condition::Throw;
        }
        break;

      case Condition::Raise:
      case Condition::Throw:
        // TODO: check against the raise type and throw type.
        break;
    }

    teardown();
    frames.pop_back();

    if (frames.empty())
    {
      locals.at(0) = std::move(ret);
      frame = nullptr;
      return;
    }

    frame = &frames.back();

    switch (frame->calltype)
    {
      case CallType::Call:
      {
        // This unwraps a Raise.
        // Return (nothing), raise (pop as return), throw (pop)
        switch (condition)
        {
          case Condition::Raise:
            popframe(std::move(ret), Condition::Return);
            return;

          case Condition::Throw:
            popframe(std::move(ret), Condition::Throw);
            return;

          default:
            break;
        }
        break;
      }

      case CallType::Subcall:
      {
        // This does not unwrap a Raise.
        // Return (nothing), raise (pop), throw (pop)
        if (condition != Condition::Return)
        {
          popframe(std::move(ret), condition);
          return;
        }
        break;
      }

      default:
        // Try: return (nothing), raise (nothing), throw (nothing)
        // TODO: this will catch internal errors as well, should it?
        break;
    }

    frame->local(dst) = std::move(ret);
    frame->calltype = CallType::Call;
  }

  void Thread::tailcall(Function* func)
  {
    if (!func)
      throw Value(Error::MethodNotFound);

    // Preserve the frame-local region.
    teardown(true);
    check_args(func->param_types);

    // Move arguments back to the current frame.
    bool stack_escape = false;

    for (size_t i = 0; i < func->param_types.size(); i++)
    {
      auto& arg = frame->arg(i);

      if (arg.location() == frame->frame_id)
        stack_escape = true;

      frame->local(i) = std::move(arg);
    }

    // Can't tailcall with stack allocations.
    if (stack_escape)
      throw Value(Error::BadStackEscape);

    // Set the new function and program counter.
    frame->func = func;
    frame->pc = func->labels.at(0);
    frame->calltype = CallType::Call;
  }

  void Thread::teardown(bool tailcall)
  {
    // Drop all frame registers.
    frame->drop();

    // Finalize the stack.
    for (size_t i = frame->finalize_base; i < frame->finalize_top; i++)
      finalize.at(i)->finalize();

    // Finalize the frame-local region. A tailcall preserves the region.
    if (!tailcall)
      frame->region.free_contents();

    // Pop the stack.
    stack.restore(frame->save);
  }

  void Thread::branch(size_t label)
  {
    if (label >= frame->func->labels.size())
      throw Value(Error::BadLabel);

    frame->pc = frame->func->labels.at(label);
  }

  void Thread::check_args(std::vector<uint32_t>& types, bool vararg)
  {
    if ((args < types.size()) || (!vararg && (args > types.size())))
    {
      drop_args();
      throw Value(Error::BadArgs);
    }

    for (size_t i = 0; i < types.size(); i++)
    {
      if (!program->subtype(arg(i).type_id(), types.at(i)))
      {
        drop_args();
        throw Value(Error::BadType);
      }
    }

    args = 0;
  }

  void Thread::check_args(std::vector<Field>& fields)
  {
    if (args != fields.size())
    {
      frame->drop_args(args);
      throw Value(Error::BadArgs);
    }

    for (size_t i = 0; i < args; i++)
    {
      if (!program->subtype(frame->arg(i).type_id(), fields.at(i).type_id))
      {
        frame->drop_args(args);
        throw Value(Error::BadType);
      }
    }

    args = 0;
  }

  Register& Thread::arg(size_t idx)
  {
    // Only use this when we don't know if we have a frame or not.
    if (frame) [[likely]]
      return frame->arg(idx);

    return locals.at(idx);
  }

  void Thread::drop_args()
  {
    // Only use this when we don't know if we have a frame or not.
    if (frame) [[likely]]
    {
      frame->drop_args(args);
    }
    else
    {
      for (size_t i = 0; i < args; i++)
        locals.at(i).drop_reg();
    }

    args = 0;
  }

  void
  Thread::queue_behavior(Register& result, uint32_t type_id, Function* func)
  {
    if (func->param_types.size() != args)
      throw Value(Error::BadArgs);

    auto& params = func->param_types;
    bool is_closure = false;
    size_t num_cowns = args;
    size_t first_cown = 0;

    if (args > 0)
    {
      auto& closure = frame->arg(0);

      if (!closure.is_cown())
      {
        // The first argument is the closure data.
        is_closure = true;

        if (!program->subtype(closure.type_id(), params.at(0)))
          throw Value(Error::BadArgs);

        if (!closure.is_sendable())
          throw Value(Error::BadArgs);

        num_cowns--;
        first_cown++;
      }
    }

    // Check that all other args are cowns of the right type.
    for (size_t i = first_cown; i < args; i++)
    {
      auto cown = frame->arg(i).get_cown();
      auto ref_type = program->ref(cown->content_type_id());

      if (!program->subtype(ref_type, params.at(i)))
        throw Value(Error::BadArgs);
    }

    args = 0;

    // Create the result cown.
    auto result_cown = Cown::create(type_id);

    if (!program->subtype(func->return_type, result_cown->content_type_id()))
      throw Value(Error::BadType);

    result = Value(result_cown);

    // Slot 0 is the result cown.
    auto b = verona::rt::BehaviourCore::make(
      num_cowns + 1, run_behavior, sizeof(Value) * 2);
    auto slots = b->get_slots();
    new (&slots[0]) verona::rt::Slot(result_cown);

    for (size_t i = 0; i < num_cowns; i++)
    {
      // The first cown argument position depends on whether this is a
      // closure or not.
      auto arg = std::move(frame->arg(first_cown + i));

      // Offset the slot by 1 to account for the result cown.
      auto& slot = slots[i + 1];
      new (&slot) verona::rt::Slot(arg.get_cown());
      // TODO optimise as this is all move.  Can set is_move on slot,
      // if we correctly invalidate the arg while scheduling.

      if (arg.is_readonly())
        slot.set_read_only();
    }

    new (&b->get_body<Value>()[0]) Value(func);
    auto cvalue = new (&b->get_body<Value>()[1]) Value();

    if (is_closure)
    {
      auto& closure = frame->arg(0);

      if (closure.is_header())
      {
        auto r = closure.get_header()->region();

        // Set the region parent, as it's captured by the behavior.
        if (r)
          r->set_parent();
      }

      *cvalue = std::move(closure);
    }

    verona::rt::BehaviourCore::schedule_many(&b, 1);
  }
}

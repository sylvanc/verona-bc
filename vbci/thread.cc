#include "thread.h"

#include "array.h"
#include "cown.h"
#include "object.h"
#include "program.h"

namespace vbci
{
  inline Op opcode(Code code)
  {
    // 8 bit opcode.
    return static_cast<Op>(code & 0xFF);
  }

  inline Local arg0(Code code)
  {
    // 8 bit local index.
    return (code >> 8) & 0xFF;
  }

  inline Local arg1(Code code)
  {
    // 8 bit local index.
    return (code >> 16) & 0xFF;
  }

  inline Local arg2(Code code)
  {
    // 8 bit local index.
    return (code >> 24) & 0xFF;
  }

  Thread& Thread::get()
  {
    thread_local Thread thread;
    return thread;
  }

  Value Thread::run(Function* func)
  {
    return get().thread_run(func);
  }

  void Thread::run_finalizer(Object* obj)
  {
    get().thread_run_finalizer(obj);
  }

  Value Thread::thread_run(Function* func)
  {
    auto depth = frames.size();
    pushframe(func, 0, Condition::Throw);

    while (depth != frames.size())
      step();

    return std::move(locals.at(0));
  }

  void Thread::thread_run_finalizer(Object* obj)
  {
    LOG(Debug) << "Running finalizer for " << obj;

    if (frame)
    {
      frame->drop_args(args);
      frame->arg(0) = Value(obj, true);
    }
    else
    {
      locals.at(0) = Value(obj, true);
    }

    args = 1;
    run(obj->finalizer()).drop();
  }

  void Thread::step()
  {
    assert(frame);
    current_pc = frame->pc;
    auto code = program->load_code(frame->pc);
    auto op = opcode(code);

    try
    {
      switch (op)
      {
        case Op::Global:
        {
          auto& dst = frame->local(arg0(code));
          dst = program->global(program->load_u32(frame->pc));
          break;
        }

        case Op::Const:
        {
          auto& dst = frame->local(arg0(code));
          auto t = static_cast<ValueType>(arg1(code));

          switch (t)
          {
            case ValueType::None:
              dst = Value::none();
              break;

            case ValueType::Bool:
              dst = Value(static_cast<bool>(arg2(code)));
              break;

            case ValueType::I8:
              dst = Value(static_cast<int8_t>(arg2(code)));
              break;

            case ValueType::I16:
              dst = Value(program->load_i16(frame->pc));
              break;

            case ValueType::I32:
              dst = Value(program->load_i32(frame->pc));
              break;

            case ValueType::I64:
              dst = Value(program->load_i64(frame->pc));
              break;

            case ValueType::U8:
              dst = Value(arg2(code));
              break;

            case ValueType::U16:
              dst = Value(program->load_u16(frame->pc));
              break;

            case ValueType::U32:
              dst = Value(program->load_u32(frame->pc));
              break;

            case ValueType::U64:
              dst = Value(program->load_u64(frame->pc));
              break;

            case ValueType::ILong:
            case ValueType::ULong:
            case ValueType::ISize:
            case ValueType::USize:
              dst = Value::from_ffi(t, program->load_u64(frame->pc));
              break;

            case ValueType::F32:
              dst = Value(program->load_f32(frame->pc));
              break;

            case ValueType::F64:
              dst = Value(program->load_f64(frame->pc));
              break;

            default:
              throw Value(Error::UnknownPrimitiveType);
          }
          break;
        }

        case Op::Stack:
        {
          auto& dst = frame->local(arg0(code));
          auto& cls = program->cls(program->load_u32(frame->pc));
          check_args(cls.fields);
          auto mem = stack.alloc(cls.size);
          auto obj =
            &Object::create(mem, cls, frame->frame_id)->init(frame, cls);
          frame->push_finalizer(obj);
          dst = obj;
          break;
        }

        case Op::Heap:
        {
          auto& dst = frame->local(arg0(code));
          auto& cls = program->cls(program->load_u32(frame->pc));
          check_args(cls.fields);
          auto region = frame->local(arg1(code)).region();
          dst = &region->object(cls)->init(frame, cls);
          break;
        }

        case Op::Region:
        {
          auto& dst = frame->local(arg0(code));
          auto& cls = program->cls(program->load_u32(frame->pc));
          check_args(cls.fields);
          auto region = Region::create(static_cast<RegionType>(arg1(code)));
          dst = &region->object(cls)->init(frame, cls);
          break;
        }

        case Op::StackArray:
        {
          auto& dst = frame->local(arg0(code));
          auto size = frame->local(arg1(code)).to_index();
          auto type_id = program->load_u32(frame->pc);
          auto rep = program->layout_type_id(type_id);
          auto mem = stack.alloc(Array::size_of(size, rep.second->size));
          dst = Array::create(
            mem, frame->frame_id, type_id, rep.first, size, rep.second->size);
          break;
        }

        case Op::StackArrayConst:
        {
          auto& dst = frame->local(arg0(code));
          auto type_id = program->load_u32(frame->pc);
          auto size = program->load_u64(frame->pc);
          auto rep = program->layout_type_id(type_id);
          auto mem = stack.alloc(Array::size_of(size, rep.second->size));
          dst = Array::create(
            mem, frame->frame_id, type_id, rep.first, size, rep.second->size);
          break;
        }

        case Op::HeapArray:
        {
          auto& dst = frame->local(arg0(code));
          auto region = frame->local(arg1(code)).region();
          auto size = frame->local(arg2(code)).to_index();
          auto type_id = program->load_u32(frame->pc);
          dst = region->array(type_id, size);
          break;
        }

        case Op::HeapArrayConst:
        {
          auto& dst = frame->local(arg0(code));
          auto region = frame->local(arg1(code)).region();
          auto type_id = program->load_u32(frame->pc);
          auto size = program->load_u64(frame->pc);
          dst = region->array(type_id, size);
          break;
        }

        case Op::RegionArray:
        {
          auto& dst = frame->local(arg0(code));
          auto region = Region::create(static_cast<RegionType>(arg1(code)));
          auto type_id = program->load_u32(frame->pc);
          auto size = frame->local(arg2(code)).to_index();
          dst = region->array(type_id, size);
          break;
        }

        case Op::RegionArrayConst:
        {
          auto& dst = frame->local(arg0(code));
          auto region = Region::create(static_cast<RegionType>(arg1(code)));
          auto type_id = program->load_u32(frame->pc);
          auto size = program->load_u64(frame->pc);
          dst = region->array(type_id, size);
          break;
        }

        case Op::Copy:
        {
          auto& dst = frame->local(arg0(code));
          auto& src = frame->local(arg1(code));
          dst = src;
          break;
        }

        case Op::Move:
        {
          auto& dst = frame->local(arg0(code));
          auto& src = frame->local(arg1(code));
          dst = std::move(src);
          break;
        }

        case Op::Drop:
        {
          auto& dst = frame->local(arg0(code));
          dst.drop();
          break;
        }

        case Op::Ref:
        {
          auto& dst = frame->local(arg0(code));
          auto arg_type = static_cast<ArgType>(arg1(code));
          auto& src = frame->local(arg2(code));
          dst = src.ref(arg_type, program->load_u32(frame->pc));
          break;
        }

        case Op::ArrayRefMove:
        {
          auto& dst = frame->local(arg0(code));
          auto& src = frame->local(arg1(code));
          auto& idx = frame->local(arg2(code));
          dst = src.arrayref(ArgType::Move, idx.to_index());
          break;
        }

        case Op::ArrayRefCopy:
        {
          auto& dst = frame->local(arg0(code));
          auto& src = frame->local(arg1(code));
          auto& idx = frame->local(arg2(code));
          dst = src.arrayref(ArgType::Copy, idx.to_index());
          break;
        }

        case Op::ArrayRefConst:
        {
          auto& dst = frame->local(arg0(code));
          auto arg_type = static_cast<ArgType>(arg1(code));
          auto& src = frame->local(arg2(code));
          auto idx = program->load_u64(frame->pc);
          dst = src.arrayref(arg_type, idx);
          break;
        }

        case Op::Load:
        {
          auto& dst = frame->local(arg0(code));
          auto& src = frame->local(arg1(code));
          dst = src.load();
          break;
        }

        case Op::StoreMove:
        {
          auto& dst = frame->local(arg0(code));
          auto& ref = frame->local(arg1(code));
          auto& src = frame->local(arg2(code));
          dst = ref.store(ArgType::Move, src);
          break;
        }

        case Op::StoreCopy:
        {
          auto& dst = frame->local(arg0(code));
          auto& ref = frame->local(arg1(code));
          auto& src = frame->local(arg2(code));
          dst = ref.store(ArgType::Copy, src);
          break;
        }

        case Op::Lookup:
        {
          auto& dst = frame->local(arg0(code));
          auto call_type = static_cast<CallType>(arg1(code));
          auto func_id = program->load_u32(frame->pc);

          switch (call_type)
          {
            case CallType::CallStatic:
            {
              dst = program->function(func_id);
              break;
            }

            case CallType::CallDynamic:
            {
              auto& src = frame->local(arg2(code));
              dst = src.method(func_id);
              break;
            }

            default:
              throw Value(Error::UnknownCallType);
          }
          break;
        }

        case Op::Arg:
        {
          auto& dst = frame->arg(args++);
          auto arg_type = static_cast<ArgType>(arg0(code));
          auto& src = frame->local(arg1(code));

          switch (arg_type)
          {
            case ArgType::Move:
              dst = std::move(src);
              break;

            case ArgType::Copy:
              dst = src;
              break;

            default:
              throw Value(Error::UnknownArgType);
          }
          break;
        }

        case Op::Call:
        {
          auto dst = arg0(code);
          auto call_type = static_cast<CallType>(arg1(code));
          Function* func;
          Condition cond;

          switch (call_type)
          {
            case CallType::CallStatic:
            {
              func = program->function(program->load_u32(frame->pc));
              cond = Condition::Return;
              break;
            }

            case CallType::SubcallStatic:
            {
              func = program->function(program->load_u32(frame->pc));
              cond = Condition::Raise;
              break;
            }

            case CallType::TryStatic:
            {
              func = program->function(program->load_u32(frame->pc));
              cond = Condition::Throw;
              break;
            }

            case CallType::CallDynamic:
            {
              func = frame->local(arg2(code)).function();
              cond = Condition::Return;
              break;
            }

            case CallType::SubcallDynamic:
            {
              func = frame->local(arg2(code)).function();
              cond = Condition::Raise;
              break;
            }

            case CallType::TryDynamic:
            {
              func = frame->local(arg2(code)).function();
              cond = Condition::Throw;
              break;
            }

            case CallType::FFI:
            {
              auto& symbol = program->symbol(program->load_u32(frame->pc));
              auto& params = symbol.params();
              auto param_size = params.size();
              check_args(params);

              if (ffi_arg_addrs.size() < param_size)
              {
                ffi_arg_addrs.resize(param_size);

                for (size_t i = 0; i < param_size; i++)
                  ffi_arg_addrs.at(i) = frame->arg(i).address_of();
              }

              frame->drop_args(args);
              auto ret =
                Value::from_ffi(symbol.retval(), symbol.call(ffi_arg_addrs));

              if (
                !ret.is_error() &&
                !program->typecheck(ret.type_id(), symbol.ret()))
              {
                throw Value(Error::BadType);
              }

              frame->local(dst) = ret;
              return;
            }

            default:
              throw Value(Error::UnknownCallType);
          }

          pushframe(func, dst, cond);
          break;
        }

        case Op::Typetest:
        {
          auto& dst = frame->local(arg0(code));
          auto& src = frame->local(arg1(code));
          Id type_id = program->load_u32(frame->pc);
          dst = program->typecheck(src.type_id(), type_id);
          break;
        }

        case Op::Tailcall:
        {
          auto call_type = static_cast<CallType>(arg1(code));
          Function* func;

          switch (call_type)
          {
            case CallType::CallStatic:
            {
              func = program->function(program->load_u32(frame->pc));
              break;
            }

            case CallType::CallDynamic:
            {
              func = frame->local(arg2(code)).function();
              break;
            }

            default:
              throw Value(Error::UnknownCallType);
          }

          tailcall(func);
          break;
        }

        case Op::Return:
        {
          auto ret = std::move(frame->local(arg0(code)));
          popframe(ret, static_cast<Condition>(arg1(code)));
          break;
        }

        case Op::Cond:
        {
          auto& cond = frame->local(arg0(code));
          auto on_true = arg1(code);
          auto on_false = arg2(code);

          if (cond.get_bool())
            branch(on_true);
          else
            branch(on_false);
          break;
        }

        case Op::Jump:
        {
          branch(arg0(code));
          break;
        }

#define do_binop(op) \
  { \
    auto& dst = frame->local(arg0(code)); \
    auto& lhs = frame->local(arg1(code)); \
    auto& rhs = frame->local(arg2(code)); \
    dst = lhs.op_##op(rhs); \
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

        case Op::MathOp:
        {
          auto& dst = frame->local(arg0(code));
          auto mop = static_cast<MathOp>(arg1(code));

          switch (mop)
          {
#define do_unop(op) \
  { \
    auto& src = frame->local(arg2(code)); \
    dst = src.op_##op(); \
    break; \
  }
            case MathOp::Neg:
              do_unop(neg);
            case MathOp::Not:
              do_unop(not);
            case MathOp::Abs:
              do_unop(abs);
            case MathOp::Ceil:
              do_unop(ceil);
            case MathOp::Floor:
              do_unop(floor);
            case MathOp::Exp:
              do_unop(exp);
            case MathOp::Log:
              do_unop(log);
            case MathOp::Sqrt:
              do_unop(sqrt);
            case MathOp::Cbrt:
              do_unop(cbrt);
            case MathOp::IsInf:
              do_unop(isinf);
            case MathOp::IsNaN:
              do_unop(isnan);
            case MathOp::Sin:
              do_unop(sin);
            case MathOp::Cos:
              do_unop(cos);
            case MathOp::Tan:
              do_unop(tan);
            case MathOp::Asin:
              do_unop(asin);
            case MathOp::Acos:
              do_unop(acos);
            case MathOp::Atan:
              do_unop(atan);
            case MathOp::Sinh:
              do_unop(sinh);
            case MathOp::Cosh:
              do_unop(cosh);
            case MathOp::Tanh:
              do_unop(tanh);
            case MathOp::Asinh:
              do_unop(asinh);
            case MathOp::Acosh:
              do_unop(acosh);
            case MathOp::Atanh:
              do_unop(atanh);
            case MathOp::Len:
              do_unop(len);
            case MathOp::ArrayPtr:
              do_unop(arrayptr);

#define do_const(op) \
  { \
    dst = Value::op(); \
    break; \
  }
            case MathOp::Const_E:
              do_const(e);
            case MathOp::Const_Pi:
              do_const(pi);
            case MathOp::Const_Inf:
              do_const(inf);
            case MathOp::Const_NaN:
              do_const(nan);

            default:
              throw Value(Error::UnknownMathOp);
          }
          break;
        }

        case Op::Convert:
        {
          auto& dst = frame->local(arg0(code));
          auto t = static_cast<ValueType>(arg1(code));
          auto& src = frame->local(arg2(code));
          dst = src.convert(t);
          break;
        }

        default:
          throw Value(Error::UnknownOpcode);
      }
    }
    catch (Value& v)
    {
      v.annotate(frame->func, current_pc);
      popframe(v, Condition::Throw);
    }
  }

  void Thread::pushframe(Function* func, Local dst, Condition condition)
  {
    assert(func);
    check_args(func->param_types);

    // Set how we will handle non-local returns in the current frame.
    Location frame_id = StackAlloc;
    size_t base = 0;
    size_t finalize_base = 0;

    if (frame)
    {
      frame->condition = condition;
      frame_id = frame->frame_id + 2;
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
      Condition::Return);

    frame = &frames.back();
  }

  void Thread::popframe(Value& ret, Condition condition)
  {
    // Save the destination register.
    auto dst = frame->dst;

    // Clear any unused arguments.
    frame->drop_args(args);

    // The return value can't be allocated in this frame.
    if (ret.location() == frame->frame_id)
    {
      ret = Value(Error::BadStackEscape);
      ret.annotate(frame->func, current_pc);
      condition = Condition::Throw;
    }

    switch (condition)
    {
      case Condition::Return:
        if (
          !ret.is_error() &&
          !program->typecheck(ret.type_id(), frame->func->return_type))
        {
          ret = Value(Error::BadType);
          ret.annotate(frame->func, current_pc);
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
      // TODO: store to the result cown?
      locals.at(0) = std::move(ret);
      frame = nullptr;
      return;
    }

    frame = &frames.back();

    switch (frame->condition)
    {
      case Condition::Return:
      {
        // This unwraps a Raise.
        // Return (nothing), raise (pop as return), throw (pop)
        switch (condition)
        {
          case Condition::Raise:
            popframe(ret, Condition::Return);
            return;

          case Condition::Throw:
            popframe(ret, Condition::Throw);
            return;

          default:
            break;
        }
        break;
      }

      case Condition::Raise:
      {
        // This does not unwrap a Raise.
        // Return (nothing), raise (pop), throw (pop)
        if (condition != Condition::Return)
        {
          popframe(ret, condition);
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
    frame->condition = Condition::Return;
  }

  void Thread::tailcall(Function* func)
  {
    assert(func);
    teardown();
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
    frame->condition = Condition::Return;
  }

  void Thread::teardown()
  {
    frame->drop();

    for (size_t i = frame->finalize_base; i < frame->finalize_top; i++)
      thread_run_finalizer(finalize.at(i));

    stack.restore(frame->save);
  }

  void Thread::branch(Local label)
  {
    if (label >= frame->func->labels.size())
      throw Value(Error::BadLabel);

    frame->pc = frame->func->labels.at(label);
  }

  void Thread::check_args(std::vector<Id>& types)
  {
    if (args != types.size())
    {
      frame->drop_args(args);
      throw Value(Error::BadArgs);
    }

    for (size_t i = 0; i < args; i++)
    {
      if (!program->typecheck(frame->arg(i).type_id(), types.at(i)))
        throw Value(Error::BadType);
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
      if (!program->typecheck(frame->arg(i).type_id(), fields.at(i).type_id))
        throw Value(Error::BadType);
    }

    args = 0;
  }
}

#include "thread.h"

#include "array.h"
#include "cown.h"
#include "object.h"
#include "program.h"

namespace vbci
{
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
    auto op = leb<Op>();

    try
    {
      switch (op)
      {
        case Op::Global:
        {
          auto& dst = frame->local(leb());
          dst = program->global(leb());
          break;
        }

        case Op::Const:
        {
          auto& dst = frame->local(leb());
          auto t = leb<ValueType>();

          switch (t)
          {
            case ValueType::None:
              dst = Value::none();
              break;

            case ValueType::Bool:
              dst = Value(leb<bool>());
              break;

            case ValueType::I8:
              dst = Value(leb<int8_t>());
              break;

            case ValueType::I16:
              dst = Value(leb<int16_t>());
              break;

            case ValueType::I32:
              dst = Value(leb<int32_t>());
              break;

            case ValueType::I64:
              dst = Value(leb<int64_t>());
              break;

            case ValueType::U8:
              dst = Value(leb<uint8_t>());
              break;

            case ValueType::U16:
              dst = Value(leb<uint16_t>());
              break;

            case ValueType::U32:
              dst = Value(leb<uint32_t>());
              break;

            case ValueType::U64:
              dst = Value(leb<uint64_t>());
              break;

            case ValueType::ILong:
            case ValueType::ISize:
              dst = Value::from_ffi(t, leb<int64_t>());
              break;

            case ValueType::ULong:
            case ValueType::USize:
              dst = Value::from_ffi(t, leb<uint64_t>());
              break;

            case ValueType::F32:
              dst = Value(leb<float>());
              break;

            case ValueType::F64:
              dst = Value(leb<double>());
              break;

            default:
              throw Value(Error::UnknownPrimitiveType);
          }
          break;
        }

        case Op::Convert:
        {
          auto& dst = frame->local(leb());
          auto t = leb<ValueType>();
          auto& src = frame->local(leb());
          dst = src.convert(t);
          break;
        }

        case Op::Stack:
        {
          auto& dst = frame->local(leb());
          auto& cls = program->cls(leb());
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
          auto& dst = frame->local(leb());
          auto region = frame->local(leb()).region();
          auto& cls = program->cls(leb());
          check_args(cls.fields);
          dst = &region->object(cls)->init(frame, cls);
          break;
        }

        case Op::Region:
        {
          auto& dst = frame->local(leb());
          auto region = Region::create(leb<RegionType>());
          auto& cls = program->cls(leb());
          check_args(cls.fields);
          dst = &region->object(cls)->init(frame, cls);
          break;
        }

        case Op::StackArray:
        {
          auto& dst = frame->local(leb());
          auto size = frame->local(leb()).to_index();
          auto type_id = leb();
          auto rep = program->layout_type_id(type_id);
          auto mem = stack.alloc(Array::size_of(size, rep.second->size));
          dst = Array::create(
            mem, frame->frame_id, type_id, rep.first, size, rep.second->size);
          break;
        }

        case Op::StackArrayConst:
        {
          auto& dst = frame->local(leb());
          auto type_id = leb();
          auto size = leb();
          auto rep = program->layout_type_id(type_id);
          auto mem = stack.alloc(Array::size_of(size, rep.second->size));
          dst = Array::create(
            mem, frame->frame_id, type_id, rep.first, size, rep.second->size);
          break;
        }

        case Op::HeapArray:
        {
          auto& dst = frame->local(leb());
          auto region = frame->local(leb()).region();
          auto size = frame->local(leb()).to_index();
          auto type_id = leb();
          dst = region->array(type_id, size);
          break;
        }

        case Op::HeapArrayConst:
        {
          auto& dst = frame->local(leb());
          auto region = frame->local(leb()).region();
          auto type_id = leb();
          auto size = leb();
          dst = region->array(type_id, size);
          break;
        }

        case Op::RegionArray:
        {
          auto& dst = frame->local(leb());
          auto region = Region::create(leb<RegionType>());
          auto size = frame->local(leb()).to_index();
          auto type_id = leb();
          dst = region->array(type_id, size);
          break;
        }

        case Op::RegionArrayConst:
        {
          auto& dst = frame->local(leb());
          auto region = Region::create(leb<RegionType>());
          auto type_id = leb();
          auto size = leb();
          dst = region->array(type_id, size);
          break;
        }

        case Op::Copy:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          dst = src;
          break;
        }

        case Op::Move:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          dst = std::move(src);
          break;
        }

        case Op::Drop:
        {
          auto& dst = frame->local(leb());
          dst.drop();
          break;
        }

        case Op::RefMove:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          dst = src.ref(true, leb());
          break;
        }

        case Op::RefCopy:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          dst = src.ref(false, leb());
          break;
        }

        case Op::ArrayRefMove:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          auto& idx = frame->local(leb());
          dst = src.arrayref(true, idx.to_index());
          break;
        }

        case Op::ArrayRefCopy:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          auto& idx = frame->local(leb());
          dst = src.arrayref(false, idx.to_index());
          break;
        }

        case Op::ArrayRefMoveConst:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          auto idx = leb();
          dst = src.arrayref(true, idx);
          break;
        }

        case Op::ArrayRefCopyConst:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          auto idx = leb();
          dst = src.arrayref(false, idx);
          break;
        }

        case Op::Load:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          dst = src.load();
          break;
        }

        case Op::StoreMove:
        {
          auto& dst = frame->local(leb());
          auto& ref = frame->local(leb());
          auto& src = frame->local(leb());
          dst = ref.store(true, src);
          break;
        }

        case Op::StoreCopy:
        {
          auto& dst = frame->local(leb());
          auto& ref = frame->local(leb());
          auto& src = frame->local(leb());
          dst = ref.store(false, src);
          break;
        }

        case Op::LookupStatic:
        {
          auto& dst = frame->local(leb());
          dst = program->function(leb());
          break;
        }

        case Op::LookupDynamic:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          auto func_id = leb();
          dst = src.method(func_id);
          break;
        }

        case Op::ArgMove:
        {
          auto& dst = frame->arg(args++);
          auto& src = frame->local(leb());
          dst = std::move(src);
          break;
        }

        case Op::ArgCopy:
        {
          auto& dst = frame->arg(args++);
          auto& src = frame->local(leb());
          dst = src;
          break;
        }

        case Op::CallStatic:
        {
          auto dst = leb();
          auto func = program->function(leb());
          pushframe(func, dst, Condition::Return);
          break;
        }

        case Op::CallDynamic:
        {
          auto dst = leb();
          auto func = frame->local(leb()).function();
          pushframe(func, dst, Condition::Return);
          break;
        }

        case Op::SubcallStatic:
        {
          auto dst = leb();
          auto func = program->function(leb());
          pushframe(func, dst, Condition::Raise);
          break;
        }

        case Op::SubcallDynamic:
        {
          auto dst = leb();
          auto func = frame->local(leb()).function();
          pushframe(func, dst, Condition::Raise);
          break;
        }

        case Op::TryStatic:
        {
          auto dst = leb();
          auto func = program->function(leb());
          pushframe(func, dst, Condition::Throw);
          break;
        }

        case Op::TryDynamic:
        {
          auto dst = leb();
          auto func = frame->local(leb()).function();
          pushframe(func, dst, Condition::Throw);
          break;
        }

        case Op::FFI:
        {
          auto dst = leb();
          auto num_args = args;
          auto& symbol = program->symbol(leb());
          check_args(symbol.params(), symbol.varargs());

          if (ffi_arg_addrs.size() < num_args)
            ffi_arg_addrs.resize(num_args);

          for (size_t i = 0; i < num_args; i++)
          {
            auto& arg = frame->arg(i);
            ffi_arg_addrs.at(i) = arg.address_of();

            if (i >= symbol.params().size())
            {
              symbol.varparam(program->layout_type_id(arg.type_id()).second);
            }
          }

          frame->drop_args(num_args);
          auto ret =
            Value::from_ffi(symbol.retval(), symbol.call(ffi_arg_addrs));

          if (
            !ret.is_error() && !program->typecheck(ret.type_id(), symbol.ret()))
          {
            throw Value(Error::BadType);
          }

          frame->local(dst) = ret;
          break;
        }

        case Op::Typetest:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          auto type_id = leb();
          dst = program->typecheck(src.type_id(), type_id);
          break;
        }

        case Op::TailcallStatic:
        {
          tailcall(program->function(leb()));
          break;
        }

        case Op::TailcallDynamic:
        {
          tailcall(frame->local(leb()).function());
          break;
        }

        case Op::Return:
        {
          auto ret = std::move(frame->local(leb()));
          popframe(ret, Condition::Return);
          break;
        }

        case Op::Raise:
        {
          auto ret = std::move(frame->local(leb()));
          popframe(ret, Condition::Raise);
          break;
        }

        case Op::Throw:
        {
          auto ret = std::move(frame->local(leb()));
          popframe(ret, Condition::Throw);
          break;
        }

        case Op::Cond:
        {
          auto& cond = frame->local(leb());
          auto on_true = leb();
          auto on_false = leb();

          if (cond.get_bool())
            branch(on_true);
          else
            branch(on_false);
          break;
        }

        case Op::Jump:
        {
          branch(leb());
          break;
        }

#define do_binop(op) \
  { \
    auto& dst = frame->local(leb()); \
    auto& lhs = frame->local(leb()); \
    auto& rhs = frame->local(leb()); \
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

#define do_unop(op) \
  { \
    auto& dst = frame->local(leb()); \
    auto& src = frame->local(leb()); \
    dst = src.op_##op(); \
    break; \
  }
        case Op::Neg:
          do_unop(neg);
        case Op::Not:
          do_unop(not);
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
        case Op::Len:
          do_unop(len);
        case Op::ArrayPtr:
          do_unop(arrayptr);
        case Op::StructPtr:
          do_unop(structptr);

#define do_const(op) \
  { \
    auto dst = frame->local(leb()); \
    dst = Value::op(); \
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
      v.annotate(frame->func, current_pc);
      popframe(v, Condition::Throw);
    }
  }

  void Thread::pushframe(Function* func, size_t dst, Condition condition)
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

  void Thread::branch(size_t label)
  {
    if (label >= frame->func->labels.size())
      throw Value(Error::BadLabel);

    frame->pc = frame->func->labels.at(label);
  }

  void Thread::check_args(std::vector<Id>& types, bool vararg)
  {
    if ((args < types.size()) || (!vararg && (args > types.size())))
    {
      frame->drop_args(args);
      throw Value(Error::BadArgs);
    }

    for (size_t i = 0; i < types.size(); i++)
    {
      if (!program->typecheck(frame->arg(i).type_id(), types.at(i)))
      {
        frame->drop_args(args);
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
      if (!program->typecheck(frame->arg(i).type_id(), fields.at(i).type_id))
      {
        frame->drop_args(args);
        throw Value(Error::BadType);
      }
    }

    args = 0;
  }
}

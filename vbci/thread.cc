#include "thread.h"

#include "array.h"
#include "cown.h"
#include "object.h"
#include "program.h"

namespace vbci
{
  Value Thread::run_async(Function* func)
  {
    auto result = Cown::create(func->return_type);
    auto b = verona::rt::BehaviourCore::make(1, run_behavior, sizeof(Value));
    new (&b->get_slots()[0]) verona::rt::Slot(result);
    new (b->get_body<Value>()) Value(func);
    verona::rt::BehaviourCore::schedule_many(&b, 1);
    return Value(result, false);
  }

  void Thread::annotate(Value& v)
  {
    auto t = get();

    if (t.frame)
      v.annotate(t.frame->func, t.current_pc);
    else
      v.annotate(t.behavior, t.current_pc);
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
    auto closure = b->get_body<Value>();

    if (closure->is_function())
    {
      // This is a function pointer.
      behavior = closure->function();
    }
    else
    {
      // This is a closure, populate the self argument.
      behavior = closure->method(ApplyMethodId);

      if (closure->is_header())
      {
        auto r = closure->get_header()->region();

        // Clear the parent region, as it's no longer acquired by the behaviour.
        if (r)
          r->clear_parent();
      }

      locals.at(args++) = std::move(*closure);
    }

    // Populate cown arguments.
    auto slots = b->get_slots();
    auto num_cowns = b->get_count();
    auto result = static_cast<Cown*>(slots[0].cown());

    for (size_t i = 1; i < num_cowns; i++)
    {
      auto cown = static_cast<Cown*>(slots[i].cown());
      locals.at(args++) = Value(cown, slots[i].is_read_only());
    }

    // Execute the function.
    auto ret = thread_run(behavior);
    result->store(true, ret);
    verona::rt::BehaviourCore::finished(work);
  }

  Value Thread::thread_run(Function* func)
  {
    auto depth = frames.size();
    pushframe(func, 0, Condition::Throw);

    while (depth != frames.size())
      step();

    return std::move(locals.at(0));
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
              throw Value(Error::BadConversion);
          }
          break;
        }

        case Op::String:
        {
          auto& dst = frame->local(leb());
          dst = program->get_string(leb());
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

        case Op::New:
        {
          auto& dst = frame->local(leb());
          auto& cls = program->cls(TypeId::leb(leb()));

          if (cls.singleton)
          {
            dst = cls.singleton;
          }
          else
          {
            check_args(cls.fields);
            dst = &frame->region.object(cls)->init(frame, cls);
          }
          break;
        }

        case Op::Stack:
        {
          auto& dst = frame->local(leb());
          auto& cls = program->cls(TypeId::leb(leb()));

          if (cls.singleton)
          {
            dst = cls.singleton;
          }
          else
          {
            check_args(cls.fields);
            auto mem = stack.alloc(cls.size);
            auto obj =
              &Object::create(mem, cls, frame->frame_id)->init(frame, cls);
            frame->push_finalizer(obj);
            dst = obj;
          }
          break;
        }

        case Op::Heap:
        {
          auto& dst = frame->local(leb());
          auto region = frame->local(leb()).region();
          auto& cls = program->cls(TypeId::leb(leb()));

          if (cls.singleton)
          {
            dst = cls.singleton;
          }
          else
          {
            check_args(cls.fields);
            dst = &region->object(cls)->init(frame, cls);
          }
          break;
        }

        case Op::Region:
        {
          auto& dst = frame->local(leb());
          auto region = Region::create(leb<RegionType>());
          auto& cls = program->cls(TypeId::leb(leb()));

          if (cls.singleton)
          {
            dst = cls.singleton;
          }
          else
          {
            check_args(cls.fields);
            dst = &region->object(cls)->init(frame, cls);
          }
          break;
        }

        case Op::NewArray:
        {
          auto& dst = frame->local(leb());
          auto size = frame->local(leb()).get_size();
          auto type_id = TypeId::leb(leb());
          dst = frame->region.array(type_id, size);
          break;
        }

        case Op::NewArrayConst:
        {
          auto& dst = frame->local(leb());
          auto type_id = TypeId::leb(leb());
          auto size = leb();
          dst = frame->region.array(type_id, size);
          break;
        }

        case Op::StackArray:
        {
          auto& dst = frame->local(leb());
          auto size = frame->local(leb()).get_size();
          auto type_id = TypeId::leb(leb());
          auto rep = program->layout_type_id(type_id);
          auto mem = stack.alloc(Array::size_of(size, rep.second->size));
          dst = Array::create(
            mem, frame->frame_id, type_id, rep.first, size, rep.second->size);
          break;
        }

        case Op::StackArrayConst:
        {
          auto& dst = frame->local(leb());
          auto type_id = TypeId::leb(leb());
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
          auto size = frame->local(leb()).get_size();
          auto type_id = TypeId::leb(leb());
          dst = region->array(type_id, size);
          break;
        }

        case Op::HeapArrayConst:
        {
          auto& dst = frame->local(leb());
          auto region = frame->local(leb()).region();
          auto type_id = TypeId::leb(leb());
          auto size = leb();
          dst = region->array(type_id, size);
          break;
        }

        case Op::RegionArray:
        {
          auto& dst = frame->local(leb());
          auto region = Region::create(leb<RegionType>());
          auto size = frame->local(leb()).get_size();
          auto type_id = TypeId::leb(leb());
          dst = region->array(type_id, size);
          break;
        }

        case Op::RegionArrayConst:
        {
          auto& dst = frame->local(leb());
          auto region = Region::create(leb<RegionType>());
          auto type_id = TypeId::leb(leb());
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

        case Op::RegisterRef:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          dst = Value(src, frame->frame_id);
          break;
        }

        case Op::FieldRefMove:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          dst = src.ref(true, leb());
          break;
        }

        case Op::FieldRefCopy:
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
          dst = src.arrayref(true, idx.get_size());
          break;
        }

        case Op::ArrayRefCopy:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          auto& idx = frame->local(leb());
          dst = src.arrayref(false, idx.get_size());
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
          auto f = src.method(leb());

          if (!f)
            throw Value(Error::MethodNotFound);

          dst = f;
          break;
        }

        case Op::LookupFFI:
        {
          auto& dst = frame->local(leb());
          dst = program->symbol(leb()).raw_pointer();
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
          auto& params = symbol.params();
          auto& paramvals = symbol.paramvals();
          check_args(params, symbol.varargs());

          // A Value must be passed as a pointer, not as a struct, since it is
          // a C++ non-trivally constructed type.
          if (ffi_arg_addrs.size() < num_args)
          {
            ffi_arg_addrs.resize(num_args);
            ffi_arg_vals.resize(num_args);
          }

          for (size_t i = 0; i < num_args; i++)
          {
            auto& arg = frame->arg(i);
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
              auto rep = program->layout_type_id(arg.type_id());
              symbol.varparam(rep.second);

              if (rep.first == ValueType::Invalid)
                ffi_arg_addrs.at(i) = &ffi_arg_vals.at(i);
              else
                ffi_arg_addrs.at(i) = arg.to_ffi();
            }
          }

          auto ret = symbol.call(ffi_arg_addrs);

          if (!ret.is_error() && !(ret.type_id() < symbol.ret()))
            throw Value(Error::BadType);

          frame->local(dst) = ret;
          frame->drop_args(num_args);
          break;
        }

        case Op::When:
        {
          if (args < 1)
            throw Value(Error::BadArgs);

          auto& dst = frame->local(leb());
          auto& be = frame->arg(0);
          bool closure;
          Function* apply;

          if (be.is_function())
          {
            apply = be.function();
            closure = false;

            if (apply->param_types.size() != (args - 1))
              throw Value(Error::BadArgs);
          }
          else
          {
            if (!be.is_sendable())
              throw Value(Error::BadArgs);

            apply = be.method(ApplyMethodId);
            closure = true;

            if (!apply)
              throw Value(Error::MethodNotFound);

            if (apply->param_types.size() != args)
              throw Value(Error::BadArgs);
          }

          auto& params = apply->param_types;
          size_t num_cowns = args;
          size_t first_cown = 0;

          if (closure)
          {
            // First argument is the behavior itself.
            if (!(be.type_id() < params.at(0)))
              throw Value(Error::BadArgs);

            // First cown is at index 1. Slot 0 is the result cown.
            first_cown++;
            num_cowns--;
          }

          // Check that all other args are cowns of the right type.
          for (size_t i = first_cown; i < args; i++)
          {
            auto cown = frame->arg(i).get_cown();
            auto type_id = cown->content_type_id().make_ref();

            if (!(type_id < params.at(i)))
              throw Value(Error::BadArgs);
          }

          args = 0;

          // Create the result cown.
          auto result = Cown::create(apply->return_type);
          dst = result;

          auto b = verona::rt::BehaviourCore::make(
            num_cowns + 1, run_behavior, sizeof(Value));
          auto slots = b->get_slots();
          new (&slots[0]) verona::rt::Slot(result);

          for (size_t i = 0; i < num_cowns; i++)
          {
            // The first cown argument position depends on whether this is a
            // closure or not.
            auto arg = std::move(frame->arg(first_cown + i));

            // Offset the slot by 1 to account for the result cown.
            auto& slot = slots[i + 1];
            new (&slot) verona::rt::Slot(arg.get_cown());
            slot.set_move();

            if (arg.is_readonly())
              slot.set_read_only();
          }

          if (be.is_header())
          {
            auto r = be.get_header()->region();

            // Set the region parent, as it's captured by the behavior.
            if (r)
              r->set_parent();
          }

          auto value = new (b->get_body<Value>()) Value();
          *value = std::move(be);
          verona::rt::BehaviourCore::schedule_many(&b, 1);
          break;
        }

        case Op::Typetest:
        {
          auto& dst = frame->local(leb());
          auto& src = frame->local(leb());
          auto type_id = TypeId::leb(leb());
          dst = src.type_id() < type_id;
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
        case Op::Ptr:
          do_unop(ptr);
        case Op::Read:
          do_unop(read);

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
    if (!func)
      throw Value(Error::MethodNotFound);

    check_args(func->param_types);

    // Set how we will handle non-local returns in the current frame.
    Location frame_id = loc::Stack;
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

    // Check for stack escapes.
    auto retloc = ret.location();

    if (retloc == frame->frame_id)
    {
      // The return value can't be stack allocated in this frame.
      ret = Value(Error::BadStackEscape);
      ret.annotate(frame->func, current_pc);
      condition = Condition::Throw;
    }
    else if (
      loc::is_region(retloc) && loc::to_region(retloc)->get_frame_id(retloc) &&
      (retloc == frame->frame_id))
    {
      if (frames.size() > 1)
      {
        // Drag the frame-local allocation to the previous frame.
        auto& prev_frame = frames.at(frames.size() - 2);

        if (!drag_allocation(&prev_frame.region, ret.get_header()))
        {
          ret = Value(Error::BadStackEscape);
          ret.annotate(frame->func, current_pc);
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
          ret.annotate(frame->func, current_pc);
          condition = Condition::Throw;
          r->free_region();
        }
      }
    }

    switch (condition)
    {
      case Condition::Return:
        if (!ret.is_error() && !(ret.type_id() < frame->func->return_type))
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
    if (!func)
      throw Value(Error::MethodNotFound);

    // TODO: check for arguments that are stack or frame-local allocated.
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
    // Drop all frame registers.
    frame->drop();

    // Finalize the stack.
    for (size_t i = frame->finalize_base; i < frame->finalize_top; i++)
      finalize.at(i)->finalize();

    // Finalize the frame-local region.
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

  void Thread::check_args(std::vector<TypeId>& types, bool vararg)
  {
    if ((args < types.size()) || (!vararg && (args > types.size())))
    {
      drop_args();
      throw Value(Error::BadArgs);
    }

    for (size_t i = 0; i < types.size(); i++)
    {
      if (!(arg(i).type_id() < types.at(i)))
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
      if (!(frame->arg(i).type_id() < fields.at(i).type_id))
      {
        frame->drop_args(args);
        throw Value(Error::BadType);
      }
    }

    args = 0;
  }

  Value& Thread::arg(size_t idx)
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
        locals.at(i).drop();
    }

    args = 0;
  }
}

#include "thread.h"

#include "cown.h"
#include "object.h"
#include "program.h"

namespace vbci
{
  void Thread::step()
  {
    if (!frame)
    {
      // TODO: end thread
      // decrement our acquired cowns
      return;
    }

    auto code = program->load_code(frame->pc);
    auto op = opcode(code);

    try
    {
      switch (op)
      {
        case Op::Global:
        {
          auto& dst = frame->local(arg0(code));
          auto global_id = program->load_u32(frame->pc);

          if (global_id >= program->globals.size())
            throw Value(Error::UnknownGlobal);

          dst = program->globals.at(global_id);
          break;
        }

        case Op::NewPrimitive:
        {
          auto& dst = frame->local(arg0(code));
          ValueType t = static_cast<ValueType>(arg1(code));

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
              dst = Value(static_cast<uint8_t>(arg2(code)));
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

        case Op::NewStack:
        {
          auto& dst = frame->local(arg0(code));
          auto arg_base = arg1(code);
          auto type_id = program->load_u32(frame->pc);
          dst = alloc(type_id, frame->frame_id, arg_base);
          break;
        }

        case Op::NewHeap:
        {
          auto& dst = frame->local(arg0(code));
          auto arg_base = arg1(code);
          auto& src = frame->local(arg2(code));
          auto type_id = program->load_u32(frame->pc);
          dst = alloc(type_id, src.location(), arg_base);
          break;
        }

        case Op::NewRegion:
        {
          auto& dst = frame->local(arg0(code));
          auto arg_base = arg1(code);
          auto region = Region::create(static_cast<RegionType>(arg2(code)));
          auto type_id = program->load_u32(frame->pc);
          dst = alloc(type_id, Location(region), arg_base);
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
          auto& src = frame->local(arg1(code));
          dst = src.makeref(program, program->load_u32(frame->pc));
          break;
        }

        case Op::Load:
        {
          auto& dst = frame->local(arg0(code));
          auto& src = frame->local(arg1(code));
          dst = src.load();
          break;
        }

        case Op::Store:
        {
          auto& dst = frame->local(arg0(code));
          auto& ref = frame->local(arg1(code));
          auto& src = frame->local(arg2(code));
          dst = ref.store(src);
          break;
        }

        case Op::Lookup:
        {
          auto& dst = frame->local(arg0(code));
          auto& src = frame->local(arg1(code));
          dst = src.method(program, program->load_u32(frame->pc));
          break;
        }

        case Op::Call:
        {
          auto dst = arg0(code);
          auto arg_base = arg1(code);
          auto call_type = static_cast<CallType>(arg2(code));
          auto func_id = program->load_u32(frame->pc);

          switch (call_type)
          {
            case CallType::FunctionStatic:
              pushframe(
                program->get_function(func_id),
                dst,
                arg_base,
                Condition::Return);
              break;

            case CallType::BlockStatic:
              pushframe(
                program->get_function(func_id),
                dst,
                arg_base,
                Condition::Raise);
              break;

            case CallType::TryStatic:
              pushframe(
                program->get_function(func_id),
                dst,
                arg_base,
                Condition::Throw);
              break;

            case CallType::TailCallStatic:
              tailcall(program->get_function(func_id), arg_base);
              break;

            case CallType::FunctionDynamic:
              pushframe(
                frame->local(arg_base).method(program, func_id),
                dst,
                arg_base,
                Condition::Return);
              break;

            case CallType::BlockDynamic:
              pushframe(
                frame->local(arg_base).method(program, func_id),
                dst,
                arg_base,
                Condition::Raise);
              break;

            case CallType::TryDynamic:
              pushframe(
                frame->local(arg_base).method(program, func_id),
                dst,
                arg_base,
                Condition::Throw);
              break;

            case CallType::TailCallDynamic:
              tailcall(
                frame->local(arg_base).method(program, func_id), arg_base);
              break;

            default:
              throw Value(Error::UnknownCallType);
          }
          break;
        }

        case Op::Return:
        {
          auto ret = std::move(frame->local(arg0(code)));
          popframe(ret, static_cast<Condition>(arg1(code)));
          break;
        }

        case Op::Conditional:
        {
          // TODO: implement conditional
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
          switch (static_cast<MathOp>(code))
          {
#define do_unop(op) \
  { \
    auto& dst = frame->local(arg0(code)); \
    auto& src = frame->local(arg1(code)); \
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

#define do_const(op) \
  { \
    auto& dst = frame->local(arg0(code)); \
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
          auto& src = frame->local(arg1(code));
          auto type_id = static_cast<ValueType>(arg2(code));
          dst = src.convert(type_id);
          break;
        }

        default:
          throw Value(Error::UnknownOpcode);
      }
    }
    catch (Value& v)
    {
      popframe(v, Condition::Throw);
    }
  }

  Value Thread::alloc(TypeId type_id, Location loc, Local arg_base)
  {
    // TODO: error if loc is immortal or immutable?

    auto fields = program->types.at(type_id).fields.size();
    auto obj = Object::create(type_id, loc, fields);

    for (FieldIdx i = 0; i < fields; i++)
      obj->fields[i] = std::move(frame->local(arg_base + i));

    return Value(obj);
  }

  void Thread::pushframe(
    Function* func, Local dst, Local arg_base, Condition condition)
  {
    assert(func);

    // Set how we will handle non-local returns in the current frame.
    frame->condition = condition;

    // Make sure there's enough register space.
    auto args = func->params.size();
    auto req_stack_size = (stack.size() + 1) * registers;

    if (locals.size() < req_stack_size)
      locals.resize(req_stack_size);

    // Move arguments to the new frame.
    auto new_base = frame->base + registers;

    for (Local i = 0; i < args; i++)
    {
      auto& arg = frame->local(arg_base + i);
      auto& local = locals.at(new_base + i);
      local = std::move(arg);
    }

    // TODO: argument type checks
    stack.push_back({
      .frame_id = frame->frame_id + 2,
      .locals = locals,
      .base = frame->base + registers,
      .return_type = func->return_type,
      .pc = func->pc,
      .dst = dst,
      .condition = Condition::Return,
    });

    frame = &stack.back();
  }

  void Thread::popframe(Value& ret, Condition condition)
  {
    // The return value can't be allocated in this frame.
    if (ret.location() == frame->frame_id)
      throw Value(Error::BadReturnLocation);

    // TODO: check return type

    frame->drop();
    stack.pop_back();

    if (stack.empty())
    {
      auto prev = result->store(ret);
      prev.drop();
      frame = nullptr;
      return;
    }

    frame = &stack.back();

    switch (frame->condition)
    {
      case Condition::Return:
      {
        // Call: return (nothing), raise (pop as return), throw (pop)
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
        // BlockCall: return (nothing), raise (pop), throw (pop)
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

    frame->local(frame->dst) = ret;
    frame->condition = Condition::Return;
  }

  void Thread::tailcall(Function* func, Local arg_base)
  {
    assert(func);
    auto args = func->params.size();

    // Move args to the base.
    for (Local i = 0; i < args; i++)
      frame->local(i) = std::move(frame->local(arg_base + i));

    frame->drop(arg_base);
    frame->return_type = func->return_type;
    frame->pc = func->pc;
    frame->condition = Condition::Return;
  }
}

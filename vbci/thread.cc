#include "thread.h"

#include "array.h"
#include "cown.h"
#include "object.h"
#include "program.h"
#include <ostream>

namespace vbci
{
  Value Thread::run_async(uint32_t type_id, Function* func)
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
    return Value(result, false);
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
      locals.at(args++) = Value(cown, slots[i].is_read_only());
    }

    // Store the function return value in the result cown.
    auto ret = thread_run(behavior);
    result->store(true, ret);
    verona::rt::BehaviourCore::finished(work);
  }

  Value Thread::thread_run(Function* func)
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

    try
    {
      switch (op)
      {
        case Op::Global:
        {
          auto dst_reg = leb();
          auto global_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " global_id=" << global_id;
          auto& dst = frame->local(dst_reg);
          dst = program->global(global_id).copy();
          break;
        }

        case Op::Const:
        {
          auto dst_reg = leb();
          auto t = leb<ValueType>();
          auto& dst = frame->local(dst_reg);

          switch (t)
          {
            case ValueType::None:
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=None";
              dst = Value::none();
              break;

            case ValueType::Bool:
            {
              auto value = leb<bool>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=Bool value=" << value;
              dst = Value(value);
              break;
            }

            case ValueType::I8:
            {
              auto value = leb<int8_t>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=I8 value=" << static_cast<int>(value);
              dst = Value(value);
              break;
            }

            case ValueType::I16:
            {
              auto value = leb<int16_t>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=I16 value=" << value;
              dst = Value(value);
              break;
            }

            case ValueType::I32:
            {
              auto value = leb<int32_t>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=I32 value=" << value;
              dst = Value(value);
              break;
            }

            case ValueType::I64:
            {
              auto value = leb<int64_t>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=I64 value=" << value;
              dst = Value(value);
              break;
            }

            case ValueType::U8:
            {
              auto value = leb<uint8_t>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=U8 value=" << static_cast<unsigned>(value);
              dst = Value(value);
              break;
            }

            case ValueType::U16:
            {
              auto value = leb<uint16_t>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=U16 value=" << value;
              dst = Value(value);
              break;
            }

            case ValueType::U32:
            {
              auto value = leb<uint32_t>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=U32 value=" << value;
              dst = Value(value);
              break;
            }

            case ValueType::U64:
            {
              auto value = leb<uint64_t>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=U64 value=" << value;
              dst = Value(value);
              break;
            }

            case ValueType::ILong:
            {
              auto value = leb<int64_t>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=ILong value=" << value;
              dst = Value::from_ffi(t, value);
              break;
            }
            case ValueType::ISize:
            {
              auto value = leb<int64_t>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=ISize value=" << value;
              dst = Value::from_ffi(t, value);
              break;
            }

            case ValueType::ULong:
            {
              auto value = leb<uint64_t>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=ULong value=" << value;
              dst = Value::from_ffi(t, value);
              break;
            }
            case ValueType::USize:
            {
              auto value = leb<uint64_t>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=USize value=" << value;
              dst = Value::from_ffi(t, value);
              break;
            }

            case ValueType::F32:
            {
              auto value = leb<float>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=F32 value=" << value;
              dst = Value(value);
              break;
            }

            case ValueType::F64:
            {
              auto value = leb<double>();
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=F64 value=" << value;
              dst = Value(value);
              break;
            }

            default:
              LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=Unknown(" << static_cast<int>(t) << ")";
              throw Value(Error::BadConversion);
          }
          break;
        }

        case Op::String:
        {
          auto dst_reg = leb();
          auto string_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " string_id=" << string_id;
          auto& dst = frame->local(dst_reg);
          dst = program->get_string(string_id);
          break;
        }

        case Op::Convert:
        {
          auto dst_reg = leb();
          auto t = leb<ValueType>();
          auto src_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type=" << static_cast<int>(t) << " src=" << src_reg;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          dst = src.convert(t);
          break;
        }

        case Op::New:
        {
          auto dst_reg = leb();
          auto class_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " class_id=" << class_id;
          auto& dst = frame->local(dst_reg);
          auto& cls = program->cls(class_id);

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
          auto dst_reg = leb();
          auto class_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " class_id=" << class_id;
          auto& dst = frame->local(dst_reg);
          auto& cls = program->cls(class_id);

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
          auto dst_reg = leb();
          auto region_reg = leb();
          auto class_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " region=" << region_reg << " class_id=" << class_id;
          auto& dst = frame->local(dst_reg);
          auto region = frame->local(region_reg).region();
          auto& cls = program->cls(class_id);

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
          auto dst_reg = leb();
          auto region_type = leb<RegionType>();
          auto class_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " region_type=" << static_cast<int>(region_type) << " class_id=" << class_id;
          auto& dst = frame->local(dst_reg);
          auto& cls = program->cls(class_id);

          if (cls.singleton)
          {
            throw Value(Error::BadRegionEntryPoint);
          }

          check_args(cls.fields);
          auto region = Region::create(region_type);
          dst = &region->object(cls)->init(frame, cls);
          break;
        }

        case Op::NewArray:
        {
          auto dst_reg = leb();
          auto size_reg = leb();
          auto type_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " size_reg=" << size_reg << " type_id=" << type_id;
          auto& dst = frame->local(dst_reg);
          auto size = frame->local(size_reg).get_size();
          dst = frame->region.array(type_id, size);
          break;
        }

        case Op::NewArrayConst:
        {
          auto dst_reg = leb();
          auto type_id = leb();
          auto size = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type_id=" << type_id << " size=" << size;
          auto& dst = frame->local(dst_reg);
          dst = frame->region.array(type_id, size);
          break;
        }

        case Op::StackArray:
        {
          auto dst_reg = leb();
          auto size_reg = leb();
          auto type_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " size_reg=" << size_reg << " type_id=" << type_id;
          auto& dst = frame->local(dst_reg);
          auto size = frame->local(size_reg).get_size();
          dst = stack.array(frame->frame_id, type_id, size);
          break;
        }

        case Op::StackArrayConst:
        {
          auto dst_reg = leb();
          auto type_id = leb();
          auto size = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type_id=" << type_id << " size=" << size;
          auto& dst = frame->local(dst_reg);
          dst = stack.array(frame->frame_id, type_id, size);
          break;
        }

        case Op::HeapArray:
        {
          auto dst_reg = leb();
          auto region_reg = leb();
          auto size_reg = leb();
          auto type_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " region=" << region_reg << " size_reg=" << size_reg << " type_id=" << type_id;
          auto& dst = frame->local(dst_reg);
          auto region = frame->local(region_reg).region();
          auto size = frame->local(size_reg).get_size();
          dst = region->array(type_id, size);
          break;
        }

        case Op::HeapArrayConst:
        {
          auto dst_reg = leb();
          auto region_reg = leb();
          auto type_id = leb();
          auto size = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " region=" << region_reg << " type_id=" << type_id << " size=" << size;
          auto& dst = frame->local(dst_reg);
          auto region = frame->local(region_reg).region();
          dst = region->array(type_id, size);
          break;
        }

        case Op::RegionArray:
        {
          auto dst_reg = leb();
          auto region_type = leb<RegionType>();
          auto size_reg = leb();
          auto type_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " region_type=" << static_cast<int>(region_type) << " size_reg=" << size_reg << " type_id=" << type_id;
          auto& dst = frame->local(dst_reg);
          auto region = Region::create(region_type);
          auto size = frame->local(size_reg).get_size();
          dst = region->array(type_id, size);
          break;
        }

        case Op::RegionArrayConst:
        {
          auto dst_reg = leb();
          auto region_type = leb<RegionType>();
          auto type_id = leb();
          auto size = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " region_type=" << static_cast<int>(region_type) << " type_id=" << type_id << " size=" << size;
          auto& dst = frame->local(dst_reg);
          auto region = Region::create(region_type);
          dst = region->array(type_id, size);
          break;
        }

        case Op::Copy:
        {
          auto dst_reg = leb();
          auto src_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          dst = src.copy();
          break;
        }

        case Op::Move:
        {
          auto dst_reg = leb();
          auto src_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          dst = std::move(src);
          break;
        }

        case Op::Drop:
        {
          auto dst_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg;
          auto& dst = frame->local(dst_reg);
          dst.drop();
          break;
        }

        case Op::RegisterRef:
        {
          auto dst_reg = leb();
          auto src_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          dst = Value(src, frame->frame_id);
          break;
        }

        case Op::FieldRefMove:
        {
          auto dst_reg = leb();
          auto src_reg = leb();
          auto field_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg << " field_id=" << field_id;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          dst = src.ref(true, field_id);
          break;
        }

        case Op::FieldRefCopy:
        {
          auto dst_reg = leb();
          auto src_reg = leb();
          auto field_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg << " field_id=" << field_id;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          dst = src.ref(false, field_id);
          break;
        }

        case Op::ArrayRefMove:
        {
          auto dst_reg = leb();
          auto src_reg = leb();
          auto idx_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg << " idx=" << idx_reg;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          auto& idx = frame->local(idx_reg);
          dst = src.arrayref(true, idx.get_size());
          break;
        }

        case Op::ArrayRefCopy:
        {
          auto dst_reg = leb();
          auto src_reg = leb();
          auto idx_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg << " idx=" << idx_reg;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          auto& idx = frame->local(idx_reg);
          dst = src.arrayref(false, idx.get_size());
          break;
        }

        case Op::ArrayRefMoveConst:
        {
          auto dst_reg = leb();
          auto src_reg = leb();
          auto idx = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg << " idx=" << idx;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          dst = src.arrayref(true, idx);
          break;
        }

        case Op::ArrayRefCopyConst:
        {
          auto dst_reg = leb();
          auto src_reg = leb();
          auto idx = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg << " idx=" << idx;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          dst = src.arrayref(false, idx);
          break;
        }

        case Op::Load:
        {
          auto dst_reg = leb();
          auto src_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          dst = src.load();
          break;
        }

        case Op::StoreMove:
        {
          auto dst_reg = leb();
          auto ref_reg = leb();
          auto src_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " ref=" << ref_reg << " src=" << src_reg;
          auto& dst = frame->local(dst_reg);
          auto& ref = frame->local(ref_reg);
          auto& src = frame->local(src_reg);
          dst = ref.store(true, src);
          break;
        }

        case Op::StoreCopy:
        {
          auto dst_reg = leb();
          auto ref_reg = leb();
          auto src_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " ref=" << ref_reg << " src=" << src_reg;
          auto& dst = frame->local(dst_reg);
          auto& ref = frame->local(ref_reg);
          auto& src = frame->local(src_reg);
          dst = ref.store(false, src);
          break;
        }

        case Op::LookupStatic:
        {
          auto dst_reg = leb();
          auto func_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " func_id=" << func_id;
          auto& dst = frame->local(dst_reg);
          dst = program->function(func_id);
          break;
        }

        case Op::LookupDynamic:
        {
          auto dst_reg = leb();
          auto src_reg = leb();
          auto method_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg << " method_id=" << method_id;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          auto f = src.method(method_id);

          if (!f)
            throw Value(Error::MethodNotFound);

          dst = f;
          break;
        }

        case Op::LookupFFI:
        {
          auto dst_reg = leb();
          auto symbol_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " symbol_id=" << symbol_id;
          auto& dst = frame->local(dst_reg);
          dst = program->symbol(symbol_id).raw_pointer();
          break;
        }

        case Op::ArgMove:
        {
          auto src_reg = leb();
          LOG(Trace) << "Opcode: " << op << " src=" << src_reg << " args=" << args;
          auto& dst = frame->arg(args++);
          auto& src = frame->local(src_reg);
          dst = std::move(src);
          break;
        }

        case Op::ArgCopy:
        {
          auto src_reg = leb();
          LOG(Trace) << "Opcode: " << op << " src=" << src_reg << " args=" << args;
          auto& dst = frame->arg(args++);
          auto& src = frame->local(src_reg);
          dst = src.copy();
          break;
        }

        case Op::CallStatic:
        {
          auto dst = leb();
          auto func_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst << " func_id=" << func_id;
          auto func = program->function(func_id);
          pushframe(func, dst, CallType::Call);
          break;
        }

        case Op::CallDynamic:
        {
          auto dst = leb();
          auto func_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst << " func_reg=" << func_reg;
          auto func = frame->local(func_reg).function();
          pushframe(func, dst, CallType::Call);
          break;
        }

        case Op::SubcallStatic:
        {
          auto dst = leb();
          auto func_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst << " func_id=" << func_id;
          auto func = program->function(func_id);
          pushframe(func, dst, CallType::Subcall);
          break;
        }

        case Op::SubcallDynamic:
        {
          auto dst = leb();
          auto func_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst << " func_reg=" << func_reg;
          auto func = frame->local(func_reg).function();
          pushframe(func, dst, CallType::Subcall);
          break;
        }

        case Op::TryStatic:
        {
          auto dst = leb();
          auto func_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst << " func_id=" << func_id;
          auto func = program->function(func_id);
          pushframe(func, dst, CallType::Catch);
          break;
        }

        case Op::TryDynamic:
        {
          auto dst = leb();
          auto func_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst << " func_reg=" << func_reg;
          auto func = frame->local(func_reg).function();
          pushframe(func, dst, CallType::Catch);
          break;
        }

        case Op::FFI:
        {
          auto dst = leb();
          auto symbol_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst << " symbol_id=" << symbol_id << " num_args=" << args;
          auto num_args = args;
          auto& symbol = program->symbol(symbol_id);
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

          if (!ret.is_error() && !program->subtype(ret.type_id(), symbol.ret()))
            throw Value(Error::BadType);

          // TODO: This was implicitly a copy, I have changed to move as I think that makes sense.
          // Sylvan to review.
          frame->local(dst) = std::move(ret);
          frame->drop_args(num_args);
          break;
        }

        case Op::WhenStatic:
        {
          auto dst_reg = leb();
          auto type_id = leb();
          auto func_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type_id=" << type_id << " func_id=" << func_id;
          auto& dst = frame->local(dst_reg);
          auto func = program->function(func_id);
          queue_behavior(dst, type_id, func);
          break;
        }

        case Op::WhenDynamic:
        {
          auto dst_reg = leb();
          auto type_id = leb();
          auto func_reg = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " type_id=" << type_id << " func_reg=" << func_reg;
          auto& dst = frame->local(dst_reg);
          auto func = frame->local(func_reg).function();
          queue_behavior(dst, type_id, func);
          break;
        }

        case Op::Typetest:
        {
          auto dst_reg = leb();
          auto src_reg = leb();
          auto type_id = leb();
          LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg << " type_id=" << type_id;
          auto& dst = frame->local(dst_reg);
          auto& src = frame->local(src_reg);
          dst = program->subtype(src.type_id(), type_id);
          break;
        }

        case Op::TailcallStatic:
        {
          auto func_id = leb();
          LOG(Trace) << "Opcode: " << op << " func_id=" << func_id;
          tailcall(program->function(func_id));
          break;
        }

        case Op::TailcallDynamic:
        {
          auto func_reg = leb();
          LOG(Trace) << "Opcode: " << op << " func_reg=" << func_reg;
          tailcall(frame->local(func_reg).function());
          break;
        }

        case Op::Return:
        {
          auto src_reg = leb();
          LOG(Trace) << "Opcode: " << op << " src=" << src_reg;
          auto ret = std::move(frame->local(src_reg));
          popframe(ret, Condition::Return);
          break;
        }

        case Op::Raise:
        {
          auto src_reg = leb();
          LOG(Trace) << "Opcode: " << op << " src=" << src_reg;
          auto ret = std::move(frame->local(src_reg));
          popframe(ret, Condition::Raise);
          break;
        }

        case Op::Throw:
        {
          auto src_reg = leb();
          LOG(Trace) << "Opcode: " << op << " src=" << src_reg;
          auto ret = std::move(frame->local(src_reg));
          popframe(ret, Condition::Throw);
          break;
        }

        case Op::Cond:
        {
          auto cond_reg = leb();
          auto on_true = leb();
          auto on_false = leb();
          LOG(Trace) << "Opcode: " << op << " cond=" << cond_reg << " on_true=" << on_true << " on_false=" << on_false;
          auto& cond = frame->local(cond_reg);

          if (cond.get_bool())
            branch(on_true);
          else
            branch(on_false);
          break;
        }

        case Op::Jump:
        {
          auto target = leb();
          LOG(Trace) << "Opcode: " << op << " target=" << target;
          branch(target);
          break;
        }

#define do_binop(opname) \
  { \
    auto dst_reg = leb(); \
    auto lhs_reg = leb(); \
    auto rhs_reg = leb(); \
    LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " lhs=" << lhs_reg << " rhs=" << rhs_reg; \
    auto& dst = frame->local(dst_reg); \
    auto& lhs = frame->local(lhs_reg); \
    auto& rhs = frame->local(rhs_reg); \
    dst = lhs.op_##opname(rhs); \
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
    auto dst_reg = leb(); \
    auto src_reg = leb(); \
    LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg << " src=" << src_reg; \
    auto& dst = frame->local(dst_reg); \
    auto& src = frame->local(src_reg); \
    dst = src.op_##opname(); \
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
        case Op::Bits:
          do_unop(bits);
        case Op::Len:
          do_unop(len);
        case Op::Ptr:
          do_unop(ptr);
        case Op::Read:
          do_unop(read);

#define do_const(opname) \
  { \
    auto dst_reg = leb(); \
    LOG(Trace) << "Opcode: " << op << " dst=" << dst_reg; \
    auto& dst = frame->local(dst_reg); \
    dst = Value::opname(); \
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
      popframe(v, Condition::Throw);
    }
  }

  void Thread::pushframe(Function* func, size_t dst, CallType calltype)
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

      case CallType::Subcall:
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

  void Thread::queue_behavior(Value& result, uint32_t type_id, Function* func)
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

    result = result_cown;

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

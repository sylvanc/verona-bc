#pragma once

#include <trieste/trieste.h>

namespace vbcc
{
  using namespace trieste;
  using namespace trieste::wf::ops;

  // Definition keywords.
  inline const auto Primitive = TokenDef("primitive");
  inline const auto Class = TokenDef("class");
  inline const auto Func = TokenDef("func");
  inline const auto Lib = TokenDef("lib");
  inline const auto Type = TokenDef("type");
  inline const auto Var = TokenDef("var");

  // Identifiers.
  inline const auto SymbolId = TokenDef("symbolid", flag::print);
  inline const auto TypeId = TokenDef("typeid", flag::print);
  inline const auto GlobalId = TokenDef("globalid", flag::print);
  inline const auto ClassId = TokenDef("classid", flag::print);
  inline const auto FieldId = TokenDef("fieldid", flag::print);
  inline const auto MethodId = TokenDef("methodid", flag::print);
  inline const auto FunctionId = TokenDef("functionid", flag::print);
  inline const auto LocalId = TokenDef("localid", flag::print);
  inline const auto LabelId = TokenDef("labelid", flag::print);

  // Region types.
  inline const auto RegionRC = TokenDef("rc");
  inline const auto RegionGC = TokenDef("gc");
  inline const auto RegionArena = TokenDef("arena");

  // Types.
  inline const auto None = TokenDef("none");
  inline const auto Bool = TokenDef("bool");
  inline const auto I8 = TokenDef("i8");
  inline const auto I16 = TokenDef("i16");
  inline const auto I32 = TokenDef("i32");
  inline const auto I64 = TokenDef("i64");
  inline const auto U8 = TokenDef("u8");
  inline const auto U16 = TokenDef("u16");
  inline const auto U32 = TokenDef("u32");
  inline const auto U64 = TokenDef("u64");
  inline const auto F32 = TokenDef("f32");
  inline const auto F64 = TokenDef("f64");
  inline const auto ILong = TokenDef("ilong");
  inline const auto ULong = TokenDef("ulong");
  inline const auto ISize = TokenDef("isize");
  inline const auto USize = TokenDef("usize");
  inline const auto Ptr = TokenDef("ptr");
  inline const auto Dyn = TokenDef("dyn");
  inline const auto Array = TokenDef("array");
  inline const auto Ref = TokenDef("ref");
  inline const auto Cown = TokenDef("cown");

  // Op codes.
  inline const auto Global = TokenDef("global");
  inline const auto Const = TokenDef("const");
  inline const auto Convert = TokenDef("convert");
  inline const auto New = TokenDef("new");
  inline const auto Stack = TokenDef("stack");
  inline const auto Heap = TokenDef("heap");
  inline const auto Region = TokenDef("region");
  inline const auto NewArray = TokenDef("newarray");
  inline const auto NewArrayConst = TokenDef("newarrayconst");
  inline const auto StackArray = TokenDef("stackarray");
  inline const auto StackArrayConst = TokenDef("stackarrayconst");
  inline const auto HeapArray = TokenDef("heaparray");
  inline const auto HeapArrayConst = TokenDef("heaparrayconst");
  inline const auto RegionArray = TokenDef("regionarray");
  inline const auto RegionArrayConst = TokenDef("regionarrayconst");
  inline const auto Copy = TokenDef("copy");
  inline const auto Move = TokenDef("move");
  inline const auto Drop = TokenDef("drop");
  inline const auto RegisterRef = TokenDef("registerref");
  inline const auto FieldRef = TokenDef("fieldref");
  inline const auto ArrayRef = TokenDef("arrayref");
  inline const auto ArrayRefConst = TokenDef("arrayrefconst");
  inline const auto Load = TokenDef("load");
  inline const auto Store = TokenDef("store");
  inline const auto Lookup = TokenDef("lookup");
  inline const auto Call = TokenDef("call");
  inline const auto Subcall = TokenDef("subcall");
  inline const auto Try = TokenDef("try");
  inline const auto FFI = TokenDef("ffi");
  inline const auto When = TokenDef("when");
  inline const auto Typetest = TokenDef("typetest");

  // Terminators.
  inline const auto Tailcall = TokenDef("tailcall");
  inline const auto Return = TokenDef("ret");
  inline const auto Raise = TokenDef("raise");
  inline const auto Throw = TokenDef("throw");
  inline const auto Cond = TokenDef("cond");
  inline const auto Jump = TokenDef("jump");

  // Binary operators.
  inline const auto Add = TokenDef("add");
  inline const auto Sub = TokenDef("sub");
  inline const auto Mul = TokenDef("mul");
  inline const auto Div = TokenDef("div");
  inline const auto Mod = TokenDef("mod");
  inline const auto Pow = TokenDef("pow");
  inline const auto And = TokenDef("and");
  inline const auto Or = TokenDef("or");
  inline const auto Xor = TokenDef("xor");
  inline const auto Shl = TokenDef("shl");
  inline const auto Shr = TokenDef("shr");
  inline const auto Eq = TokenDef("eq");
  inline const auto Ne = TokenDef("ne");
  inline const auto Lt = TokenDef("lt");
  inline const auto Le = TokenDef("le");
  inline const auto Gt = TokenDef("gt");
  inline const auto Ge = TokenDef("ge");
  inline const auto Min = TokenDef("min");
  inline const auto Max = TokenDef("max");
  inline const auto LogBase = TokenDef("logbase");
  inline const auto Atan2 = TokenDef("atan2");

  // Unary operators.
  inline const auto Neg = TokenDef("neg");
  inline const auto Not = TokenDef("not");
  inline const auto Abs = TokenDef("abs");
  inline const auto Ceil = TokenDef("ceil");
  inline const auto Floor = TokenDef("floor");
  inline const auto Exp = TokenDef("exp");
  inline const auto Log = TokenDef("log");
  inline const auto Sqrt = TokenDef("sqrt");
  inline const auto Cbrt = TokenDef("cbrt");
  inline const auto IsInf = TokenDef("isinf");
  inline const auto IsNaN = TokenDef("isnan");
  inline const auto Sin = TokenDef("sin");
  inline const auto Cos = TokenDef("cos");
  inline const auto Tan = TokenDef("tan");
  inline const auto Asin = TokenDef("asin");
  inline const auto Acos = TokenDef("acos");
  inline const auto Atan = TokenDef("atan");
  inline const auto Sinh = TokenDef("sinh");
  inline const auto Cosh = TokenDef("cosh");
  inline const auto Tanh = TokenDef("tanh");
  inline const auto Asinh = TokenDef("asinh");
  inline const auto Acosh = TokenDef("acosh");
  inline const auto Atanh = TokenDef("atanh");
  inline const auto Len = TokenDef("len");
  inline const auto MakePtr = TokenDef("makeptr");
  inline const auto Read = TokenDef("read");

  // Constants.
  inline const auto Const_E = TokenDef("e");
  inline const auto Const_Pi = TokenDef("pi");
  inline const auto Const_Inf = TokenDef("inf");
  inline const auto Const_NaN = TokenDef("nan");

  // Literals.
  inline const auto True = TokenDef("true");
  inline const auto False = TokenDef("false");
  inline const auto Bin = TokenDef("bin", flag::print);
  inline const auto Oct = TokenDef("oct", flag::print);
  inline const auto Hex = TokenDef("hex", flag::print);
  inline const auto Int = TokenDef("int", flag::print);
  inline const auto Float = TokenDef("float", flag::print);
  inline const auto HexFloat = TokenDef("hexfloat", flag::print);
  inline const auto String = TokenDef("string", flag::print);

  // Structure.
  inline const auto Source = TokenDef("source");
  inline const auto Offset = TokenDef("offset");
  inline const auto Symbol = TokenDef("symbol");
  inline const auto Symbols = TokenDef("symbols");
  inline const auto Vararg = TokenDef("vararg");
  inline const auto Union = TokenDef("union");
  inline const auto Field = TokenDef("field");
  inline const auto Fields = TokenDef("fields");
  inline const auto Method = TokenDef("method");
  inline const auto Methods = TokenDef("methods");
  inline const auto Param = TokenDef("param");
  inline const auto Params = TokenDef("params");
  inline const auto FFIParams = TokenDef("ffiparams");
  inline const auto Label = TokenDef("label");
  inline const auto Labels = TokenDef("labels");
  inline const auto Arg = TokenDef("arg");
  inline const auto Args = TokenDef("args");
  inline const auto MoveArg = TokenDef("movearg");
  inline const auto MoveArgs = TokenDef("moveargs");
  inline const auto ArgMove = TokenDef("argmove");
  inline const auto ArgCopy = TokenDef("argcopy");
  inline const auto Body = TokenDef("body");
  inline const auto FnPointer = TokenDef("fnpointer");
  inline const auto CallDyn = TokenDef("calldyn");
  inline const auto SubcallDyn = TokenDef("subcalldyn");
  inline const auto TryDyn = TokenDef("trydyn");
  inline const auto TailcallDyn = TokenDef("tailcalldyn");
  inline const auto ConstStr = TokenDef("conststr");

  // Convenient names.
  inline const auto Lhs = TokenDef("lhs");
  inline const auto Rhs = TokenDef("rhs");

  inline const auto wfRegionType = RegionRC | RegionGC | RegionArena;

  inline const auto wfIntType =
    I8 | I16 | I32 | I64 | U8 | U16 | U32 | U64 | ILong | ULong | ISize | USize;
  inline const auto wfFloatType = F32 | F64;
  inline const auto wfPrimitiveType = None | Bool | wfIntType | wfFloatType;
  inline const auto wfType =
    wfPrimitiveType | Ptr | Dyn | ClassId | TypeId | Array | Ref | Cown | Union;

  inline const auto wfIntLiteral = Bin | Oct | Hex | Int;
  inline const auto wfLiteral =
    None | True | False | wfIntLiteral | Float | HexFloat;

  inline const auto wfBinop = Add | Sub | Mul | Div | Mod | Pow | And | Or |
    Xor | Shl | Shr | Eq | Ne | Lt | Le | Gt | Ge | Min | Max | LogBase | Atan2;

  inline const auto wfUnop = Neg | Not | Abs | Ceil | Floor | Exp | Log | Sqrt |
    Cbrt | IsInf | IsNaN | Sin | Cos | Tan | Asin | Acos | Atan | Sinh | Cosh |
    Tanh | Asinh | Acosh | Atanh | Len | MakePtr | Read;

  inline const auto wfConst = Const_E | Const_Pi | Const_Inf | Const_NaN;

  inline const auto wfStatement = Source | Offset | Global | Const | ConstStr |
    Convert | New | Stack | Heap | Region | NewArray | NewArrayConst |
    StackArray | StackArrayConst | HeapArray | HeapArrayConst | RegionArray |
    RegionArrayConst | Copy | Move | Drop | RegisterRef | FieldRef | ArrayRef |
    ArrayRefConst | Load | Store | Lookup | FnPointer | Arg | Call | CallDyn |
    Subcall | SubcallDyn | Try | TryDyn | FFI | When | Typetest | wfBinop |
    wfUnop | wfConst;

  inline const auto wfTerminator =
    Tailcall | TailcallDyn | Return | Raise | Throw | Cond | Jump;

  inline const auto wfDst = (LocalId >>= LocalId);
  inline const auto wfSrc = (Rhs >>= LocalId);
  inline const auto wfLhs = (Lhs >>= LocalId);
  inline const auto wfRhs = (Rhs >>= LocalId);
  inline const auto wfRgn = (Region >>= wfRegionType);
  inline const auto wfLit = (Rhs >>= wfIntLiteral);

  // Any language that can meet the wfIR definition can be compiled to byte
  // code. A trieste file with the pass name "VIR" can be passed to `vbcc` as an
  // input file. It will be checked, validated, and converted to byte code.

  // clang-format off
  inline const auto wfIR =
      (Top <<= (Primitive | Class | Type | Func | Lib)++)
    | (Array <<= (Type >>= wfType))
    | (Ref <<= (Type >>= wfType))
    | (Cown <<= (Type >>= wfType))
    | (Union <<= wfType++)
    | (Lib <<= String * Symbols)
    | (Symbols <<= Symbol++)
    | (Symbol <<=
        SymbolId * (Lhs >>= String) * (Rhs >>= String) *
        (Vararg >>= Vararg | None) * FFIParams * (Return >>= wfType))
    | (FFIParams <<= wfType++)
    | (Type <<= TypeId * (Type >>= wfType))
    | (Primitive <<= (Type >>= wfPrimitiveType) * Methods)
    | (Class <<= ClassId * Fields * Methods)
    | (Fields <<= Field++)
    | (Field <<= FieldId * (Type >>= wfType))
    | (Methods <<= Method++)
    | (Method <<= MethodId * FunctionId)
    | (Func <<= FunctionId * Params * (Type >>= wfType) * Var * Labels)
    | (Params <<= Param++)
    | (Param <<= LocalId * (Type >>= wfType))
    | (Var <<= LocalId++)
    | (Labels <<= Label++)
    | (Label <<= LabelId * Body * (Return >>= wfTerminator))
    | (Body <<= wfStatement++)
    | (Source <<= String)
    | (Offset <<= Int)
    | (Global <<= wfDst * GlobalId)
    | (Const <<= wfDst * (Type >>= wfPrimitiveType) * (Rhs >>= wfLiteral))
    | (ConstStr <<= wfDst * String)
    | (Convert <<= wfDst * (Type >>= wfPrimitiveType) * wfSrc)
    | (New <<= wfDst * ClassId * Args)
    | (Stack <<= wfDst * ClassId * Args)
    | (Heap <<= wfDst * wfSrc * ClassId * Args)
    | (Region <<= wfDst * wfRgn * ClassId * Args)
    | (NewArray <<= wfDst * (Type >>= wfType) * wfRhs)
    | (NewArrayConst <<= wfDst * (Type >>= wfType) * wfLit)
    | (StackArray <<= wfDst * (Type >>= wfType) * wfRhs)
    | (StackArrayConst <<= wfDst * (Type >>= wfType) * wfLit)
    | (HeapArray <<= wfDst * wfLhs * (Type >>= wfType) * wfRhs)
    | (HeapArrayConst <<= wfDst * wfSrc * (Type >>= wfType) * wfLit)
    | (RegionArray <<=  wfDst * wfRgn * (Type >>= wfType) * wfRhs)
    | (RegionArrayConst <<= wfDst * wfRgn * (Type >>= wfType) * wfLit)
    | (Copy <<= wfDst * wfSrc)
    | (Move <<= wfDst * wfSrc)
    | (Drop <<= LocalId)
    | (RegisterRef <<= wfDst * wfSrc)
    | (FieldRef <<= wfDst * Arg * FieldId)
    | (ArrayRef <<= wfDst * Arg * wfSrc)
    | (ArrayRefConst <<= wfDst * Arg * wfLit)
    | (Load <<= wfDst * wfSrc)
    | (Store <<= wfDst * wfSrc * Arg)
    | (Lookup <<= wfDst * wfSrc * MethodId)
    | (FnPointer <<= wfDst * (Rhs >>= FunctionId | SymbolId))
    | (Call <<= wfDst * FunctionId * Args)
    | (CallDyn <<= wfDst * wfSrc * Args)
    | (Subcall <<= wfDst * FunctionId * Args)
    | (SubcallDyn <<= wfDst * wfSrc * Args)
    | (Try <<= wfDst * FunctionId * Args)
    | (TryDyn <<= wfDst * wfSrc * Args)
    | (FFI <<= wfDst * SymbolId * Args)
    | (When <<= wfDst * Args * Arg)
    | (Typetest <<= wfDst * wfSrc * (Type >>= wfType))
    | (Args <<= Arg++)
    | (Arg <<= (Type >>= (ArgMove | ArgCopy)) * wfSrc)
    | (Tailcall <<= FunctionId * MoveArgs)
    | (TailcallDyn <<= LocalId * MoveArgs)
    | (MoveArgs <<= MoveArg++)
    | (MoveArg <<= (Type >>= ArgMove) * wfSrc)
    | (Return <<= LocalId)
    | (Raise <<= LocalId)
    | (Throw <<= LocalId)
    | (Cond <<= LocalId * (Lhs >>= LabelId) * (Rhs >>= LabelId))
    | (Jump <<= LabelId)
    | (Add <<= wfDst * wfLhs * wfRhs)
    | (Sub <<= wfDst * wfLhs * wfRhs)
    | (Mul <<= wfDst * wfLhs * wfRhs)
    | (Div <<= wfDst * wfLhs * wfRhs)
    | (Mod <<= wfDst * wfLhs * wfRhs)
    | (Pow <<= wfDst * wfLhs * wfRhs)
    | (And <<= wfDst * wfLhs * wfRhs)
    | (Or <<= wfDst * wfLhs * wfRhs)
    | (Xor <<= wfDst * wfLhs * wfRhs)
    | (Shl <<= wfDst * wfLhs * wfRhs)
    | (Shr <<= wfDst * wfLhs * wfRhs)
    | (Eq <<= wfDst * wfLhs * wfRhs)
    | (Ne <<= wfDst * wfLhs * wfRhs)
    | (Lt <<= wfDst * wfLhs * wfRhs)
    | (Le <<= wfDst * wfLhs * wfRhs)
    | (Gt <<= wfDst * wfLhs * wfRhs)
    | (Ge <<= wfDst * wfLhs * wfRhs)
    | (Min <<= wfDst * wfLhs * wfRhs)
    | (Max <<= wfDst * wfLhs * wfRhs)
    | (LogBase <<= wfDst * wfLhs * wfRhs)
    | (Atan2 <<= wfDst * wfLhs * wfRhs)
    | (Neg <<= wfDst * wfSrc)
    | (Not <<= wfDst * wfSrc)
    | (Abs <<= wfDst * wfSrc)
    | (Ceil <<= wfDst * wfSrc)
    | (Floor <<= wfDst * wfSrc)
    | (Exp <<= wfDst * wfSrc)
    | (Log <<= wfDst * wfSrc)
    | (Sqrt <<= wfDst * wfSrc)
    | (Cbrt <<= wfDst * wfSrc)
    | (IsInf <<= wfDst * wfSrc)
    | (IsNaN <<= wfDst * wfSrc)
    | (Sin <<= wfDst * wfSrc)
    | (Cos <<= wfDst * wfSrc)
    | (Tan <<= wfDst * wfSrc)
    | (Asin <<= wfDst * wfSrc)
    | (Acos <<= wfDst * wfSrc)
    | (Atan <<= wfDst * wfSrc)
    | (Sinh <<= wfDst * wfSrc)
    | (Cosh <<= wfDst * wfSrc)
    | (Tanh <<= wfDst * wfSrc)
    | (Asinh <<= wfDst * wfSrc)
    | (Acosh <<= wfDst * wfSrc)
    | (Atanh <<= wfDst * wfSrc)
    | (Len <<= wfDst * wfSrc)
    | (MakePtr <<= wfDst * wfSrc)
    | (Read <<= wfDst * wfSrc)
    | (Const_E <<= wfDst)
    | (Const_Pi <<= wfDst)
    | (Const_Inf <<= wfDst)
    | (Const_NaN <<= wfDst)
    ;
  // clang-format on
}

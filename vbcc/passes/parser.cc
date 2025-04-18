#include "../lang.h"

namespace vbcc
{
  const auto wfParserTokens = Primitive | Class | Func | GlobalId | LocalId |
    LabelId | Equals | LParen | RParen | LBracket | RBracket | Comma | Colon |
    wfRegionType | wfPrimitiveType | wfStatement | wfTerminator | wfBinop |
    wfUnop | wfConst | wfLiteral;

  // clang-format off
  const auto wfParser =
      (Top <<= (Directory | File)++)
    | (Directory <<= (Directory | File)++)
    | (File <<= Group)
    | (Group <<= wfParserTokens++)
    ;
  // clang-format on

  Parse parser()
  {
    Parse p(depth::subdirectories, wfParser);
    p.prefile([](auto&, auto& path) { return path.extension() == ".vir"; });

    p.postparse([](auto&, auto& path, auto) {
      auto& opt = options();

      if (opt.bytecode_file.empty())
        opt.bytecode_file = path.stem().replace_extension(".vbc");

      return 0;
    });

    p("start",
      {
        // Whitespace between tokens.
        "[[:space:]]+" >> [](auto&) {},

        // Definition keywords.
        "primitive\\b" >> [](auto& m) { m.add(Primitive); },
        "class\\b" >> [](auto& m) { m.add(Class); },
        "func\\b" >> [](auto& m) { m.add(Func); },

        // Region types.
        "rc\\b" >> [](auto& m) { m.add(RegionRC); },
        "gc\\b" >> [](auto& m) { m.add(RegionGC); },
        "arena\\b" >> [](auto& m) { m.add(RegionArena); },

        // Primitive types.
        "none\\b" >> [](auto& m) { m.add(None); },
        "bool\\b" >> [](auto& m) { m.add(Bool); },
        "i8\\b" >> [](auto& m) { m.add(I8); },
        "i16\\b" >> [](auto& m) { m.add(I16); },
        "i32\\b" >> [](auto& m) { m.add(I32); },
        "i64\\b" >> [](auto& m) { m.add(I64); },
        "u8\\b" >> [](auto& m) { m.add(U8); },
        "u16\\b" >> [](auto& m) { m.add(U16); },
        "u32\\b" >> [](auto& m) { m.add(U32); },
        "u64\\b" >> [](auto& m) { m.add(U64); },
        "f32\\b" >> [](auto& m) { m.add(F32); },
        "f64\\b" >> [](auto& m) { m.add(F64); },

        // Types.
        "dyn\\b" >> [](auto& m) { m.add(Dyn); },

        // Op codes.
        "global\\b" >> [](auto& m) { m.add(Global); },
        "const\\b" >> [](auto& m) { m.add(Const); },
        "convert\\b" >> [](auto& m) { m.add(Convert); },
        "stack\\b" >> [](auto& m) { m.add(Stack); },
        "heap\\b" >> [](auto& m) { m.add(Heap); },
        "region\\b" >> [](auto& m) { m.add(Region); },
        "copy\\b" >> [](auto& m) { m.add(Copy); },
        "move\\b" >> [](auto& m) { m.add(Move); },
        "drop\\b" >> [](auto& m) { m.add(Drop); },
        "ref\\b" >> [](auto& m) { m.add(Ref); },
        "load\\b" >> [](auto& m) { m.add(Load); },
        "store\\b" >> [](auto& m) { m.add(Store); },
        "lookup\\b" >> [](auto& m) { m.add(Lookup); },
        "call\\b" >> [](auto& m) { m.add(Call); },
        "subcall\\b" >> [](auto& m) { m.add(Subcall); },
        "try\\b" >> [](auto& m) { m.add(Try); },

        // Terminators.
        "tailcall\\b" >> [](auto& m) { m.add(Tailcall); },
        "ret\\b" >> [](auto& m) { m.add(Return); },
        "raise\\b" >> [](auto& m) { m.add(Raise); },
        "throw\\b" >> [](auto& m) { m.add(Throw); },
        "cond\\b" >> [](auto& m) { m.add(Cond); },
        "jump\\b" >> [](auto& m) { m.add(Jump); },

        // Binary operators.
        "add\\b" >> [](auto& m) { m.add(Add); },
        "sub\\b" >> [](auto& m) { m.add(Sub); },
        "mul\\b" >> [](auto& m) { m.add(Mul); },
        "div\\b" >> [](auto& m) { m.add(Div); },
        "mod\\b" >> [](auto& m) { m.add(Mod); },
        "pow\\b" >> [](auto& m) { m.add(Pow); },
        "and\\b" >> [](auto& m) { m.add(And); },
        "or\\b" >> [](auto& m) { m.add(Or); },
        "xor\\b" >> [](auto& m) { m.add(Xor); },
        "shl\\b" >> [](auto& m) { m.add(Shl); },
        "shr\\b" >> [](auto& m) { m.add(Shr); },
        "eq\\b" >> [](auto& m) { m.add(Eq); },
        "ne\\b" >> [](auto& m) { m.add(Ne); },
        "lt\\b" >> [](auto& m) { m.add(Lt); },
        "le\\b" >> [](auto& m) { m.add(Le); },
        "gt\\b" >> [](auto& m) { m.add(Gt); },
        "ge\\b" >> [](auto& m) { m.add(Ge); },
        "min\\b" >> [](auto& m) { m.add(Min); },
        "max\\b" >> [](auto& m) { m.add(Max); },
        "logbase\\b" >> [](auto& m) { m.add(LogBase); },
        "atan2\\b" >> [](auto& m) { m.add(Atan2); },

        // Unary operators.
        "not\\b" >> [](auto& m) { m.add(Not); },
        "neg\\b" >> [](auto& m) { m.add(Neg); },
        "abs\\b" >> [](auto& m) { m.add(Abs); },
        "ceil\\b" >> [](auto& m) { m.add(Ceil); },
        "floor\\b" >> [](auto& m) { m.add(Floor); },
        "exp\\b" >> [](auto& m) { m.add(Exp); },
        "log\\b" >> [](auto& m) { m.add(Log); },
        "sqrt\\b" >> [](auto& m) { m.add(Sqrt); },
        "cbrt\\b" >> [](auto& m) { m.add(Cbrt); },
        "isinf\\b" >> [](auto& m) { m.add(IsInf); },
        "isnan\\b" >> [](auto& m) { m.add(IsNaN); },
        "sin\\b" >> [](auto& m) { m.add(Sin); },
        "cos\\b" >> [](auto& m) { m.add(Cos); },
        "tan\\b" >> [](auto& m) { m.add(Tan); },
        "asin\\b" >> [](auto& m) { m.add(Asin); },
        "acos\\b" >> [](auto& m) { m.add(Acos); },
        "atan\\b" >> [](auto& m) { m.add(Atan); },
        "sinh\\b" >> [](auto& m) { m.add(Sinh); },
        "cosh\\b" >> [](auto& m) { m.add(Cosh); },
        "tanh\\b" >> [](auto& m) { m.add(Tanh); },
        "asinh\\b" >> [](auto& m) { m.add(Asinh); },
        "acosh\\b" >> [](auto& m) { m.add(Acosh); },
        "atanh\\b" >> [](auto& m) { m.add(Atanh); },

        // Constants.
        "e\\b" >> [](auto& m) { m.add(Const_E); },
        "pi\\b" >> [](auto& m) { m.add(Const_Pi); },
        "inf\\b" >> [](auto& m) { m.add(Const_Inf); },
        "nan\\b" >> [](auto& m) { m.add(Const_NaN); },

        // Symbols.
        "=" >> [](auto& m) { m.add(Equals); },
        "\\(" >> [](auto& m) { m.add(LParen); },
        "\\)" >> [](auto& m) { m.add(RParen); },
        "\\[" >> [](auto& m) { m.add(LBracket); },
        "\\]" >> [](auto& m) { m.add(RBracket); },
        "," >> [](auto& m) { m.add(Comma); },
        ":" >> [](auto& m) { m.add(Colon); },

        // // Identifiers.
        "\\@[_[:alnum:]]*" >> [](auto& m) { m.add(GlobalId); },
        "\\$[_[:alnum:]]*" >> [](auto& m) { m.add(LocalId); },
        "\\^[_[:alnum:]]*" >> [](auto& m) { m.add(LabelId); },

        // Bool.
        "true\\b" >> [](auto& m) { m.add(True); },
        "false\\b" >> [](auto& m) { m.add(False); },

        // Hex float.
        "[-]?0x[_[:xdigit:]]+\\.[_[:xdigit:]]+(?:p[+-][_[:digit:]]+)?\\b" >>
          [](auto& m) { m.add(HexFloat); },

        // Float.
        "[-]?[[:digit:]][_[:digit:]]*\\.[_[:digit:]]+(?:e[+-]?[_[:digit:]]+)?"
        "\\b" >>
          [](auto& m) { m.add(Float); },

        // Bin.
        "0b[_01]+\\b" >> [](auto& m) { m.add(Bin); },

        // Oct.
        "0o[_01234567]+\\b" >> [](auto& m) { m.add(Oct); },

        // Hex.
        "0x[_[:xdigit:]]+\\b" >> [](auto& m) { m.add(Hex); },

        // Int.
        "[-]?[[:digit:]][_[:digit:]]*\\b" >> [](auto& m) { m.add(Int); },

        // Line comment.
        "//[^\r\n]*" >> [](auto&) {},
      });

    return p;
  }
}

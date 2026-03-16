#include "../lang.h"

#include <map>

namespace vc
{
  struct Builtin
  {
    size_t typeargs;
    size_t args;
    Node ast;
  };

  const std::map<std::string_view, Builtin> builtins = {
    {"convi8", {0, 1, Convert << I8}},
    {"convi16", {0, 1, Convert << I16}},
    {"convi32", {0, 1, Convert << I32}},
    {"convi64", {0, 1, Convert << I64}},
    {"convu8", {0, 1, Convert << U8}},
    {"convu16", {0, 1, Convert << U16}},
    {"convu32", {0, 1, Convert << U32}},
    {"convu64", {0, 1, Convert << U64}},
    {"convilong", {0, 1, Convert << ILong}},
    {"convulong", {0, 1, Convert << ULong}},
    {"convisize", {0, 1, Convert << ISize}},
    {"convusize", {0, 1, Convert << USize}},
    {"convf32", {0, 1, Convert << F32}},
    {"convf64", {0, 1, Convert << F64}},
    {"add", {0, 2, Binop << Add}},
    {"sub", {0, 2, Binop << Sub}},
    {"mul", {0, 2, Binop << Mul}},
    {"div", {0, 2, Binop << Div}},
    {"mod", {0, 2, Binop << Mod}},
    {"pow", {0, 2, Binop << Pow}},
    {"and", {0, 2, Binop << And}},
    {"or", {0, 2, Binop << Or}},
    {"xor", {0, 2, Binop << Xor}},
    {"shl", {0, 2, Binop << Shl}},
    {"shr", {0, 2, Binop << Shr}},
    {"eq", {0, 2, Binop << Eq}},
    {"ne", {0, 2, Binop << Ne}},
    {"lt", {0, 2, Binop << Lt}},
    {"le", {0, 2, Binop << Le}},
    {"gt", {0, 2, Binop << Gt}},
    {"ge", {0, 2, Binop << Ge}},
    {"min", {0, 2, Binop << Min}},
    {"max", {0, 2, Binop << Max}},
    {"logbase", {0, 2, Binop << LogBase}},
    {"atan2", {0, 2, Binop << Atan2}},
    {"neg", {0, 1, Unop << Neg}},
    {"not", {0, 1, Unop << Not}},
    {"abs", {0, 1, Unop << Abs}},
    {"ceil", {0, 1, Unop << Ceil}},
    {"floor", {0, 1, Unop << Floor}},
    {"exp", {0, 1, Unop << Exp}},
    {"log", {0, 1, Unop << Log}},
    {"sqrt", {0, 1, Unop << Sqrt}},
    {"cbrt", {0, 1, Unop << Cbrt}},
    {"isinf", {0, 1, Unop << IsInf}},
    {"isnan", {0, 1, Unop << IsNaN}},
    {"sin", {0, 1, Unop << Sin}},
    {"cos", {0, 1, Unop << Cos}},
    {"tan", {0, 1, Unop << Tan}},
    {"asin", {0, 1, Unop << Asin}},
    {"acos", {0, 1, Unop << Acos}},
    {"atan", {0, 1, Unop << Atan}},
    {"sinh", {0, 1, Unop << Sinh}},
    {"cosh", {0, 1, Unop << Cosh}},
    {"tanh", {0, 1, Unop << Tanh}},
    {"asinh", {0, 1, Unop << Asinh}},
    {"acosh", {0, 1, Unop << Acosh}},
    {"atanh", {0, 1, Unop << Atanh}},
    {"bits", {0, 1, Unop << Bits}},
    {"len", {0, 1, Unop << Len}},
    {"ptr", {0, 1, Unop << MakePtr}},
    {"read", {0, 1, Unop << Read}},
    {"none", {0, 0, Nulop << None}},
    {"e", {0, 0, Nulop << Const_E}},
    {"pi", {0, 0, Nulop << Const_Pi}},
    {"inf", {0, 0, Nulop << Const_Inf}},
    {"nan", {0, 0, Nulop << Const_NaN}},
    {"arrayref", {0, 2, ArrayRef}},
    {"newarray", {1, 1, NewArray}},
    {"make_callback", {0, 1, MakeCallback}},
    {"callback_ptr", {0, 1, CallbackPtr}},
    {"free_callback", {0, 1, FreeCallback}},
    {"register_external_notify", {0, 1, RegisterExternalNotify}},
    {"freeze", {0, 1, Freeze}},
    {"arraycopy", {0, 5, ArrayCopy}},
    {"arrayfill", {0, 4, ArrayFill}},
    {"arraycmp", {0, 5, ArrayCompare}},
  };

  // Global builtins: available outside _builtin.
  const std::map<std::string_view, Builtin> global_builtins = {
    {"add_external", {0, 0, Nulop << AddExternal}},
    {"remove_external", {0, 0, Nulop << RemoveExternal}},
  };

  Node resolve_triplecolon(Node name, Node args)
  {
    if (name->size() != 1)
      return err(name, "Builtins and FFIs aren't namespaced");

    auto id = (name->front() / Ident)->location().view();
    auto ta = name->front() / TypeArgs;
    size_t ta_count = ta->size();
    size_t arg_count = args->size();

    auto find = builtins.find(id);
    if (find == builtins.end())
    {
      // Check global builtins (available outside _builtin).
      auto gfind = global_builtins.find(id);
      if (gfind != global_builtins.end())
      {
        if (gfind->second.typeargs != ta_count)
          return err(name, "Wrong number of type arguments");

        if (gfind->second.args != arg_count)
          return err(name, "Wrong number of arguments");

        auto r = clone(gfind->second.ast);

        for (auto& t : *ta)
          r << (Type << *t);

        return r << args;
      }

      if (ta_count != 0)
        return err(name, "FFI calls can't have type arguments");

      return FFI << (SymbolId ^ (name->front() / Ident)) << args;
    }

    // Builtin operators can only be used in the _builtin package.
    auto pkg = name->parent(ClassDef);

    while (pkg)
    {
      auto next = pkg->parent(ClassDef);

      if (!next)
        break;

      pkg = next;
    }

    if (!pkg || ((pkg / Ident)->location().view() != "_builtin"))
      return err(name, "Builtin operators can only appear in `_builtin`");

    if (find->second.typeargs != ta_count)
      return err(name, "Wrong number of type arguments");

    if (find->second.args != arg_count)
      return err(name, "Wrong number of arguments");

    auto r = clone(find->second.ast);

    for (auto& t : *ta)
      r << (Type << *t);

    return r << args;
  }

  PassDef dot()
  {
    PassDef p{
      "dot",
      wfPassDot,
      dir::topdown,
      {
        // Qualified function call followed by dot: Ns::f(args).method
        // Convert to Call before the dot rules consume it.
        In(Expr) * T(FuncName)[FuncName] * T(Tuple)[Tuple] * T(Dot) >>
          [](Match& _) {
            return Seq << (Call << _(FuncName) << (Args << *_(Tuple)))
                       << _(Dot);
          },

        // Prefix function call with parens at expression start: f(args)
        // Must be before juxtaposition rules so that the Tuple binds to
        // the FuncName rather than being consumed by ValuePat * ValuePat.
        In(Expr) * Start * T(FuncName)[FuncName] * T(Tuple)[Tuple] >>
          [](Match& _) { return Call << _(FuncName) << (Args << *_(Tuple)); },

        // Qualified zero-arg function call followed by dot: Ns::f.method
        In(Expr) * T(FuncName)[FuncName] * T(Dot)[Dot] >>
          [](Match& _) {
            return Seq << (Call << _(FuncName) << Args) << _(Dot);
          },

        // Dot with RHS tuple.
        In(Expr) * ValuePat[Lhs] * T(Dot)[Dot] * T(Tuple)[Tuple] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (_(Dot) / Ident)
                           << (_(Dot) / TypeArgs) << (Args << *_(Tuple));
          },

        // Dot with RHS value.
        In(Expr) * ValuePat[Lhs] * T(Dot)[Dot] * ValuePat[Rhs] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (_(Dot) / Ident)
                           << (_(Dot) / TypeArgs) << (Args << (Expr << _(Rhs)));
          },

        // Dot without arguments.
        In(Expr) * ValuePat[Lhs] * T(Dot)[Dot] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (_(Dot) / Ident)
                           << (_(Dot) / TypeArgs) << Args;
          },

        // TripleColon with RHS tuple.
        In(Expr) * T(TripleColon)[TripleColon] * T(Tuple)[Args] >>
          [](Match& _) -> Node {
          return resolve_triplecolon(_(TripleColon), Args << *_(Args));
        },

        // TripleColon with RHS value.
        In(Expr) * T(TripleColon)[TripleColon] * ValuePat[Rhs] >>
          [](Match& _) -> Node {
          return resolve_triplecolon(_(TripleColon), Args << (Expr << _(Rhs)));
        },

        // TripleColon without arguments.
        In(Expr) * T(TripleColon)[TripleColon] * End >> [](Match& _) -> Node {
          Node args = Args;
          return resolve_triplecolon(_(TripleColon), args);
        },

        // Application with RHS tuple.
        In(Expr) * ValuePat[Lhs] * T(Tuple)[Tuple] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (Ident ^ "apply") << TypeArgs
                           << (Args << *_(Tuple));
          },

        // Application with RHS value.
        In(Expr) * ValuePat[Lhs] * ValuePat[Rhs] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (Ident ^ "apply") << TypeArgs
                           << (Args << (Expr << _(Rhs)));
          },
      }};

    return p;
  }
}

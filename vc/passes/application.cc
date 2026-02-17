#include "../lang.h"

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
    {"convilong", {0, 1, Convert << I64}},
    {"convulong", {0, 1, Convert << U64}},
    {"convisize", {0, 1, Convert << I64}},
    {"convusize", {0, 1, Convert << U64}},
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
    {"acos", {0, 1, Unop << Acos}},
    {"asin", {0, 1, Unop << Asin}},
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
  };

  PassDef application()
  {
    PassDef p{
      "application",
      wfPassApplication,
      dir::topdown,
      {
        // FFI and builtins.
        In(Expr) * T(TripleColon)[TripleColon] * T(Tuple, ExprSeq)[Args] >>
          [](Match& _) -> Node {
          auto name = _(TripleColon);

          if (name->size() != 1)
            return err(name, "Builtins and FFIs aren't namespaced");

          auto id = (name->front() / Ident)->location().view();
          auto ta = name->front() / TypeArgs;
          auto args = Args << *_(Args);
          size_t ta_count = ta->size();
          size_t arg_count = args->size();

          auto find = builtins.find(id);
          if (find == builtins.end())
          {
            // FFI calls can't have type arguments.
            if (ta_count != 0)
              return err(name, "FFI calls can't have type arguments");

            // Emit an ffi call.
            return FFI << (SymbolId ^ (name->front() / Ident)) << args;
          }

          if (find->second.typeargs != ta_count)
            return err(name, "Wrong number of type arguments");

          if (find->second.args != arg_count)
            return err(name, "Wrong number of arguments");

          auto r = clone(find->second.ast);

          for (auto& t : *ta)
            r << (Type << *t);

          return r << args;
        },

        // Ref.
        In(Expr) * (T(Ref) << End) * ValuePat[Expr] >>
          [](Match& _) { return Ref << (Expr << _(Expr)); },

        // Hash.
        In(Expr) * (T(Hash) << End) * ValuePat[Expr] >>
          [](Match& _) { return Hash << (Expr << _(Expr)); },

        // Infix function with RHS tuple.
        In(Expr) * ValuePat[Lhs] * T(FuncName)[FuncName] * T(Tuple)[Tuple] >>
          [](Match& _) {
            return Call << _(FuncName)
                        << (Args << (Expr << _(Lhs)) << *_(Tuple));
          },

        // Infix function with RHS value.
        In(Expr) * ValuePat[Lhs] * T(FuncName)[FuncName] * ValuePat[Rhs] >>
          [](Match& _) {
            return Call << _(FuncName)
                        << (Args << (Expr << _(Lhs)) << (Expr << _(Rhs)));
          },

        // Prefix function with RHS tuple.
        In(Expr) * T(FuncName)[FuncName] * T(Tuple)[Tuple] >>
          [](Match& _) { return Call << _(FuncName) << (Args << *_(Tuple)); },

        // Prefix function with RHS value.
        In(Expr) * T(FuncName)[FuncName] * ValuePat[Rhs] >>
          [](Match& _) {
            return Call << _(FuncName) << (Args << (Expr << _(Rhs)));
          },

        // Zero-argument function.
        In(Expr) * T(FuncName)[FuncName] * End >>
          [](Match& _) { return Call << _(FuncName) << Args; },

        // Infix method with RHS tuple.
        In(Expr) * ValuePat[Lhs] * T(MethodName)[MethodName] *
            T(Tuple)[Tuple] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (_(MethodName) / Ident)
                           << (_(MethodName) / TypeArgs) << (Args << *_(Tuple));
          },

        // Infix method with RHS value.
        In(Expr) * ValuePat[Lhs] * T(MethodName)[MethodName] * ValuePat[Rhs] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Lhs)) << (_(MethodName) / Ident)
                           << (_(MethodName) / TypeArgs)
                           << (Args << (Expr << _(Rhs)));
          },

        // Prefix method with RHS tuple.
        In(Expr) * T(MethodName)[MethodName] *
            (T(Tuple) << (T(Expr)[Lhs] * (T(Expr)++)[Rhs])) >>
          [](Match& _) {
            return CallDyn << (_(Lhs)) << (_(MethodName) / Ident)
                           << (_(MethodName) / TypeArgs) << (Args << _[Rhs]);
          },

        // Prefix method with RHS value.
        In(Expr) * T(MethodName)[MethodName] * ValuePat[Rhs] >>
          [](Match& _) {
            return CallDyn << (Expr << _(Rhs)) << (_(MethodName) / Ident)
                           << (_(MethodName) / TypeArgs) << Args;
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

    p.post([](auto top) {
      top->traverse([&](auto node) {
        bool ok = true;

        if (node == Error)
        {
          ok = false;
        }
        else if (node->get_contains_error())
        {
          // Do nothing.
        }
        else if (node == Expr)
        {
          if (node->size() != 1)
          {
            node->parent()->replace(node, err(node, "Expected an expression"));
            ok = false;
          }
        }
        else if (node->in({Ref, Hash}))
        {
          if (node->empty())
          {
            node->parent()->replace(node, err(node, "Expected an expression"));
            ok = false;
          }
        }
        else if (node == MethodName)
        {
          node->parent()->replace(
            node, err(node, "Expected at least one argument to this method"));
          ok = false;
        }
        else if (node == Binop)
        {
          if ((node / Args)->size() != 2)
          {
            node->replace(
              node->front(), err(node->front(), "Expected two arguments"));
            ok = false;
          }
        }
        else if (node == Unop)
        {
          if ((node / Args)->size() != 1)
          {
            node->replace(
              node->front(), err(node->front(), "Expected one argument"));
            ok = false;
          }
        }
        else if (node == Nulop)
        {
          if ((node / Args)->size() != 0)
          {
            node->replace(
              node->front(), err(node->front(), "Expected no arguments"));
            ok = false;
          }
        }

        return ok;
      });

      return 0;
    });

    return p;
  }
}

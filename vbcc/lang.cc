#include "lang.h"

#include <CLI/CLI.hpp>

namespace vbcc
{
  Node err(Node node, const std::string& msg)
  {
    while (node->location().source->origin().empty())
    {
      auto parent = node->parent();

      if (!parent)
        break;

      node = parent;
    }

    return Error << (ErrorMsg ^ msg) << node;
  }

  ValueType val(Node ptype)
  {
    if (ptype == None)
      return ValueType::None;
    if (ptype == Bool)
      return ValueType::Bool;
    if (ptype == I8)
      return ValueType::I8;
    if (ptype == I16)
      return ValueType::I16;
    if (ptype == I32)
      return ValueType::I32;
    if (ptype == I64)
      return ValueType::I64;
    if (ptype == U8)
      return ValueType::U8;
    if (ptype == U16)
      return ValueType::U16;
    if (ptype == U32)
      return ValueType::U32;
    if (ptype == U64)
      return ValueType::U64;
    if (ptype == F32)
      return ValueType::F32;
    if (ptype == F64)
      return ValueType::F64;
    if (ptype == ILong)
      return ValueType::ILong;
    if (ptype == ULong)
      return ValueType::ULong;
    if (ptype == ISize)
      return ValueType::ISize;
    if (ptype == USize)
      return ValueType::USize;
    if (ptype == Ptr)
      return ValueType::Ptr;

    assert(false);
    return ValueType::Invalid;
  }

  static bool append_utf8(uint32_t cp, std::string& out)
  {
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
      return false;

    if (cp < 0x80)
    {
      out.push_back(char(cp));
    }
    else if (cp < 0x800)
    {
      out.push_back(char(0xC0 | (cp >> 6)));
      out.push_back(char(0x80 | (cp & 0x3F)));
    }
    else if (cp < 0x10000)
    {
      out.push_back(char(0xE0 | (cp >> 12)));
      out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(char(0x80 | (cp & 0x3F)));
    }
    else
    {
      out.push_back(char(0xF0 | (cp >> 18)));
      out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(char(0x80 | (cp & 0x3F)));
    }

    return true;
  }

  static uint32_t parse_number(
    const std::string_view& s, size_t& i, int base, size_t max_len, bool exact)
  {
    uint32_t v = 0;
    size_t len = 0;

    for (; i < s.size() && std::isxdigit(static_cast<uint8_t>(s[i])); ++i)
    {
      int d = std::isdigit(s[i]) ? s[i] - '0' : std::tolower(s[i]) - 'a' + 10;

      if (d >= base)
        break;

      v = (v * base) + d;

      if (++len == max_len)
        break;
    }

    if ((len == 0) || (exact && (len != max_len)))
      return uint32_t(-1);

    return v;
  }

  std::string unescape(const std::string_view& in)
  {
    std::string out;

    for (size_t i = 0; i < in.size(); i++)
    {
      char c = in[i];

      if (c != '\\')
      {
        out.push_back(c);
        continue;
      }

      if (++i == in.size())
        return "error: trailing backslash";

      switch (in[i])
      {
        case 'a':
          out.push_back('\a');
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'v':
          out.push_back('\v');
          break;
        case '\\':
          out.push_back('\\');
          break;
        case '\'':
          out.push_back('\'');
          break;
        case '\"':
          out.push_back('\"');
          break;
        case '?':
          out.push_back('\?');
          break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        {
          if (!append_utf8(parse_number(in, i, 8, 3, false), out))
            return "error: invalid octal escape sequence";
          break;
        }

        case 'x':
        {
          i++;
          if (!append_utf8(parse_number(in, i, 16, 0, false), out))
            return "error: invalid hex escape sequence";
          break;
        }

        case 'u':
        {
          i++;
          if (!append_utf8(parse_number(in, i, 16, 4, true), out))
            return "error: invalid unicode escape sequence";
          break;
        }

        case 'U':
        {
          i++;
          if (!append_utf8(parse_number(in, i, 16, 8, true), out))
            return "error: invalid unicode escape sequence";
          break;
        }

        default:
          return "error: unknown escape sequence";
      }
    }

    return out;
  }
}

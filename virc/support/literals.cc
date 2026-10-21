#include "literals.h"

#include <cassert>
#include <cctype>

namespace virc
{
  PrimitiveKind val(Node ptype)
  {
    if (ptype == None)
      return PrimitiveKind::None;
    if (ptype == Bool)
      return PrimitiveKind::Bool;
    if (ptype == I8)
      return PrimitiveKind::I8;
    if (ptype == I16)
      return PrimitiveKind::I16;
    if (ptype == I32)
      return PrimitiveKind::I32;
    if (ptype == I64)
      return PrimitiveKind::I64;
    if (ptype == U8)
      return PrimitiveKind::U8;
    if (ptype == U16)
      return PrimitiveKind::U16;
    if (ptype == U32)
      return PrimitiveKind::U32;
    if (ptype == U64)
      return PrimitiveKind::U64;
    if (ptype == F32)
      return PrimitiveKind::F32;
    if (ptype == F64)
      return PrimitiveKind::F64;
    if (ptype == ILong)
      return PrimitiveKind::ILong;
    if (ptype == ULong)
      return PrimitiveKind::ULong;
    if (ptype == ISize)
      return PrimitiveKind::ISize;
    if (ptype == USize)
      return PrimitiveKind::USize;
    if (ptype == Ptr)
      return PrimitiveKind::Ptr;

    assert(false);
    return PrimitiveKind::None;
  }

  Node ffi_struct_result_type()
  {
    return TupleType << USize << (Array << USize) << (Array << U8);
  }

  static bool append_utf8(uint32_t codepoint, std::string& output)
  {
    if (
      codepoint > 0x10FFFF ||
      (codepoint >= 0xD800 && codepoint <= 0xDFFF))
      return false;

    if (codepoint < 0x80)
    {
      output.push_back(char(codepoint));
    }
    else if (codepoint < 0x800)
    {
      output.push_back(char(0xC0 | (codepoint >> 6)));
      output.push_back(char(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint < 0x10000)
    {
      output.push_back(char(0xE0 | (codepoint >> 12)));
      output.push_back(char(0x80 | ((codepoint >> 6) & 0x3F)));
      output.push_back(char(0x80 | (codepoint & 0x3F)));
    }
    else
    {
      output.push_back(char(0xF0 | (codepoint >> 18)));
      output.push_back(char(0x80 | ((codepoint >> 12) & 0x3F)));
      output.push_back(char(0x80 | ((codepoint >> 6) & 0x3F)));
      output.push_back(char(0x80 | (codepoint & 0x3F)));
    }

    return true;
  }

  static uint32_t parse_number(
    const std::string_view& text,
    size_t& index,
    int base,
    size_t max_length,
    bool exact)
  {
    uint32_t value = 0;
    size_t length = 0;

    for (
      ; index < text.size() &&
      std::isxdigit(static_cast<uint8_t>(text[index]));
      ++index)
    {
      int digit = std::isdigit(text[index]) ? text[index] - '0' :
                                              std::tolower(text[index]) - 'a' + 10;

      if (digit >= base)
        break;

      value = (value * base) + digit;

      if (++length == max_length)
        break;
    }

    if ((length == 0) || (exact && (length != max_length)))
      return uint32_t(-1);

    return value;
  }

  std::string unescape(const std::string_view& input)
  {
    std::string output;

    for (size_t index = 0; index < input.size(); index++)
    {
      char character = input[index];

      if (character != '\\')
      {
        output.push_back(character);
        continue;
      }

      if (++index == input.size())
        return "error: trailing backslash";

      switch (input[index])
      {
        case 'a':
          output.push_back('\a');
          break;
        case 'b':
          output.push_back('\b');
          break;
        case 'f':
          output.push_back('\f');
          break;
        case 'n':
          output.push_back('\n');
          break;
        case 'r':
          output.push_back('\r');
          break;
        case 't':
          output.push_back('\t');
          break;
        case 'v':
          output.push_back('\v');
          break;
        case '\\':
          output.push_back('\\');
          break;
        case '\'':
          output.push_back('\'');
          break;
        case '\"':
          output.push_back('\"');
          break;
        case '?':
          output.push_back('\?');
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
          if (!append_utf8(parse_number(input, index, 8, 3, false), output))
            return "error: invalid octal escape sequence";
          break;
        }

        case 'x':
        {
          index++;
          if (!append_utf8(parse_number(input, index, 16, 0, false), output))
            return "error: invalid hex escape sequence";
          break;
        }

        case 'u':
        {
          index++;
          if (!append_utf8(parse_number(input, index, 16, 4, true), output))
            return "error: invalid unicode escape sequence";
          break;
        }

        case 'U':
        {
          index++;
          if (!append_utf8(parse_number(input, index, 16, 8, true), output))
            return "error: invalid unicode escape sequence";
          break;
        }

        default:
          return "error: unknown escape sequence";
      }
    }

    return output;
  }
}
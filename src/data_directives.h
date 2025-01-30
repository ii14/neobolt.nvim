/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf --compare-lengths --hash-function-name=is_data_directive_hash --lookup-function-name=is_data_directive_lookup --output=src/data_directives.h src/data_directives.txt  */
/* Computed positions: -k'1-2,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif


#define TOTAL_KEYWORDS 54
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 8
#define MIN_HASH_VALUE 5
#define MAX_HASH_VALUE 119
/* maximum key range = 115, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
is_data_directive_hash (register const char *str, register size_t len)
{
  static unsigned char asso_values[] =
    {
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120,   6,
       60, 120,  55, 120,   0, 120,   0, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120,  60,  40,   5,
       30,   0,  40,   5,  10,  20, 120,   0,  50, 120,
        0,  50,   1,   2, 120,   0,   0,   5,  45,  10,
       20,   2,  50, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
      120, 120, 120, 120, 120, 120
    };
  return len + asso_values[(unsigned char)str[1]] + asso_values[(unsigned char)str[0]] + asso_values[(unsigned char)str[len - 1]];
}

const char *
is_data_directive_lookup (register const char *str, register size_t len)
{
  static unsigned char lengthtable[] =
    {
       0,  0,  0,  0,  0,  4,  5,  7,  8,  0,  0,  6,  0,  0,
       0,  5,  0,  0,  0,  0,  0,  0,  0,  3,  0,  0,  6,  0,
       0,  0,  0,  0,  2,  0,  4,  4,  0,  0,  0,  4,  5,  4,
       2,  0,  4,  5,  4,  0,  0,  4,  5,  5,  0,  0,  4,  5,
       0,  7,  0,  4,  5,  0,  7,  8,  4,  5,  0,  0,  8,  4,
       5,  0,  0,  0,  4,  5,  0,  0,  3,  4,  5,  0,  0,  0,
       4,  5,  6,  0,  0,  4,  5,  0,  0,  0,  4,  5,  0,  0,
       0,  4,  5,  0,  0,  0,  4,  5,  0,  0,  0,  4,  5,  0,
       0,  0,  4,  5,  0,  0,  0,  4
    };
  static const char * wordlist[] =
    {
      "", "", "", "", "",
      "skip",
      "space",
      "string8",
      "string16",
      "", "",
      "string",
      "", "", "",
      "short",
      "", "", "", "", "", "", "",
      "int",
      "", "",
      "single",
      "", "", "", "", "",
      "ds",
      "",
      "ds.s",
      "ds.p",
      "", "", "",
      "dc.s",
      "dcb.s",
      "quad",
      "dc",
      "",
      "ds.w",
      "8byte",
      "byte",
      "", "",
      "dc.w",
      "dcb.w",
      "1byte",
      "", "",
      "ds.x",
      "hword",
      "",
      "sleb128",
      "",
      "dc.x",
      "dcb.x",
      "",
      "uleb128",
      "string64",
      "ds.d",
      "xword",
      "", "",
      "string32",
      "dc.d",
      "dcb.d",
      "", "", "",
      "ds.b",
      "dword",
      "", "",
      "dcb",
      "dc.b",
      "dcb.b",
      "", "", "",
      "ds.l",
      "ascii",
      "double",
      "", "",
      "dc.l",
      "dcb.l",
      "", "", "",
      "word",
      "float",
      "", "", "",
      "dc.a",
      "4byte",
      "", "", "",
      "zero",
      "2byte",
      "", "", "",
      "long",
      "value",
      "", "", "",
      "fill",
      "asciz",
      "", "", "",
      "octa"
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = is_data_directive_hash (str, len);

      if (key <= MAX_HASH_VALUE)
        if (len == lengthtable[key])
          {
            register const char *s = wordlist[key];

            if (*str == *s && !memcmp (str + 1, s + 1, len - 1))
              return s;
          }
    }
  return 0;
}

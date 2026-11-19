//Character classification for the 65C64, ASCII only.
#include <ctype.h>


int isdigit(int c) noexcept		{ return (c >= '0') && (c <= '9'); }
int islower(int c) noexcept		{ return (c >= 'a') && (c <= 'z'); }
int isupper(int c) noexcept		{ return (c >= 'A') && (c <= 'Z'); }
int isalpha(int c) noexcept		{ return islower(c) || isupper(c); }
int isalnum(int c) noexcept		{ return isalpha(c) || isdigit(c); }
int isxdigit(int c) noexcept		{ return isdigit(c) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F')); }
int isblank(int c) noexcept		{ return (c == ' ') || (c == '\t'); }
int isspace(int c) noexcept		{ return (c == ' ') || ((c >= '\t') && (c <= '\r')); }
int iscntrl(int c) noexcept		{ return ((c >= 0) && (c < ' ')) || (c == 127); }
int isprint(int c) noexcept		{ return (c >= ' ') && (c < 127); }
int isgraph(int c) noexcept		{ return (c > ' ') && (c < 127); }
int ispunct(int c) noexcept		{ return isgraph(c) && !isalnum(c); }
int tolower(int c) noexcept		{ return isupper(c) ? c - 'A' + 'a' : c; }
int toupper(int c) noexcept		{ return islower(c) ? c - 'a' + 'A' : c; }

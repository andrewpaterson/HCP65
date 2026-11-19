//Character classification for the 65C64, ASCII only.
#ifndef __LIBC65_CTYPE_H__
#define __LIBC65_CTYPE_H__


extern "C"
{
int		isalnum(int c) noexcept;
int		isalpha(int c) noexcept;
int		isblank(int c) noexcept;
int		iscntrl(int c) noexcept;
int		isdigit(int c) noexcept;
int		isgraph(int c) noexcept;
int		islower(int c) noexcept;
int		isprint(int c) noexcept;
int		ispunct(int c) noexcept;
int		isspace(int c) noexcept;
int		isupper(int c) noexcept;
int		isxdigit(int c) noexcept;
int		tolower(int c) noexcept;
int		toupper(int c) noexcept;
}


#endif // __LIBC65_CTYPE_H__

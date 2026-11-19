//Mathematical functions (math.cpp).
#ifndef __LIBC65_MATH_H__
#define __LIBC65_MATH_H__


#define HUGE_VAL		(__builtin_huge_val())
#define HUGE_VALF		(__builtin_huge_valf())
#define INFINITY		(__builtin_inff())
#define NAN				(__builtin_nanf(""))
#define M_PI			3.14159265358979323846


extern "C"
{
double	fabs(double x) noexcept;
float	fabsf(float x) noexcept;
double	sqrt(double x) noexcept;
float	sqrtf(float x) noexcept;
double	sin(double x) noexcept;
float	sinf(float x) noexcept;
double	cos(double x) noexcept;
float	cosf(float x) noexcept;
double	tan(double x) noexcept;
float	tanf(float x) noexcept;
double	asin(double x) noexcept;
double	acos(double x) noexcept;
double	atan(double x) noexcept;
float	atanf(float x) noexcept;
double	atan2(double y, double x) noexcept;
float	atan2f(float y, float x) noexcept;
double	exp(double x) noexcept;
float	expf(float x) noexcept;
double	log(double x) noexcept;
float	logf(float x) noexcept;
double	log10(double x) noexcept;
double	pow(double x, double y) noexcept;
float	powf(float x, float y) noexcept;
double	floor(double x) noexcept;
float	floorf(float x) noexcept;
double	ceil(double x) noexcept;
float	ceilf(float x) noexcept;
double	fmod(double x, double y) noexcept;
float	fmodf(float x, float y) noexcept;
double	modf(double x, double* pdInteger) noexcept;
double	ldexp(double x, int iExponent) noexcept;
double	frexp(double x, int* piExponent) noexcept;
double	round(double x) noexcept;
float	roundf(float x) noexcept;
long	lround(double x) noexcept;
long	lroundf(float x) noexcept;
long long	llround(double x) noexcept;
long long	llroundf(float x) noexcept;
double	trunc(double x) noexcept;
float	truncf(float x) noexcept;
double	hypot(double x, double y) noexcept;
float	hypotf(float x, float y) noexcept;
float	asinf(float x) noexcept;
float	acosf(float x) noexcept;

//long double is double.
long double	fabsl(long double x) noexcept;
long double	sqrtl(long double x) noexcept;
long double	floorl(long double x) noexcept;
long double	ceill(long double x) noexcept;
long double	truncl(long double x) noexcept;
long double	roundl(long double x) noexcept;
long double	fmodl(long double x, long double y) noexcept;
long double	powl(long double x, long double y) noexcept;
long double	logl(long double x) noexcept;
long double	log10l(long double x) noexcept;
long double	expl(long double x) noexcept;
}


//The classification macros of C are overloaded functions here (as C++'s <cmath>), defined in math.cpp (bodies
//in system headers are skipped).
bool	isnan(double x) noexcept;
bool	isnan(float x) noexcept;
bool	isinf(double x) noexcept;
bool	isinf(float x) noexcept;
bool	isfinite(double x) noexcept;
bool	isfinite(float x) noexcept;


#endif // __LIBC65_MATH_H__

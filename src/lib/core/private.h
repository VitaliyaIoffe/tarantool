/* Private header for tzdb code.  */

#ifndef PRIVATE_H

#define PRIVATE_H

/*
** This file is in the public domain, so clarified as of
** 1996-06-05 by Arthur David Olson.
*/

/*
** This header is for use ONLY with the time conversion code.
** There is no guarantee that it will remain unchanged,
** or that it will remain at all.
** Do NOT copy it to any system include directory.
** Thank you!
*/

/*
** zdump has been made independent of the rest of the time
** conversion package to increase confidence in the verification it provides.
** You can use zdump to help in verifying other implementations.
** To do this, compile with -DUSE_LTZ=0 and link without the tz library.
*/
#ifndef USE_LTZ
# define USE_LTZ 1
#endif

/* This string was in the Factory zone through version 2016f.  */
#define GRANDPARENTED	"Local time zone must be set--see zic manual page"

/*
** Defaults for preprocessor symbols.
** You can override these in your C compiler options, e.g. '-DHAVE_GETTEXT=1'.
*/

#ifndef HAVE_DECL_ASCTIME_R
#define HAVE_DECL_ASCTIME_R 1
#endif

#if !defined HAVE_GENERIC && defined __has_extension
# if __has_extension(c_generic_selections)
#  define HAVE_GENERIC 1
# else
#  define HAVE_GENERIC 0
# endif
#endif
/* _Generic is buggy in pre-4.9 GCC.  */
#if !defined HAVE_GENERIC && defined __GNUC__
# define HAVE_GENERIC (4 < __GNUC__ + (9 <= __GNUC_MINOR__))
#endif
#ifndef HAVE_GENERIC
# define HAVE_GENERIC (201112 <= __STDC_VERSION__)
#endif

#ifndef HAVE_GETTEXT
#define HAVE_GETTEXT		0
#endif /* !defined HAVE_GETTEXT */

#ifndef HAVE_INCOMPATIBLE_CTIME_R
#define HAVE_INCOMPATIBLE_CTIME_R	0
#endif

#ifndef HAVE_LINK
#define HAVE_LINK		1
#endif /* !defined HAVE_LINK */

#ifndef HAVE_POSIX_DECLS
#define HAVE_POSIX_DECLS 1
#endif

#ifndef HAVE_STDBOOL_H
#define HAVE_STDBOOL_H (199901 <= __STDC_VERSION__)
#endif

#ifndef HAVE_STRDUP
#define HAVE_STRDUP 1
#endif

#ifndef HAVE_STRTOLL
#define HAVE_STRTOLL 1
#endif

#ifndef HAVE_SYMLINK
#define HAVE_SYMLINK		1
#endif /* !defined HAVE_SYMLINK */

#ifndef HAVE_SYS_STAT_H
#define HAVE_SYS_STAT_H		1
#endif /* !defined HAVE_SYS_STAT_H */

#ifndef HAVE_SYS_WAIT_H
#define HAVE_SYS_WAIT_H		1
#endif /* !defined HAVE_SYS_WAIT_H */

#ifndef HAVE_UNISTD_H
#define HAVE_UNISTD_H		1
#endif /* !defined HAVE_UNISTD_H */

#ifndef HAVE_UTMPX_H
#define HAVE_UTMPX_H		1
#endif /* !defined HAVE_UTMPX_H */

#ifndef NETBSD_INSPIRED
# define NETBSD_INSPIRED 1
#endif

#if HAVE_INCOMPATIBLE_CTIME_R
#define asctime_r _incompatible_asctime_r
#define ctime_r _incompatible_ctime_r
#endif /* HAVE_INCOMPATIBLE_CTIME_R */

/* Enable tm_gmtoff, tm_zone, and environ on GNUish systems.  */
#define _GNU_SOURCE 1
/* Fix asctime_r on Solaris 11.  */
#define _POSIX_PTHREAD_SEMANTICS 1
/* Enable strtoimax on pre-C99 Solaris 11.  */
#define __EXTENSIONS__ 1

/* To avoid having 'stat' fail unnecessarily with errno == EOVERFLOW,
   enable large files on GNUish systems ...  */
#ifndef _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 64
#endif
/* ... and on AIX ...  */
#define _LARGE_FILES 1
/* ... and enable large inode numbers on Mac OS X 10.5 and later.  */
#define _DARWIN_USE_64_BIT_INODE 1

/*
** Nested includes
*/

/* Avoid clashes with NetBSD by renaming NetBSD's declarations.
   If defining the 'timezone' variable, avoid a clash with FreeBSD's
   'timezone' function by renaming its declaration.  */
#define localtime_rz sys_localtime_rz
#define mktime_z sys_mktime_z
#define posix2time_z sys_posix2time_z
#define time2posix_z sys_time2posix_z
#if defined USG_COMPAT && USG_COMPAT == 2
# define timezone sys_timezone
#endif
#define timezone_t sys_timezone_t
#define tzalloc sys_tzalloc
#define tzfree sys_tzfree
#include <time.h>
#undef localtime_rz
#undef mktime_z
#undef posix2time_z
#undef time2posix_z
#if defined USG_COMPAT && USG_COMPAT == 2
# undef timezone
#endif
#undef timezone_t
#undef tzalloc
#undef tzfree

#include <sys/types.h>	/* for time_t */
#include <string.h>
#include <limits.h>	/* for CHAR_BIT et al. */
#include <stdlib.h>

#include <errno.h>

#ifndef ENAMETOOLONG
# define ENAMETOOLONG EINVAL
#endif
#ifndef ENOTSUP
# define ENOTSUP EINVAL
#endif
#ifndef EOVERFLOW
# define EOVERFLOW EINVAL
#endif

#if HAVE_GETTEXT
#include <libintl.h>
#endif /* HAVE_GETTEXT */

#if HAVE_UNISTD_H
#include <unistd.h>	/* for R_OK, and other POSIX goodness */
#endif /* HAVE_UNISTD_H */

#ifndef HAVE_STRFTIME_L
# if _POSIX_VERSION < 200809
#  define HAVE_STRFTIME_L 0
# else
#  define HAVE_STRFTIME_L 1
# endif
#endif

#ifndef USG_COMPAT
# ifndef _XOPEN_VERSION
#  define USG_COMPAT 0
# else
#  define USG_COMPAT 1
# endif
#endif

#ifndef HAVE_TZNAME
# if _POSIX_VERSION < 198808 && !USG_COMPAT
#  define HAVE_TZNAME 0
# else
#  define HAVE_TZNAME 1
# endif
#endif

#ifndef ALTZONE
# if defined __sun || defined _M_XENIX
#  define ALTZONE 1
# else
#  define ALTZONE 0
# endif
#endif

#ifndef R_OK
#define R_OK	4
#endif /* !defined R_OK */

/* Unlike <ctype.h>'s isdigit, this also works if c < 0 | c > UCHAR_MAX. */
#define is_digit(c) ((unsigned)(c) - '0' <= 9)

/*
** Define HAVE_STDINT_H's default value here, rather than at the
** start, since __GLIBC__ and INTMAX_MAX's values depend on
** previously-included files.  glibc 2.1 and Solaris 10 and later have
** stdint.h, even with pre-C99 compilers.
*/
#ifndef HAVE_STDINT_H
#define HAVE_STDINT_H \
   (199901 <= __STDC_VERSION__ \
    || 2 < __GLIBC__ + (1 <= __GLIBC_MINOR__)	\
    || __CYGWIN__ || INTMAX_MAX)
#endif /* !defined HAVE_STDINT_H */

#if HAVE_STDINT_H
#include <stdint.h>
#endif /* !HAVE_STDINT_H */

#ifndef HAVE_INTTYPES_H
# define HAVE_INTTYPES_H HAVE_STDINT_H
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif

/* Pre-C99 GCC compilers define __LONG_LONG_MAX__ instead of LLONG_MAX.  */
#ifdef __LONG_LONG_MAX__
# ifndef LLONG_MAX
#  define LLONG_MAX __LONG_LONG_MAX__
# endif
# ifndef LLONG_MIN
#  define LLONG_MIN (-1 - LLONG_MAX)
# endif
#endif

#ifndef INT_FAST64_MAX
# ifdef LLONG_MAX
typedef long long	int_fast64_t;
#  define INT_FAST64_MIN LLONG_MIN
#  define INT_FAST64_MAX LLONG_MAX
# else
#  if LONG_MAX >> 31 < 0xffffffff
Please use a compiler that supports a 64-bit integer type (or wider);
you may need to compile with "-DHAVE_STDINT_H".
#  endif
typedef long		int_fast64_t;
#  define INT_FAST64_MIN LONG_MIN
#  define INT_FAST64_MAX LONG_MAX
# endif
#endif

#ifndef PRIdFAST64
# if INT_FAST64_MAX == LLONG_MAX
#  define PRIdFAST64 "lld"
# else
#  define PRIdFAST64 "ld"
# endif
#endif

#ifndef SCNdFAST64
# define SCNdFAST64 PRIdFAST64
#endif

#ifndef INT_FAST32_MAX
# if INT_MAX >> 31 == 0
typedef long int_fast32_t;
#  define INT_FAST32_MAX LONG_MAX
#  define INT_FAST32_MIN LONG_MIN
# else
typedef int int_fast32_t;
#  define INT_FAST32_MAX INT_MAX
#  define INT_FAST32_MIN INT_MIN
# endif
#endif

#ifndef INTMAX_MAX
# ifdef LLONG_MAX
typedef long long intmax_t;
#  if HAVE_STRTOLL
#   define strtoimax strtoll
#  endif
#  define INTMAX_MAX LLONG_MAX
#  define INTMAX_MIN LLONG_MIN
# else
typedef long intmax_t;
#  define INTMAX_MAX LONG_MAX
#  define INTMAX_MIN LONG_MIN
# endif
# ifndef strtoimax
#  define strtoimax strtol
# endif
#endif

#ifndef PRIdMAX
# if INTMAX_MAX == LLONG_MAX
#  define PRIdMAX "lld"
# else
#  define PRIdMAX "ld"
# endif
#endif

#ifndef UINT_FAST64_MAX
# if defined ULLONG_MAX || defined __LONG_LONG_MAX__
typedef unsigned long long uint_fast64_t;
# else
#  if ULONG_MAX >> 31 >> 1 < 0xffffffff
Please use a compiler that supports a 64-bit integer type (or wider);
you may need to compile with "-DHAVE_STDINT_H".
#  endif
typedef unsigned long	uint_fast64_t;
# endif
#endif

#ifndef UINTMAX_MAX
# if defined ULLONG_MAX || defined __LONG_LONG_MAX__
typedef unsigned long long uintmax_t;
# else
typedef unsigned long uintmax_t;
# endif
#endif

#ifndef PRIuMAX
# if defined ULLONG_MAX || defined __LONG_LONG_MAX__
#  define PRIuMAX "llu"
# else
#  define PRIuMAX "lu"
# endif
#endif

#ifndef INT32_MAX
#define INT32_MAX 0x7fffffff
#endif /* !defined INT32_MAX */
#ifndef INT32_MIN
#define INT32_MIN (-1 - INT32_MAX)
#endif /* !defined INT32_MIN */

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t) -1)
#endif

#if 3 <= __GNUC__
# define ATTRIBUTE_CONST __attribute__ ((const))
# define ATTRIBUTE_MALLOC __attribute__ ((__malloc__))
# define ATTRIBUTE_PURE __attribute__ ((__pure__))
# define ATTRIBUTE_FORMAT(spec) __attribute__ ((__format__ spec))
#else
# define ATTRIBUTE_CONST /* empty */
# define ATTRIBUTE_MALLOC /* empty */
# define ATTRIBUTE_PURE /* empty */
# define ATTRIBUTE_FORMAT(spec) /* empty */
#endif

#if !defined _Noreturn && __STDC_VERSION__ < 201112
# if 2 < __GNUC__ + (8 <= __GNUC_MINOR__)
#  define _Noreturn __attribute__ ((__noreturn__))
# else
#  define _Noreturn
# endif
#endif

#if __STDC_VERSION__ < 199901 && !defined restrict
# define restrict /* empty */
#endif

/*
** Workarounds for compilers/systems.
*/

#ifndef EPOCH_LOCAL
# define EPOCH_LOCAL 0
#endif
#ifndef EPOCH_OFFSET
# define EPOCH_OFFSET 0
#endif
#ifndef RESERVE_STD_EXT_IDS
# define RESERVE_STD_EXT_IDS 0
#endif

/* If standard C identifiers with external linkage (e.g., localtime)
   are reserved and are not already being renamed anyway, rename them
   as if compiling with '-Dtime_tz=time_t'.  */
#if !defined time_tz && RESERVE_STD_EXT_IDS && USE_LTZ
# define time_tz time_t
#endif

#ifndef HAVE_DECL_ENVIRON
# if defined environ || defined __USE_GNU
#  define HAVE_DECL_ENVIRON 1
# else
#  define HAVE_DECL_ENVIRON 0
# endif
#endif

#if !HAVE_DECL_ENVIRON
extern char **environ;
#endif

#if 2 <= HAVE_TZNAME + (TZ_TIME_T || !HAVE_POSIX_DECLS)
extern char *tzname[];
#endif
#if 2 <= USG_COMPAT + (TZ_TIME_T || !HAVE_POSIX_DECLS)
extern long timezone;
extern int daylight;
#endif
#if 2 <= ALTZONE + (TZ_TIME_T || !HAVE_POSIX_DECLS)
extern long altzone;
#endif


#define NO_TM_ZONE 1

/* Infer TM_ZONE on systems where this information is known, but suppress
   guessing if NO_TM_ZONE is defined.  Similarly for TM_GMTOFF.  */
#if (defined __GLIBC__ \
     || defined __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__ \
     || (defined __APPLE__ && defined __MACH__))
# if !defined TM_GMTOFF && !defined NO_TM_GMTOFF
#  define TM_GMTOFF tm_gmtoff
# endif
# if !defined TM_ZONE && !defined NO_TM_ZONE
#  define TM_ZONE tm_zone
# endif
#endif

/*
** Finally, some convenience items.
*/

#if HAVE_STDBOOL_H
# include <stdbool.h>
#else
# define true 1
# define false 0
# define bool int
#endif

#define TYPE_BIT(type)	(sizeof (type) * CHAR_BIT)
#define TYPE_SIGNED(type) (((type) -1) < 0)
#define TWOS_COMPLEMENT(t) ((t) ~ (t) 0 < 0)

/* Max and min values of the integer type T, of which only the bottom
   B bits are used, and where the highest-order used bit is considered
   to be a sign bit if T is signed.  */
#define MAXVAL(t, b)						\
  ((t) (((t) 1 << ((b) - 1 - TYPE_SIGNED(t)))			\
	- 1 + ((t) 1 << ((b) - 1 - TYPE_SIGNED(t)))))
#define MINVAL(t, b)						\
  ((t) (TYPE_SIGNED(t) ? - TWOS_COMPLEMENT(t) - MAXVAL(t, b) : 0))

/* The extreme time values, assuming no padding.  */
#define TIME_T_MIN_NO_PADDING MINVAL(time_t, TYPE_BIT(time_t))
#define TIME_T_MAX_NO_PADDING MAXVAL(time_t, TYPE_BIT(time_t))

/* The extreme time values.  These are macros, not constants, so that
   any portability problem occur only when compiling .c files that use
   the macros, which is safer for applications that need only zdump and zic.
   This implementation assumes no padding if time_t is signed and
   either the compiler lacks support for _Generic or time_t is not one
   of the standard signed integer types.  */
#if HAVE_GENERIC
# define TIME_T_MIN \
    _Generic((time_t) 0, \
	     signed char: SCHAR_MIN, short: SHRT_MIN, \
	     int: INT_MIN, long: LONG_MIN, long long: LLONG_MIN, \
	     default: TIME_T_MIN_NO_PADDING)
# define TIME_T_MAX \
    (TYPE_SIGNED(time_t) \
     ? _Generic((time_t) 0, \
		signed char: SCHAR_MAX, short: SHRT_MAX, \
		int: INT_MAX, long: LONG_MAX, long long: LLONG_MAX, \
		default: TIME_T_MAX_NO_PADDING)			    \
     : (time_t) -1)
#else
# define TIME_T_MIN TIME_T_MIN_NO_PADDING
# define TIME_T_MAX TIME_T_MAX_NO_PADDING
#endif

/*
** 302 / 1000 is log10(2.0) rounded up.
** Subtract one for the sign bit if the type is signed;
** add one for integer division truncation;
** add one more for a minus sign if the type is signed.
*/
#define INT_STRLEN_MAXIMUM(type) \
	((TYPE_BIT(type) - TYPE_SIGNED(type)) * 302 / 1000 + \
	1 + TYPE_SIGNED(type))

/*
** INITIALIZE(x)
*/

#ifdef GCC_LINT
# define INITIALIZE(x)	((x) = 0)
#else
# define INITIALIZE(x)
#endif

#ifndef UNINIT_TRAP
# define UNINIT_TRAP 0
#endif

/*
** For the benefit of GNU folk...
** '_(MSGID)' uses the current locale's message library string for MSGID.
** The default is to use gettext if available, and use MSGID otherwise.
*/

#if HAVE_GETTEXT
#define _(msgid) gettext(msgid)
#else /* !HAVE_GETTEXT */
#define _(msgid) msgid
#endif /* !HAVE_GETTEXT */

#if !defined TZ_DOMAIN && defined HAVE_GETTEXT
# define TZ_DOMAIN "tz"
#endif

/* Handy macros that are independent of tzfile implementation.  */

#define YEARSPERREPEAT		400	/* years before a Gregorian repeat */

#define SECSPERMIN	60
#define MINSPERHOUR	60
#define HOURSPERDAY	24
#define DAYSPERWEEK	7
#define DAYSPERNYEAR	365
#define DAYSPERLYEAR	366
#define SECSPERHOUR	(SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY	((int_fast32_t) SECSPERHOUR * HOURSPERDAY)
#define MONSPERYEAR	12

#define TM_SUNDAY	0
#define TM_MONDAY	1
#define TM_TUESDAY	2
#define TM_WEDNESDAY	3
#define TM_THURSDAY	4
#define TM_FRIDAY	5
#define TM_SATURDAY	6

#define TM_JANUARY	0
#define TM_FEBRUARY	1
#define TM_MARCH	2
#define TM_APRIL	3
#define TM_MAY		4
#define TM_JUNE		5
#define TM_JULY		6
#define TM_AUGUST	7
#define TM_SEPTEMBER	8
#define TM_OCTOBER	9
#define TM_NOVEMBER	10
#define TM_DECEMBER	11

#define TM_YEAR_BASE	1900

#define EPOCH_YEAR	1970
#define EPOCH_WDAY	TM_THURSDAY

#define isleap(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))

/*
** Since everything in isleap is modulo 400 (or a factor of 400), we know that
**	isleap(y) == isleap(y % 400)
** and so
**	isleap(a + b) == isleap((a + b) % 400)
** or
**	isleap(a + b) == isleap(a % 400 + b % 400)
** This is true even if % means modulo rather than Fortran remainder
** (which is allowed by C89 but not by C99 or later).
** We use this to avoid addition overflow problems.
*/

#define isleap_sum(a, b)	isleap((a) % 400 + (b) % 400)


/*
** The Gregorian year averages 365.2425 days, which is 31556952 seconds.
*/

#define AVGSECSPERYEAR		31556952L
#define SECSPERREPEAT \
  ((int_fast64_t) YEARSPERREPEAT * (int_fast64_t) AVGSECSPERYEAR)
#define SECSPERREPEAT_BITS	34	/* ceil(log2(SECSPERREPEAT)) */

#endif /* !defined PRIVATE_H */

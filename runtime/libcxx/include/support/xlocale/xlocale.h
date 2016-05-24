// -*- C++ -*-
//===------------------- support/xlocale/xlocale.h ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This is a shared implementation of a shim to provide extended locale support
// on top of libc's that don't support it (like Android's bionic, and Newlib).
//
// The 'illusion' only works when the specified locale is "C" or "POSIX", but
// that's about as good as we can do without implementing full xlocale support
// in the underlying libc.
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP_SUPPORT_XLOCALE_XLOCALE_H
#define _LIBCPP_SUPPORT_XLOCALE_XLOCALE_H

#include <stdlib.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _PDCLIB_LOCALE_H
#define LC_COLLATE_MASK  (1<<0)
#define LC_CTYPE_MASK    (1<<1)
#define LC_MESSAGES_MASK (1<<2)
#define LC_MONETARY_MASK (1<<3)
#define LC_NUMERIC_MASK  (1<<4)
#define LC_TIME_MASK     (1<<5)
#define LC_ALL_MASK      (LC_COLLATE_MASK | LC_CTYPE_MASK | LC_MESSAGES_MASK | \
                                  LC_MONETARY_MASK | LC_NUMERIC_MASK | LC_TIME_MASK)
#define LC_GLOBAL_LOCALE ((locale_t)-1)
#else
// This is missing in PDClib
#define LC_MESSAGES_MASK     (LC_TIME_MASK << 1)
#undef LC_ALL_MASK
#define LC_ALL_MASK      (LC_COLLATE_MASK | LC_CTYPE_MASK | LC_MESSAGES_MASK | \
                                  LC_MONETARY_MASK | LC_NUMERIC_MASK | LC_TIME_MASK)
#endif
#define MB_CUR_MAX_L(l) MB_CUR_MAX

#ifndef _PDCLIB_LOCALE_H
typedef struct _xlocale *locale_t;

static inline locale_t         duplocale(locale_t base) { return base; }
static inline int              freelocale(locale_t loc) { return 0; }
static inline locale_t         newlocale(int mask, const char *locale, locale_t base) { return base; }
static inline const char      *querylocale(int mask, locale_t loc) { return "C"; }
static inline locale_t         uselocale(locale_t loc) { return LC_GLOBAL_LOCALE; }
#endif

static inline int vsnprintf_l(char *__s, size_t __n, locale_t __l, const char *__format, va_list __va) {
    return vsnprintf(__s, __n, __format, __va);
}

static inline int snprintf_l(char *__s, size_t __n, locale_t __l, const char *__format, ...)
{
  va_list __va;
  va_start(__va, __format);
  int __res = vsnprintf_l(__s, __n , __l, __format, __va);
  va_end(__va);
  return __res;
}

static inline int asprintf_l(char **__s, locale_t __l, const char *__format, ...) {
  va_list __va;
  va_start(__va, __format);
  // FIXME:
  int __res = vasprintf(__s, __format, __va);
  va_end(__va);
  return __res;
}

static inline int sscanf_l(const char *__s, locale_t __l, const char *__format, ...) {
  va_list __va;
  va_start(__va, __format);
  // FIXME:
  int __res = vsscanf(__s, __format, __va);
  va_end(__va);
  return __res;
}

static inline int isalnum_l(int c, locale_t) {
  return isalnum(c);
}

static inline int isalpha_l(int c, locale_t) {
  return isalpha(c);
}

static inline int isblank_l(int c, locale_t) {
  return isblank(c);
}

static inline int iscntrl_l(int c, locale_t) {
  return iscntrl(c);
}

static inline int isdigit_l(int c, locale_t) {
  return isdigit(c);
}

static inline int isgraph_l(int c, locale_t) {
  return isgraph(c);
}

static inline int islower_l(int c, locale_t) {
  return islower(c);
}

static inline int isprint_l(int c, locale_t) {
  return isprint(c);
}

static inline int ispunct_l(int c, locale_t) {
  return ispunct(c);
}

static inline int isspace_l(int c, locale_t) {
  return isspace(c);
}

static inline int isupper_l(int c, locale_t) {
  return isupper(c);
}

static inline int isxdigit_l(int c, locale_t) {
  return isxdigit(c);
}

static inline int iswalnum_l(wint_t c, locale_t) {
  return iswalnum(c);
}

static inline int iswalpha_l(wint_t c, locale_t) {
  return iswalpha(c);
}

static inline int iswblank_l(wint_t c, locale_t) {
  return iswblank(c);
}

static inline int iswcntrl_l(wint_t c, locale_t) {
  return iswcntrl(c);
}

static inline int iswdigit_l(wint_t c, locale_t) {
  return iswdigit(c);
}

static inline int iswgraph_l(wint_t c, locale_t) {
  return iswgraph(c);
}

static inline int iswlower_l(wint_t c, locale_t) {
  return iswlower(c);
}

static inline int iswprint_l(wint_t c, locale_t) {
  return iswprint(c);
}

static inline int iswpunct_l(wint_t c, locale_t) {
  return iswpunct(c);
}

static inline int iswspace_l(wint_t c, locale_t) {
  return iswspace(c);
}

static inline int iswupper_l(wint_t c, locale_t) {
  return iswupper(c);
}

static inline int iswxdigit_l(wint_t c, locale_t) {
  return iswxdigit(c);
}

static inline int iswctype_l(wint_t c, wctype_t cls, locale_t) {
  return iswctype(c, cls);
}

static inline int toupper_l(int c, locale_t) {
  return toupper(c);
}

static inline int tolower_l(int c, locale_t) {
  return tolower(c);
}

static inline int towupper_l(int c, locale_t) {
  return towupper(c);
}

static inline int towlower_l(int c, locale_t) {
  return towlower(c);
}

static inline int strcoll_l(const char *s1, const char *s2, locale_t) {
  return strcoll(s1, s2);
}

static inline size_t strxfrm_l(char *dest, const char *src, size_t n,
                               locale_t) {
  return strxfrm(dest, src, n);
}

static inline size_t strftime_l(char *s, size_t max, const char *format,
                                const struct tm *tm, locale_t) {
  return strftime(s, max, format, tm);
}

static inline int wcscoll_l(const wchar_t *ws1, const wchar_t *ws2, locale_t) {
  return wcscoll(ws1, ws2);
}

static inline size_t wcsxfrm_l(wchar_t *dest, const wchar_t *src, size_t n,
                               locale_t) {
  return wcsxfrm(dest, src, n);
}

static inline long double strtold_l(const char *nptr, char **endptr, locale_t) {
  return strtold(nptr, endptr);
}

static inline long long strtoll_l(const char *nptr, char **endptr, int base,
                                  locale_t) {
  return strtoll(nptr, endptr, base);
}

static inline unsigned long long strtoull_l(const char *nptr, char **endptr,
                                            int base, locale_t) {
  return strtoull(nptr, endptr, base);
}

static inline long long wcstoll_l(const wchar_t *nptr, wchar_t **endptr,
                                  int base, locale_t) {
  return wcstoll(nptr, endptr, base);
}

static inline unsigned long long wcstoull_l(const wchar_t *nptr,
                                            wchar_t **endptr, int base,
                                            locale_t) {
  return wcstoull(nptr, endptr, base);
}

static inline long double wcstold_l(const wchar_t *nptr, wchar_t **endptr,
                                    locale_t) {
  return wcstold(nptr, endptr);
}

static inline wint_t btowc_l(int c, locale_t) {
    return btowc(c);
}

static inline size_t mbsrtowcs_l(wchar_t *dst, const char **src, size_t len, mbstate_t *ps, locale_t) {
    return mbsrtowcs(dst, src, len, ps);
}

static inline struct lconv *localeconv_l(locale_t) {
    return localeconv();
}

static inline size_t wcrtomb_l(char *s, wchar_t wc, mbstate_t *ps, locale_t) {
    return wcrtomb(s, wc, ps);
}

static inline size_t mbrlen_l(const char *s, size_t n, mbstate_t *ps, locale_t) {
    return mbrlen(s, n, ps);
}

static inline size_t mbrtowc_l(wchar_t *wc, const char *s, size_t n, mbstate_t *mbs, locale_t) {
    return mbrtowc(wc, s, n, mbs);
}

static inline int wctob_l(wint_t wc, locale_t) {
    return wctob(wc);
}

static inline int mbtowc_l(wchar_t *pwc, const char *s, size_t n, locale_t) {
    return mbtowc(pwc, s, n);
}

static inline size_t mbsnrtowcs_l(wchar_t *dst, const char **src, size_t nmc, size_t len, mbstate_t *ps, locale_t) {
    return mbsnrtowcs(dst, src, nmc, len, ps);
}

static inline size_t wcsrtombs_l(char *dst, const wchar_t **src, size_t len, mbstate_t *ps, locale_t) {
    return wcsrtombs(dst, src, len, ps);
}

static inline size_t wcsnrtombs_l(char *dst, const wchar_t **src, size_t nwc, size_t len, mbstate_t *ps, locale_t) {
    return wcsnrtombs(dst, src, nwc, len, ps);
}

static inline
long strtol_l(const char *__nptr, char **__endptr,
    int __base, locale_t __loc) {
  return strtol(__nptr, __endptr, __base);
}
static inline
unsigned long strtoul_l(const char *__nptr, char **__endptr,
    int __base, locale_t __loc) {
  return strtoul(__nptr, __endptr, __base);
}
#ifdef __cplusplus
}
#endif

#endif // _LIBCPP_SUPPORT_XLOCALE_XLOCALE_H

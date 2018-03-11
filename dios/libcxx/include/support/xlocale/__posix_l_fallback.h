// -*- C++ -*-
//===--------------- support/xlocale/__posix_l_fallback.h -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// These are reimplementations of some extended locale functions ( *_l ) that
// are normally part of POSIX.  This shared implementation provides parts of the
// extended locale support for libc's that normally don't have any (like
// Android's bionic and Newlib).
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP_SUPPORT_XLOCALE_POSIX_L_FALLBACK_H
#define _LIBCPP_SUPPORT_XLOCALE_POSIX_L_FALLBACK_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_CUR_MAX_L(l) MB_CUR_MAX

inline _LIBCPP_ALWAYS_INLINE int isalnum_l(int c, locale_t) {
  return ::isalnum(c);
}

inline _LIBCPP_ALWAYS_INLINE int isalpha_l(int c, locale_t) {
  return ::isalpha(c);
}

inline _LIBCPP_ALWAYS_INLINE int isblank_l(int c, locale_t) {
  return ::isblank(c);
}

inline _LIBCPP_ALWAYS_INLINE int iscntrl_l(int c, locale_t) {
  return ::iscntrl(c);
}

inline _LIBCPP_ALWAYS_INLINE int isdigit_l(int c, locale_t) {
  return ::isdigit(c);
}

inline _LIBCPP_ALWAYS_INLINE int isgraph_l(int c, locale_t) {
  return ::isgraph(c);
}

inline _LIBCPP_ALWAYS_INLINE int islower_l(int c, locale_t) {
  return ::islower(c);
}

inline _LIBCPP_ALWAYS_INLINE int isprint_l(int c, locale_t) {
  return ::isprint(c);
}

inline _LIBCPP_ALWAYS_INLINE int ispunct_l(int c, locale_t) {
  return ::ispunct(c);
}

inline _LIBCPP_ALWAYS_INLINE int isspace_l(int c, locale_t) {
  return ::isspace(c);
}

inline _LIBCPP_ALWAYS_INLINE int isupper_l(int c, locale_t) {
  return ::isupper(c);
}

inline _LIBCPP_ALWAYS_INLINE int isxdigit_l(int c, locale_t) {
  return ::isxdigit(c);
}

inline _LIBCPP_ALWAYS_INLINE int iswalnum_l(wint_t c, locale_t) {
  return ::iswalnum(c);
}

inline _LIBCPP_ALWAYS_INLINE int iswalpha_l(wint_t c, locale_t) {
  return ::iswalpha(c);
}

inline _LIBCPP_ALWAYS_INLINE int iswblank_l(wint_t c, locale_t) {
  return ::iswblank(c);
}

inline _LIBCPP_ALWAYS_INLINE int iswcntrl_l(wint_t c, locale_t) {
  return ::iswcntrl(c);
}

inline _LIBCPP_ALWAYS_INLINE int iswdigit_l(wint_t c, locale_t) {
  return ::iswdigit(c);
}

inline _LIBCPP_ALWAYS_INLINE int iswgraph_l(wint_t c, locale_t) {
  return ::iswgraph(c);
}

inline _LIBCPP_ALWAYS_INLINE int iswlower_l(wint_t c, locale_t) {
  return ::iswlower(c);
}

inline _LIBCPP_ALWAYS_INLINE int iswprint_l(wint_t c, locale_t) {
  return ::iswprint(c);
}

inline _LIBCPP_ALWAYS_INLINE int iswpunct_l(wint_t c, locale_t) {
  return ::iswpunct(c);
}

inline _LIBCPP_ALWAYS_INLINE int iswspace_l(wint_t c, locale_t) {
  return ::iswspace(c);
}

inline _LIBCPP_ALWAYS_INLINE int iswupper_l(wint_t c, locale_t) {
  return ::iswupper(c);
}

inline _LIBCPP_ALWAYS_INLINE int iswxdigit_l(wint_t c, locale_t) {
  return ::iswxdigit(c);
}

inline _LIBCPP_ALWAYS_INLINE int toupper_l(int c, locale_t) {
  return ::toupper(c);
}

inline _LIBCPP_ALWAYS_INLINE int tolower_l(int c, locale_t) {
  return ::tolower(c);
}

inline _LIBCPP_ALWAYS_INLINE wint_t towupper_l(wint_t c, locale_t) {
  return ::towupper(c);
}

inline _LIBCPP_ALWAYS_INLINE wint_t towlower_l(wint_t c, locale_t) {
  return ::towlower(c);
}

inline _LIBCPP_ALWAYS_INLINE int strcoll_l(const char *s1, const char *s2,
                                           locale_t) {
  return ::strcoll(s1, s2);
}

inline _LIBCPP_ALWAYS_INLINE size_t strxfrm_l(char *dest, const char *src,
                                              size_t n, locale_t) {
  return ::strxfrm(dest, src, n);
}

inline _LIBCPP_ALWAYS_INLINE size_t strftime_l(char *s, size_t max,
                                               const char *format,
                                               const struct tm *tm, locale_t) {
  return ::strftime(s, max, format, tm);
}

inline _LIBCPP_ALWAYS_INLINE int wcscoll_l(const wchar_t *ws1,
                                           const wchar_t *ws2, locale_t) {
  return ::wcscoll(ws1, ws2);
}

inline _LIBCPP_ALWAYS_INLINE size_t wcsxfrm_l(wchar_t *dest, const wchar_t *src,
                                              size_t n, locale_t) {
  return ::wcsxfrm(dest, src, n);
}

static inline int vsnprintf_l(char *__s, size_t __n, locale_t __l, const char *__format, va_list __va)
{
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

#endif // _LIBCPP_SUPPORT_XLOCALE_POSIX_L_FALLBACK_H

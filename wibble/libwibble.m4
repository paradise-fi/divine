# LIBWIBBLE_DEFS([LIBWIBBLE_REQS=libwibble])
# ---------------------------------------
AC_DEFUN([LIBWIBBLE_DEFS],
[
	dnl Import libtagcoll data
	PKG_CHECK_MODULES(LIBWIBBLE,m4_default([$1], libwibble))
	AC_SUBST(LIBWIBBLE_CFLAGS)
	AC_SUBST(LIBWIBBLE_LIBS)
])

AC_DEFUN([LIBWIBBLEGC_DEFS],
[
	dnl Import libtagcoll data
	PKG_CHECK_MODULES(LIBWIBBLE,m4_default([$1], libwibble-gc))
	AC_SUBST(LIBWIBBLE_CFLAGS)
	AC_SUBST(LIBWIBBLE_LIBS)
])

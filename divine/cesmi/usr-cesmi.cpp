#ifdef _WIN32
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // for vsnprintf
#include "cesmi.h"

int vasprintf(char **result, const char *format, va_list aq)
{
	int i, n, size = 16;
	va_list ap;
	char *buf = (char *) malloc(size);

	*result = 0;

	if (!buf)
		return -1;

	for (i = 0;; i++) {
		va_copy(ap, aq);
		n = vsnprintf(buf, size, format, ap);
		va_end(ap);

		if (0 <= n && n < size)
			break;

		free(buf);
		/* Up to glibc 2.0.6 returns -1 */
		size = (n < 0) ? size * 2 : n + 1;
		if (!(buf = (char *) malloc(size)))
			return -1;
	}

	if (i > 1) {
		/* Reallocating more then once? */
		if (!(*result = strdup(buf))) {
			free(buf);
			return -1;
		}
		free(buf);
	} else
		*result = buf;

	return n + 1;
}

int asprintf(char **result, const char *format, ...)
{
	int r;
	va_list ap;
	va_start(ap, format);
	r = vasprintf(result, format, ap);
	va_end(ap);
	return r;
}

#endif

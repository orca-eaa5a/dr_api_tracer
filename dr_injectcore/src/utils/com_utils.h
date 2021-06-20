#pragma once

#ifdef DEBUG
# define IF_DEBUG(x) x
# define IF_DEBUG_ELSE(x,y) x
# define _IF_DEBUG(x) , x
#else
# define IF_DEBUG(x)
# define IF_DEBUG_ELSE(x,y) y
# define _IF_DEBUG(x)
#endif

inline int fast_strcmp(char *s1, size_t s1_len, char *s2, size_t s2_len) {
	if (s1_len != s2_len)
		return -1;

#ifdef WINDOWS
	return memcmp(s1, s2, s1_len); /* VC2013 doesn't have bcmp(), sadly. */
#else
	return bcmp(s1, s2, s1_len);  /* Fastest option. */
#endif
}

#define MIN(x,y) (((x) > (y)) ? (y) : (x))
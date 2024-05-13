
#ifndef JUICE_EXPORT_H
#define JUICE_EXPORT_H

#ifdef JUICE_STATIC
#  define JUICE_EXPORT
#  define JUICE_NO_EXPORT
#else
#  ifndef JUICE_EXPORT
#    ifdef juice_EXPORTS
        /* We are building this library */
#      define JUICE_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define JUICE_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef JUICE_NO_EXPORT
#    define JUICE_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef JUICE_DEPRECATED
#  define JUICE_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef JUICE_DEPRECATED_EXPORT
#  define JUICE_DEPRECATED_EXPORT JUICE_EXPORT JUICE_DEPRECATED
#endif

#ifndef JUICE_DEPRECATED_NO_EXPORT
#  define JUICE_DEPRECATED_NO_EXPORT JUICE_NO_EXPORT JUICE_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef JUICE_NO_DEPRECATED
#    define JUICE_NO_DEPRECATED
#  endif
#endif

#endif /* JUICE_EXPORT_H */

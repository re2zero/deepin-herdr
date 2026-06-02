
#ifndef TERMINALWIDGET_EXPORT_H
#define TERMINALWIDGET_EXPORT_H

#ifdef TERMINALWIDGET6_STATIC_DEFINE
#  define TERMINALWIDGET_EXPORT
#  define TERMINALWIDGET6_NO_EXPORT
#else
#  ifndef TERMINALWIDGET_EXPORT
#    ifdef terminalwidget6_EXPORTS
        /* We are building this library */
#      define TERMINALWIDGET_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define TERMINALWIDGET_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef TERMINALWIDGET6_NO_EXPORT
#    define TERMINALWIDGET6_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef TERMINALWIDGET6_DEPRECATED
#  define TERMINALWIDGET6_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef TERMINALWIDGET6_DEPRECATED_EXPORT
#  define TERMINALWIDGET6_DEPRECATED_EXPORT TERMINALWIDGET_EXPORT TERMINALWIDGET6_DEPRECATED
#endif

#ifndef TERMINALWIDGET6_DEPRECATED_NO_EXPORT
#  define TERMINALWIDGET6_DEPRECATED_NO_EXPORT TERMINALWIDGET6_NO_EXPORT TERMINALWIDGET6_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef TERMINALWIDGET6_NO_DEPRECATED
#    define TERMINALWIDGET6_NO_DEPRECATED
#  endif
#endif

#endif /* TERMINALWIDGET_EXPORT_H */

#ifndef LIBCLIENT_EXPORT_H
#define LIBCLIENT_EXPORT_H

#include <QtGlobal>

#ifndef LIBCLIENT_STATIC
# ifdef LIBCLIENT_BUILD
#  define CLIENT_API Q_DECL_EXPORT
# else // LIBCLIENT_BUILD
#  define CLIENT_API Q_DECL_IMPORT
# endif // LIBCLIENT_BUILD
#else // LIBCLIENT_STATIC
#define CLIENT_API
#endif // LIBCLIENT_STATIC

#endif // LIBCLIENT_EXPORT_H

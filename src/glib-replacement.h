#ifndef GLIB_REPLACEMENT
#define GLIB_REPLACEMENT

#include <string.h>
#include <assert.h>
#include <stdlib.h>
///#include <glib.h>

#undef	MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef	MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

typedef char   gchar;
typedef short  gshort;
typedef long   glong;
typedef int    gint;
typedef gint   gboolean;

typedef unsigned char   guchar;
typedef unsigned short  gushort;
typedef unsigned long   gulong;
typedef unsigned int    guint;

typedef float   gfloat;
typedef double  gdouble;

typedef signed char gint8;
typedef unsigned char guint8;
typedef signed short gint16;
typedef unsigned short guint16;

typedef guint16 gunichar2;

typedef signed int gint32;
typedef unsigned int guint32;
typedef guint32 gunichar;
typedef void* gpointer;

typedef struct _GArray		GArray;
typedef struct _GByteArray	GByteArray;
typedef struct _GPtrArray	GPtrArray;

struct _GArray
{
	gchar *data;
	guint len;
};

struct _GByteArray
{
	guint8 *data;
	guint	  len;
};

struct _GPtrArray
{
	gpointer *pdata;
	guint	    len;
};

#define g_warning printf
#define g_strdup strdup
#define g_assert assert
#define g_free free
#endif

#ifndef UCD_SNMP_STRUCT
#define UCD_SNMP_STRUCT

#define STRMAX 1024
#define SHPROC 1
#define EXECPROC 2
#define PASSTHRU 3
#define PASSTHRU_PERSIST 4
#define MIBMAX 30

struct extensible {
    char            name[STRMAX];
    char            command[STRMAX];
    char            fixcmd[STRMAX];
    int             type;
    int             result;
    char            output[STRMAX];
    struct extensible *next;
    oid             miboid[MIBMAX];
    size_t          miblen;
    netsnmp_pid_t   pid;
#if defined(WIN32)
    HANDLE          tid;                /* WIN32 thread */
#endif
};

#if HAVE_PCRE_H
// Pointer to pcre struct. Abstract pcre native pointer so all *.c files
// do not have to include pcre.h
typedef netsnmp_regex_ptr (void *);
#endif

struct myproc {
    char            name[STRMAX];
#if HAVE_PCRE_H
    netsnmp_regex_ptr *regexp;
#endif
    char            fixcmd[STRMAX];
    int             min;
    int             max;
    struct myproc  *next;
};

/*
 * struct mibinfo 
 * {
 * int numid;
 * unsigned long mibid[10];
 * char *name;
 * void (*handle) ();
 * };
 */

#endif

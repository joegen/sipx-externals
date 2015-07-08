/************************************************

  postgres.c -

  Author: matz 
  created at: Tue May 13 20:07:35 JST 1997

  Author: ematsu
  modified at: Wed Jan 20 16:41:51 1999

  $Author: noboru $
  $Date: 2003/01/06 01:38:20 $
************************************************/

#include "ruby.h"
#include "rubyio.h"

#include <libpq-fe.h>
#include <libpq/libpq-fs.h>              /* large-object interface */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifndef HAVE_PG_ENCODING_TO_CHAR
#define pg_encoding_to_char(x) "SQL_ASCII"
#endif

static VALUE rb_cPGconn;
static VALUE rb_cPGresult;

static VALUE rb_ePGError;

static VALUE pgresult_result _((VALUE));
static VALUE pgresult_result_with_clear _((VALUE));
static VALUE pgresult_new _((PGresult*));

EXTERN VALUE rb_mEnumerable;

static VALUE rb_cPGlarge; 

/* Large Object support */
typedef struct pglarge_object
{
    PGconn *pgconn;
    Oid lo_oid;
    int lo_fd;
} PGlarge;

static VALUE pglarge_new _((PGconn*, Oid, int));
/* Large Object support */

static void
free_pgconn(ptr)
    PGconn *ptr;
{
    PQfinish(ptr);
}

static VALUE
pgconn_s_connect(argc, argv, pgconn)
    int argc;
    VALUE *argv;
    VALUE pgconn;
{
    VALUE arg[7];
    char *pghost, *pgopt, *pgtty, *pgdbname, *pglogin, *pgpwd;
    int pgport;
    char port_buffer[20];
    PGconn *conn;

    pghost=pgopt=pgtty=pgdbname=pglogin=pgpwd=NULL;
    pgport = -1;
    rb_scan_args(argc,argv,"07", &arg[0], &arg[1], &arg[2], &arg[3], &arg[4],
                                &arg[5], &arg[6]);
    if (!NIL_P(arg[0])) {
	Check_Type(arg[0], T_STRING);
	pghost = STR2CSTR(arg[0]);
    }
    if (!NIL_P(arg[1])) {
	pgport = NUM2INT(arg[1]);
    }
    if (!NIL_P(arg[2])) {
	Check_Type(arg[2], T_STRING);
	pgopt = STR2CSTR(arg[2]);
    }
    if (!NIL_P(arg[3])) {
	Check_Type(arg[3], T_STRING);
	pgtty = STR2CSTR(arg[3]);
    }
    if (!NIL_P(arg[4])) {
	Check_Type(arg[4], T_STRING);
	pgdbname = STR2CSTR(arg[4]);
    }
    if (!NIL_P(arg[5])) {
        Check_Type(arg[5], T_STRING);
        pglogin = STR2CSTR(arg[5]);
    }
    if (!NIL_P(arg[6])) {
        Check_Type(arg[6], T_STRING);
        pgpwd = STR2CSTR(arg[6]);
    }
    if (pgport!=-1) {
	sprintf(port_buffer, "%d", pgport);
        conn = PQsetdbLogin(pghost, port_buffer, pgopt, pgtty, pgdbname,
                            pglogin, pgpwd);
    }
    else {
        conn = PQsetdbLogin(pghost, NULL, pgopt, pgtty, pgdbname,
                            pglogin, pgpwd);
    }

    if (PQstatus(conn) == CONNECTION_BAD) {
	rb_raise(rb_ePGError, PQerrorMessage(conn));
    }

    return Data_Wrap_Struct(pgconn, 0, free_pgconn, conn);
}

#ifdef HAVE_PQESCAPESTRING
static VALUE
pgconn_s_escape(self, obj)
    VALUE self;
    VALUE obj;
{
    char *to;
    long len;
    VALUE ret;
    
    Check_Type(obj, T_STRING);
    
    to = ALLOC_N(char, RSTRING(obj)->len * 2);
    
    len = PQescapeString(to, RSTRING(obj)->ptr, RSTRING(obj)->len);
    
    ret = rb_str_new(to, len);
    OBJ_INFECT(ret, obj);
    
    free(to);
    
    return ret;
}

static VALUE
pgconn_s_quote(self, obj)
    VALUE self;
    VALUE obj;
{
    VALUE ret;
    char *to;
    long len;
    long idx;
    
    switch(TYPE(obj)) {
    case T_STRING:
      
      to = ALLOC_N(char, RSTRING(obj)->len * 2 + 2);
      
      *to = '\'';
      len = PQescapeString(to + 1, RSTRING(obj)->ptr, RSTRING(obj)->len);
      *(to + len + 1) = '\'';
      
      ret = rb_str_new(to, len + 2);
      OBJ_INFECT(ret, obj);
      
      free(to);
      break;
      
    case T_FIXNUM:
    case T_BIGNUM:
    case T_FLOAT:
      ret = rb_obj_as_string(obj);
      break;
      
    case T_NIL:
      ret = rb_str_new2("NULL");
      break;
      
    case T_TRUE:
      ret = rb_str_new2("'t'");
      break;
      
    case T_FALSE:
      ret = rb_str_new2("'f'");
      break;
      
    case T_ARRAY:
      ret = rb_str_new(0,0);
      len = RARRAY(obj)->len;
      for(idx=0; idx<len; idx++) {
        rb_str_concat(ret,  pgconn_s_quote(self, rb_ary_entry(obj, idx)));
        if (idx<len-1) {
          rb_str_cat2(ret, ", ");
        }
      }
      break;
    default:
      if (rb_block_given_p()==Qtrue) {
        ret = rb_yield(obj);
      } else {
        rb_raise(rb_ePGError, "can't quote");
      }
    }
    
    return ret;
}

static VALUE
pgconn_s_escape_bytea(self, obj)
    VALUE self;
    VALUE obj;
{
    unsigned char c;
    char *from, *to;
    long idx;
    size_t from_len, to_len;
    VALUE ret;
    
    Check_Type(obj, T_STRING);
    from      = RSTRING(obj)->ptr;
    from_len  = RSTRING(obj)->len;
    
    to = (char *)PQescapeBytea(from, from_len, &to_len);
    
    ret = rb_str_new(to, to_len - 1);
    OBJ_INFECT(ret, obj);
    
    free(to);
    
    return ret;
}
#endif

static VALUE
pgconn_s_new(argc, argv, pgconn)
    int argc;
    VALUE *argv;
    VALUE pgconn;
{
    VALUE conn = pgconn_s_connect(argc, argv, pgconn);

    rb_obj_call_init(conn, argc, argv);
    return conn;
}

static VALUE
pgconn_init(argc, argv, conn)
     int argc;
     VALUE *argv;
     VALUE conn;
{
     return conn;
}

static PGconn*
get_pgconn(obj)
    VALUE obj;
{
    PGconn *conn;

    Data_Get_Struct(obj, PGconn, conn);
    if (conn == 0) rb_raise(rb_ePGError, "closed connection");
    return conn;
}

static VALUE
pgconn_close(obj)
    VALUE obj;
{
    PQfinish(get_pgconn(obj));
    DATA_PTR(obj) = 0;
    
    return Qnil;
}

static VALUE
pgconn_reset(obj)
    VALUE obj;
{
    PQreset(get_pgconn(obj));
    return obj;
}

static PGresult*
get_pgresult(obj)
    VALUE obj;
{
    PGresult *result;

    Data_Get_Struct(obj, PGresult, result);
    if (result == 0) rb_raise(rb_ePGError, "query not performed");
    return result;
}

static VALUE
pgconn_exec(obj, str)
    VALUE obj, str;
{
    PGconn *conn = get_pgconn(obj);
    PGresult *result;
    int status;
    char *msg;

    Check_Type(str, T_STRING);

    result = PQexec(conn, STR2CSTR(str));
    if (!result) {
	rb_raise(rb_ePGError, PQerrorMessage(conn));
    }
    status = PQresultStatus(result);

    switch (status) {
    case PGRES_TUPLES_OK:
    case PGRES_COPY_OUT:
    case PGRES_COPY_IN:
    case PGRES_EMPTY_QUERY:
    case PGRES_COMMAND_OK:	/* no data will be received */
      return pgresult_new(result);

    case PGRES_BAD_RESPONSE:
    case PGRES_FATAL_ERROR:
    case PGRES_NONFATAL_ERROR:
      msg = PQerrorMessage(conn);
      break;
    default:
      msg = "internal error : unknown result status.";
      break;
    }
    PQclear(result);
    rb_raise(rb_ePGError, msg);
}

static VALUE
pgconn_async_exec(obj, str)
    VALUE obj, str;
{
    PGconn *conn = get_pgconn(obj);
    PGresult *result;
    int status;
    char *msg;

	int cs;
	int ret;
	fd_set rset;

	Check_Type(str, T_STRING);
	
	while(result = PQgetResult(conn)) {
		PQclear(result);
	}

	if (!PQsendQuery(conn, RSTRING(str)->ptr)) {
		rb_raise(rb_ePGError, PQerrorMessage(conn));
	}

	cs = PQsocket(conn);
	for(;;) {
		FD_ZERO(&rset);
		FD_SET(cs, &rset);
		ret = rb_thread_select(cs + 1, &rset, NULL, NULL, NULL);
		if (ret < 0) {
			rb_sys_fail(0);
		}
		
		if (ret == 0) {
			continue;
		}

		if (PQconsumeInput(conn) == 0) {
			rb_raise(rb_ePGError, PQerrorMessage(conn));
		}

		if (PQisBusy(conn) == 0) {
			break;
		}
	}

	result = PQgetResult(conn);

    if (!result) {
	rb_raise(rb_ePGError, PQerrorMessage(conn));
    }
    status = PQresultStatus(result);

    switch (status) {
    case PGRES_TUPLES_OK:
    case PGRES_COPY_OUT:
    case PGRES_COPY_IN:
    case PGRES_EMPTY_QUERY:
    case PGRES_COMMAND_OK:	/* no data will be received */
      return pgresult_new(result);

    case PGRES_BAD_RESPONSE:
    case PGRES_FATAL_ERROR:
    case PGRES_NONFATAL_ERROR:
      msg = PQerrorMessage(conn);
      break;
    default:
      msg = "internal error : unknown result status.";
      break;
    }
    PQclear(result);
    rb_raise(rb_ePGError, msg);
}


static VALUE
pgconn_query(obj, str)
    VALUE obj, str;
{
    return pgresult_result_with_clear(pgconn_exec(obj, str));
}

static VALUE
pgconn_async_query(obj, str)
    VALUE obj, str;
{
    return pgresult_result_with_clear(pgconn_async_exec(obj, str));
}


static VALUE
pgconn_get_notify(obj)
    VALUE obj;
{
    PGnotify *notify;
    VALUE ary;

    /* gets notify and builds result */
    notify = PQnotifies(get_pgconn(obj));
    if (notify == NULL) {
        /* there are no unhandled notifications */
        return Qnil;
    }
    ary = rb_ary_new3(2, rb_tainted_str_new2(notify->relname),
		      INT2NUM(notify->be_pid));
    free(notify);

    /* returns result */
    return ary;
}

static VALUE pg_escape_regex;
static VALUE pg_escape_str;
static ID    pg_gsub_bang_id;

static VALUE
pgconn_insert_table(obj, table, values)
    VALUE obj, table, values;
{
    PGconn *conn = get_pgconn(obj);
    PGresult *result;
    VALUE s, buffer;
    int i, j;
    int res = 0;

    Check_Type(table, T_STRING);
    Check_Type(values, T_ARRAY);
    i = RARRAY(values)->len;
    while (i--) {
	if (TYPE(RARRAY(RARRAY(values)->ptr[i])) != T_ARRAY) {
	    rb_raise(rb_ePGError, "second arg must contain some kind of arrays.");
	}
    }
    
    buffer = rb_str_new(0, RSTRING(table)->len + 17 + 1);
    /* starts query */
    snprintf(RSTRING(buffer)->ptr, RSTRING(buffer)->len, "copy %s from stdin ", STR2CSTR(table));
    
    result = PQexec(conn, STR2CSTR(buffer));
    if (!result){
	rb_raise(rb_ePGError, PQerrorMessage(conn));
    }
    PQclear(result);

    for (i = 0; i < RARRAY(values)->len; i++) {
	struct RArray *row = RARRAY(RARRAY(values)->ptr[i]);
        buffer = rb_tainted_str_new(0,0);
	for (j = 0; j < row->len; j++) {
	    if (j > 0) rb_str_cat(buffer, "\t", 1);
	    if (NIL_P(row->ptr[j])) {
		rb_str_cat(buffer, "\\N",2);
	    } else {
		s = rb_obj_as_string(row->ptr[j]);
		rb_funcall(s,pg_gsub_bang_id,2,pg_escape_regex,pg_escape_str);
		rb_str_cat(buffer, STR2CSTR(s), RSTRING(s)->len);
	    }
	}
	rb_str_cat(buffer, "\n\0", 2);
	/* sends data */
	PQputline(conn, STR2CSTR(buffer));
    }
    PQputline(conn, "\\.\n");
    res = PQendcopy(conn);

    return obj;
}

static VALUE
pgconn_putline(obj, str)
    VALUE obj, str;
{
    Check_Type(str, T_STRING);
    PQputline(get_pgconn(obj), STR2CSTR(str));
    return obj;
}

static VALUE
pgconn_getline(obj)
    VALUE obj;
{
    PGconn *conn = get_pgconn(obj);
    VALUE str;
    long size = BUFSIZ;
    long bytes = 0;
    int	 ret;
    
    str = rb_tainted_str_new(0, size);

    for (;;) {
      ret = PQgetline(conn, RSTRING(str)->ptr + bytes, size - bytes);
      switch (ret) {
	case EOF:
	  return Qnil;
        case 0:
	  return str;
        case 1:
	  break;
      }
      bytes += BUFSIZ;
      size += BUFSIZ;
      rb_str_resize(str, size);
    }
    return Qnil;
}

static VALUE
pgconn_endcopy(obj)
    VALUE obj;
{
    if (PQendcopy(get_pgconn(obj)) == 1) {
        rb_raise(rb_ePGError, "cannot complete copying");
    }
    return Qnil;
}

static VALUE
pgconn_notifies(obj)
    VALUE obj;
{
    PGnotify *notifies = PQnotifies(get_pgconn(obj));
    return Qnil;
}

static VALUE
pgconn_host(obj)
    VALUE obj;
{
    char *host = PQhost(get_pgconn(obj));
    if (!host) return Qnil;
    return rb_tainted_str_new2(host);
}

static VALUE
pgconn_port(obj)
    VALUE obj;
{
    char* port = PQport(get_pgconn(obj));
    return INT2NUM(atol(port));
}

static VALUE
pgconn_db(obj)
    VALUE obj;
{
    char *db = PQdb(get_pgconn(obj));
    if (!db) return Qnil;
    return rb_tainted_str_new2(db);
}

static VALUE
pgconn_options(obj)
    VALUE obj;
{
    char *options = PQoptions(get_pgconn(obj));
    if (!options) return Qnil;
    return rb_tainted_str_new2(options);
}

static VALUE
pgconn_tty(obj)
    VALUE obj;
{
    char *tty = PQtty(get_pgconn(obj));
    if (!tty) return Qnil;
    return rb_tainted_str_new2(tty);
}

static VALUE
pgconn_user(obj)
    VALUE obj;
{
    char *user = PQuser(get_pgconn(obj));
    if (!user) return Qnil;
    return rb_tainted_str_new2(user);
}

static VALUE
pgconn_status(obj)
    VALUE obj;
{
    int status = PQstatus(get_pgconn(obj));
    return INT2NUM(status);
}

static VALUE
pgconn_error(obj)
    VALUE obj;
{
    char *error = PQerrorMessage(get_pgconn(obj));
    if (!error) return Qnil;
    return rb_tainted_str_new2(error);
}

static VALUE
pgconn_trace(obj, port)
    VALUE obj, port;
{
    OpenFile* fp;

    Check_Type(port, T_FILE);
    GetOpenFile(port, fp);

    PQtrace(get_pgconn(obj), fp->f2?fp->f2:fp->f);

    return obj;
}

static VALUE
pgconn_untrace(obj)
    VALUE obj;
{
    PQuntrace(get_pgconn(obj));
    return obj;
}

#ifdef HAVE_PQSETCLIENTENCODING
static VALUE
pgconn_client_encoding(obj)
    VALUE obj;
{
    char *encoding = (char *)pg_encoding_to_char(PQclientEncoding(get_pgconn(obj)));
    return rb_tainted_str_new2(encoding);
}

static VALUE
pgconn_set_client_encoding(obj, str)
    VALUE obj, str;
{
    Check_Type(str, T_STRING);
    if ((PQsetClientEncoding(get_pgconn(obj), STR2CSTR(str))) == -1){
	rb_raise(rb_ePGError, "invalid encoding name %s",str);
    }
    return Qnil;
}
#endif
    
static void
free_pgresult(ptr)
    PGresult *ptr;
{
    PQclear(ptr);
}

static VALUE
fetch_pgresult(result, tuple_index, field_index)
	PGresult *result;
	int tuple_index;
	int field_index;
{
	VALUE value;
	if (PQgetisnull(result, tuple_index, field_index) != 1) {
		char * valuestr = PQgetvalue(result, tuple_index, field_index);
		value = rb_tainted_str_new2(valuestr);
	} else {
		value = Qnil;
	}
	return value;
}


static VALUE
pgresult_new(ptr)
    PGresult *ptr;
{
    return Data_Wrap_Struct(rb_cPGresult, 0, free_pgresult, ptr);
}

static VALUE
pgresult_status(obj)
    VALUE obj;
{
    int status;

    status = PQresultStatus(get_pgresult(obj));
    return INT2NUM(status);
}

static VALUE
pgresult_result(obj)
    VALUE obj;
{
    PGresult *result;
    VALUE ary;
    int nt, nf, i, j;

    result = get_pgresult(obj);
    nt = PQntuples(result);
    nf = PQnfields(result);
    ary = rb_ary_new2(nt);
    for (i=0; i<nt; i++) {
	VALUE row = rb_ary_new2(nf);
	for (j=0; j<nf; j++) {
		VALUE value = fetch_pgresult(result, i, j);
	    rb_ary_push(row, value);
	}
	rb_ary_push(ary, row);
    };

    return ary;
}

static VALUE
pgresult_each(obj)
    VALUE obj;
{
    PGresult *result;
    int nt, nf, i, j;

    result = get_pgresult(obj);
    nt = PQntuples(result);
    nf = PQnfields(result);
    for (i=0; i<nt; i++) {
        VALUE row = rb_ary_new2(nf);
        for (j=0; j<nf; j++) {
			VALUE value = fetch_pgresult(result, i, j);
            rb_ary_push(row, value);
        }
        rb_yield(row);
    };

    return Qnil;
}

static VALUE
pgresult_aref(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    PGresult *result;
    VALUE a1, a2, val;
    int i, j, nf, nt;

    result = get_pgresult(obj);
    nt = PQntuples(result);
    nf = PQnfields(result);
    switch (rb_scan_args(argc, argv, "11", &a1, &a2)) {
      case 1:
        i = NUM2INT(a1);
	if( i >= nt ) return Qnil;

        val = rb_ary_new();
        for (j=0; j<nf; j++) {
			VALUE value = fetch_pgresult(result, i, j);
            rb_ary_push(val, value);
        }
        return val;

      case 2:
        i = NUM2INT(a1);
	if( i >= nt ) return Qnil;
        j = NUM2INT(a2);
	if( j >= nf ) return Qnil;
        return fetch_pgresult(result, i, j);

      default:
        return Qnil;		/* not reached */
    }
}

static VALUE
pgresult_fields(obj)
    VALUE obj;
{
    PGresult *result;
    VALUE ary;
    int n, i;

    result = get_pgresult(obj);
    n = PQnfields(result);
    ary = rb_ary_new2(n);
    for (i=0;i<n;i++) {
	rb_ary_push(ary, rb_tainted_str_new2(PQfname(result, i)));
    }
    return ary;
}

static VALUE
pgresult_num_tuples(obj)
    VALUE obj;
{
    int n;

    n = PQntuples(get_pgresult(obj));
    return INT2NUM(n);
}

static VALUE
pgresult_num_fields(obj)
    VALUE obj;
{
    int n;

    n = PQnfields(get_pgresult(obj));
    return INT2NUM(n);
}

static VALUE
pgresult_fieldname(obj, index)
    VALUE obj, index;
{
    PGresult *result;
    int i = NUM2INT(index);
    char *name;

    result = get_pgresult(obj);
    if (i < 0 || i >= PQnfields(result)) {
	rb_raise(rb_eArgError,"invalid field number %d", i);
    }
    name = PQfname(result, i);
    return rb_tainted_str_new2(name);
}

static VALUE
pgresult_fieldnum(obj, name)
    VALUE obj, name;
{
    int n;
    
    Check_Type(name, T_STRING);
    
    n = PQfnumber(get_pgresult(obj), STR2CSTR(name));
    if (n == -1) {
	rb_raise(rb_eArgError,"Unknown field: %s", STR2CSTR(name));
    }
    return INT2NUM(n);
}

static VALUE
pgresult_type(obj, index)
    VALUE obj, index;
{
    PGresult *result;
    int i = NUM2INT(index);
    int type;

    result = get_pgresult(obj);
    if (i < 0 || i >= PQnfields(result)) {
	rb_raise(rb_eArgError,"invalid field number %d", i);
    }
    type = PQftype(result, i);
    return INT2NUM(type);
}

static VALUE
pgresult_size(obj, index)
    VALUE obj, index;
{
    PGresult *result;
    int i = NUM2INT(index);
    int size;

    result = get_pgresult(obj);
    if (i < 0 || i >= PQnfields(result)) {
	rb_raise(rb_eArgError,"invalid field number %d", i);
    }
    size = PQfsize(result, i);
    return INT2NUM(size);
}

static VALUE
pgresult_getvalue(obj, tup_num, field_num)
    VALUE obj, tup_num, field_num;
{
    PGresult *result;
    int i = NUM2INT(tup_num);
    int j = NUM2INT(field_num);

    result = get_pgresult(obj);
    if (i < 0 || i >= PQntuples(result)) {
	rb_raise(rb_eArgError,"invalid tuple number %d", i);
    }
    if (j < 0 || j >= PQnfields(result)) {
	rb_raise(rb_eArgError,"invalid field number %d", j);
    }

	return fetch_pgresult(result, i, j);
}

static VALUE
pgresult_getlength(obj, tup_num, field_num)
    VALUE obj, tup_num, field_num;
{
    PGresult *result;
    int i = NUM2INT(tup_num);
    int j = NUM2INT(field_num);

    result = get_pgresult(obj);
    if (i < 0 || i >= PQntuples(result)) {
	rb_raise(rb_eArgError,"invalid tuple number %d", i);
    }
    if (j < 0 || j >= PQnfields(result)) {
	rb_raise(rb_eArgError,"invalid field number %d", j);
    }
    return INT2FIX(PQgetlength(result, i, j));
}

static VALUE
pgresult_getisnull(obj, tup_num, field_num)
    VALUE obj, tup_num, field_num;
{
    PGresult *result;
    int i = NUM2INT(tup_num);
    int j = NUM2INT(field_num);

    result = get_pgresult(obj);
    if (i < 0 || i >= PQntuples(result)) {
	rb_raise(rb_eArgError,"invalid tuple number %d", i);
    }
    if (j < 0 || j >= PQnfields(result)) {
	rb_raise(rb_eArgError,"invalid field number %d", j);
    }
    return PQgetisnull(result, i, j) ? Qtrue : Qfalse;
}

static VALUE
pgresult_print(obj, file, opt)
    VALUE obj, file, opt;
{
    VALUE value;
    ID mem;
    OpenFile* fp;
    PQprintOpt po;

    Check_Type(file, T_FILE);
    Check_Type(opt,  T_STRUCT);
    GetOpenFile(file, fp);

    memset(&po, 0, sizeof(po));

    mem = rb_intern("header");
    value = rb_struct_getmember(opt, mem);
    po.header = value == Qtrue ? 1 : 0;

    mem = rb_intern("align");
    value = rb_struct_getmember(opt, mem);
    po.align = value == Qtrue ? 1 : 0;

    mem = rb_intern("standard");
    value = rb_struct_getmember(opt, mem);
    po.standard = value == Qtrue ? 1 : 0;

    mem = rb_intern("html3");
    value = rb_struct_getmember(opt, mem);
    po.html3 = value == Qtrue ? 1 : 0;

    mem = rb_intern("expanded");
    value = rb_struct_getmember(opt, mem);
    po.expanded = value == Qtrue ? 1 : 0;

    mem = rb_intern("pager");
    value = rb_struct_getmember(opt, mem);
    po.pager = value == Qtrue ? 1 : 0;

    mem = rb_intern("fieldSep");
    value = rb_struct_getmember(opt, mem);
    if (!NIL_P(value)) {
      Check_Type(value, T_STRING);
      po.fieldSep = STR2CSTR(value);
    }

    mem = rb_intern("tableOpt");
    value = rb_struct_getmember(opt, mem);
    if (!NIL_P(value)) {
      Check_Type(value, T_STRING);
      po.tableOpt = STR2CSTR(value);
    }

    mem = rb_intern("caption");
    value = rb_struct_getmember(opt, mem);
    if (!NIL_P(value)) {
      Check_Type(value, T_STRING);
      po.caption = STR2CSTR(value);
    }

    PQprint(fp->f2?fp->f2:fp->f, get_pgresult(obj), &po);
    return obj;
}

static VALUE
pgresult_cmdtuples(obj)
        VALUE obj;
{
    long n;
    n = strtol(PQcmdTuples(get_pgresult(obj)),NULL, 10);
    return INT2NUM(n);
}

static VALUE
pgresult_cmdstatus(obj)
    VALUE obj;
{
    return rb_tainted_str_new2(PQcmdStatus(get_pgresult(obj)));
}

static VALUE
pgresult_clear(obj)
    VALUE obj;
{
    PQclear(get_pgresult(obj));
    DATA_PTR(obj) = 0;

    return Qnil;
}

static VALUE
pgresult_result_with_clear(obj)
    VALUE obj;
{
    VALUE tuples = pgresult_result(obj);
    pgresult_clear(obj);
    return tuples;
}

/* Large Object support */
static PGlarge*
get_pglarge(obj)
    VALUE obj;
{
    PGlarge *pglarge;
    Data_Get_Struct(obj, PGlarge, pglarge);
    if (pglarge == 0) rb_raise(rb_ePGError, "invalid large object");
    return pglarge;
}

static VALUE
pgconn_loimport(obj, filename)
    VALUE obj, filename;
{
    Oid lo_oid;

    PGconn *conn = get_pgconn(obj);

    Check_Type(filename, T_STRING);

    lo_oid = lo_import(conn, STR2CSTR(filename));
    if (lo_oid == 0) {
	rb_raise(rb_ePGError, PQerrorMessage(conn));
    }
    return pglarge_new(conn, lo_oid, -1);
}

static VALUE
pgconn_loexport(obj, lo_oid,filename)
    VALUE obj, lo_oid, filename;
{
    PGconn *conn = get_pgconn(obj);
    int oid;
    Check_Type(filename, T_STRING);

    oid = NUM2INT(lo_oid);
    if (oid < 0) {
	rb_raise(rb_ePGError, "invalid large object oid %d",oid);
    }

    if (!lo_export(conn, oid, STR2CSTR(filename))) {
	rb_raise(rb_ePGError, PQerrorMessage(conn));
    }
    return Qnil;
}

static VALUE
pgconn_locreate(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    Oid lo_oid;
    int mode;
    VALUE nmode;
    PGconn *conn;
    
    if (rb_scan_args(argc, argv, "01", &nmode) == 0) {
	mode = INV_READ;
    }
    else {
	mode = FIX2INT(nmode);
    }
  
    conn = get_pgconn(obj);
    lo_oid = lo_creat(conn, mode);
    if (lo_oid == 0){
	rb_raise(rb_ePGError, "can't creat large object");
    }

    return pglarge_new(conn, lo_oid, -1);
}

static VALUE
pgconn_loopen(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    Oid lo_oid;
    int fd, mode;
    VALUE nmode, objid;
    PGconn *conn = get_pgconn(obj);

    switch (rb_scan_args(argc, argv, "02", &objid, &nmode)) {
      case 1:
	lo_oid = NUM2INT(objid);
	mode = INV_READ;
	break;
      case 2:
	lo_oid = NUM2INT(objid);
	mode = FIX2INT(nmode);
	break;
      default:
	mode = INV_READ;
	lo_oid = lo_creat(conn, mode);
	if (lo_oid == 0){
	    rb_raise(rb_ePGError, "can't creat large object");
	}
    }
    if((fd = lo_open(conn, lo_oid, mode)) < 0) {
	rb_raise(rb_ePGError, "can't open large object");
    }
    return pglarge_new(conn, lo_oid, fd);
}

static VALUE
pgconn_lounlink(obj, lo_oid)
    VALUE obj, lo_oid;
{
    PGconn *conn;
    int oid = NUM2INT(lo_oid);
    int result;
    
    if (oid < 0){
	rb_raise(rb_ePGError, "invalid oid %d",oid);
    }
    conn = get_pgconn(obj);
    result = lo_unlink(conn,oid);

    return Qnil;
}

static void
free_pglarge(ptr)
    PGlarge *ptr;
{
    if ((ptr->lo_fd) > 0) {
	lo_close(ptr->pgconn,ptr->lo_fd);
    }
    free(ptr);
}

static VALUE
pglarge_new(conn, lo_oid ,lo_fd)
    PGconn *conn;
    Oid lo_oid;
    int lo_fd;
{
    VALUE obj;
    PGlarge *pglarge;

    obj = Data_Make_Struct(rb_cPGlarge, PGlarge, 0, free_pglarge, pglarge);
    pglarge->pgconn = conn;
    pglarge->lo_oid = lo_oid;
    pglarge->lo_fd = lo_fd;

    return obj;
}

static VALUE
pglarge_oid(obj)
    VALUE obj;
{
    PGlarge *pglarge = get_pglarge(obj);

    return INT2NUM(pglarge->lo_oid);
}

static VALUE
pglarge_open(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    PGlarge *pglarge = get_pglarge(obj);
    VALUE nmode;
    int fd;
    int mode;

    if (rb_scan_args(argc, argv, "01", &nmode) == 0) {
	mode = INV_READ;
    }
    else {
	mode = FIX2INT(nmode);
    }
  
    if((fd = lo_open(pglarge->pgconn, pglarge->lo_oid, mode)) < 0) {
	rb_raise(rb_ePGError, "can't open large object");
    }
    pglarge->lo_fd = fd;

    return INT2FIX(pglarge->lo_fd);
}

static VALUE
pglarge_close(obj)
    VALUE obj;
{
    PGlarge *pglarge = get_pglarge(obj);

    if((lo_close(pglarge->pgconn, pglarge->lo_fd)) < 0) {
	rb_raise(rb_ePGError, "can't closed large object");
    }
    DATA_PTR(obj) = 0;
  
    return Qnil;
}

static VALUE
pglarge_tell(obj)
    VALUE obj;
{
    int start;
    PGlarge *pglarge = get_pglarge(obj);

    if ((start = lo_tell(pglarge->pgconn,pglarge->lo_fd)) == -1) {
	rb_raise(rb_ePGError, "error while getting position");
    }
    return INT2NUM(start);
}

static VALUE
loread_all(obj)
    VALUE obj;
{
    PGlarge *pglarge = get_pglarge(obj);
    VALUE str;
    long siz = BUFSIZ;
    long bytes = 0;
    int n;

    str = rb_tainted_str_new(0,siz);
    for (;;) {
	n = lo_read(pglarge->pgconn, pglarge->lo_fd, RSTRING(str)->ptr + bytes,siz - bytes);
	if (n == 0 && bytes == 0) return Qnil;
	bytes += n;
	if (bytes < siz ) break;
	siz += BUFSIZ;
	rb_str_resize(str,siz);
    }
    if (bytes == 0) return rb_tainted_str_new(0,0);
    if (bytes != siz) rb_str_resize(str, bytes);
    return str;
}

static VALUE
pglarge_read(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    int len;
    PGlarge *pglarge = get_pglarge(obj);
    VALUE str;
    VALUE length;
    
    rb_scan_args(argc, argv, "01", &length);
    if (NIL_P(length)) {
	return loread_all(obj);
    }
    
    len = NUM2INT(length);
    if (len < 0){
	rb_raise(rb_ePGError,"nagative length %d given", len);
    }
    str = rb_tainted_str_new(0,len);

    if((len = lo_read(pglarge->pgconn, pglarge->lo_fd, STR2CSTR(str), len)) < 0) {
	rb_raise(rb_ePGError, "error while reading");
    }
    if (len == 0) return Qnil;
    RSTRING(str)->len = len;
    return str;
}

static VALUE
pglarge_write(obj, buffer)
    VALUE obj, buffer;
{
    int n;
    PGlarge *pglarge = get_pglarge(obj);

    Check_Type(buffer, T_STRING);

    if( RSTRING(buffer)->len < 0) {
	rb_raise(rb_ePGError, "write buffer zero string");
    }
    if((n = lo_write(pglarge->pgconn, pglarge->lo_fd, STR2CSTR(buffer), RSTRING(buffer)->len)) == -1) {
	rb_raise(rb_ePGError, "buffer truncated during write");
    }
  
    return INT2FIX(n);
}

static VALUE
pglarge_seek(obj, offset, whence)
    VALUE obj, offset, whence;
{
    PGlarge *pglarge = get_pglarge(obj);
    int ret;
    
    if((ret = lo_lseek(pglarge->pgconn, pglarge->lo_fd, NUM2INT(offset), NUM2INT(whence))) == -1) {
	rb_raise(rb_ePGError, "error while moving cursor");
    }

    return INT2NUM(ret);
}

static VALUE
pglarge_size(obj)
    VALUE obj;
{
    PGlarge *pglarge = get_pglarge(obj);
    int start, end;

    if ((start = lo_tell(pglarge->pgconn,pglarge->lo_fd)) == -1) {
	rb_raise(rb_ePGError, "error while getting position");
    }

    if ((end = lo_lseek(pglarge->pgconn, pglarge->lo_fd, 0, SEEK_END)) == -1) {
	rb_raise(rb_ePGError, "error while moving cursor");
    }

    if ((start = lo_lseek(pglarge->pgconn, pglarge->lo_fd,start, SEEK_SET)) == -1) {
	rb_raise(rb_ePGError, "error while moving back to posiion");
    }
	
    return INT2NUM(end);
}
    
static VALUE
pglarge_export(obj, filename)
    VALUE obj, filename;
{
    PGlarge *pglarge = get_pglarge(obj);

    Check_Type(filename, T_STRING);

    if (!lo_export(pglarge->pgconn, pglarge->lo_oid, STR2CSTR(filename))){
	rb_raise(rb_ePGError, PQerrorMessage(pglarge->pgconn));
    }

    return Qnil;
}

static VALUE
pglarge_unlink(obj)
    VALUE obj;
{
    PGlarge *pglarge = get_pglarge(obj);

    if (!lo_unlink(pglarge->pgconn,pglarge->lo_oid)) {
	rb_raise(rb_ePGError, PQerrorMessage(pglarge->pgconn));
    }
    DATA_PTR(obj) = 0;

    return Qnil;
}
/* Large Object support */

void
Init_postgres()
{
    pg_gsub_bang_id = rb_intern("gsub!");
    pg_escape_regex = rb_reg_new("([\\t\\n\\\\])",10,0);
    rb_global_variable(&pg_escape_regex);
    pg_escape_str = rb_str_new("\\\\\\1",4);
    rb_global_variable(&pg_escape_str);

    rb_ePGError = rb_define_class("PGError", rb_eStandardError);

    rb_cPGconn = rb_define_class("PGconn", rb_cObject);
    rb_define_singleton_method(rb_cPGconn, "new", pgconn_s_new, -1);
    rb_define_singleton_method(rb_cPGconn, "connect", pgconn_s_connect, -1);
    rb_define_singleton_method(rb_cPGconn, "setdb", pgconn_s_connect, -1);
    rb_define_singleton_method(rb_cPGconn, "setdblogin", pgconn_s_connect, -1);
#ifdef HAVE_PQESCAPESTRING
    rb_define_singleton_method(rb_cPGconn, "escape", pgconn_s_escape, 1);
    rb_define_singleton_method(rb_cPGconn, "quote", pgconn_s_quote, 1);
    rb_define_singleton_method(rb_cPGconn, "escape_bytea", pgconn_s_escape_bytea, 1);
#endif
    rb_define_const(rb_cPGconn, "CONNECTION_OK", INT2FIX(CONNECTION_OK));
    rb_define_const(rb_cPGconn, "CONNECTION_BAD", INT2FIX(CONNECTION_BAD));

    rb_define_method(rb_cPGconn, "initialize", pgconn_init, -1);    
    rb_define_method(rb_cPGconn, "db", pgconn_db, 0);
    rb_define_method(rb_cPGconn, "host", pgconn_host, 0);
    rb_define_method(rb_cPGconn, "options", pgconn_options, 0);
    rb_define_method(rb_cPGconn, "port", pgconn_port, 0);
    rb_define_method(rb_cPGconn, "tty", pgconn_tty, 0);
    rb_define_method(rb_cPGconn, "status", pgconn_status, 0);
    rb_define_method(rb_cPGconn, "error", pgconn_error, 0);
    rb_define_method(rb_cPGconn, "close", pgconn_close, 0);
    rb_define_alias(rb_cPGconn, "finish", "close");
    rb_define_method(rb_cPGconn, "reset", pgconn_reset, 0);
    rb_define_method(rb_cPGconn, "user", pgconn_user, 0);
    rb_define_method(rb_cPGconn, "trace", pgconn_trace, 1);
    rb_define_method(rb_cPGconn, "untrace", pgconn_untrace, 0);

    rb_define_method(rb_cPGconn, "exec", pgconn_exec, 1);
    rb_define_method(rb_cPGconn, "query", pgconn_query, 1);
    rb_define_method(rb_cPGconn, "async_exec", pgconn_async_exec, 1);
    rb_define_method(rb_cPGconn, "async_query", pgconn_async_query, 1);
    rb_define_method(rb_cPGconn, "get_notify", pgconn_get_notify, 0);
    rb_define_method(rb_cPGconn, "insert_table", pgconn_insert_table, 2);
    rb_define_method(rb_cPGconn, "putline", pgconn_putline, 1);
    rb_define_method(rb_cPGconn, "getline", pgconn_getline, 0);
    rb_define_method(rb_cPGconn, "endcopy", pgconn_endcopy, 0);
    rb_define_method(rb_cPGconn, "notifies", pgconn_notifies, 0);

#ifdef HAVE_PQSETCLIENTENCODING
    rb_define_method(rb_cPGconn, "client_encoding",pgconn_client_encoding, 0);
    rb_define_method(rb_cPGconn, "set_client_encoding",pgconn_set_client_encoding, 1);
#endif    

/* Large Object support */
    rb_define_method(rb_cPGconn, "lo_import", pgconn_loimport, 1);
    rb_define_alias(rb_cPGconn, "loimport", "lo_import");
    rb_define_method(rb_cPGconn, "lo_create", pgconn_locreate, -1);
    rb_define_alias(rb_cPGconn, "locreate", "lo_create");
    rb_define_method(rb_cPGconn, "lo_open", pgconn_loopen, -1);
    rb_define_alias(rb_cPGconn, "loopen", "lo_open");
    rb_define_method(rb_cPGconn, "lo_export", pgconn_loexport, 2);
    rb_define_alias(rb_cPGconn, "loexport", "lo_export");
    rb_define_method(rb_cPGconn, "lo_unlink", pgconn_lounlink, 1);
    rb_define_alias(rb_cPGconn, "lounlink", "lo_unlink");
    
    rb_cPGlarge = rb_define_class("PGlarge", rb_cObject);
    rb_define_method(rb_cPGlarge, "oid",pglarge_oid, 0);
    rb_define_method(rb_cPGlarge, "open",pglarge_open, -1);
    rb_define_method(rb_cPGlarge, "close",pglarge_close, 0);
    rb_define_method(rb_cPGlarge, "read",pglarge_read, -1);
    rb_define_method(rb_cPGlarge, "write",pglarge_write, 1);
    rb_define_method(rb_cPGlarge, "seek",pglarge_seek, 2);
    rb_define_method(rb_cPGlarge, "tell",pglarge_tell, 0);
    rb_define_method(rb_cPGlarge, "size",pglarge_size, 0);
    rb_define_method(rb_cPGlarge, "export",pglarge_export, 1);
    rb_define_method(rb_cPGlarge, "unlink",pglarge_unlink, 0);

    rb_define_const(rb_cPGlarge, "INV_WRITE", INT2FIX(INV_WRITE));
    rb_define_const(rb_cPGlarge, "INV_READ", INT2FIX(INV_READ));
    rb_define_const(rb_cPGlarge, "SEEK_SET", INT2FIX(SEEK_SET));
    rb_define_const(rb_cPGlarge, "SEEK_CUR", INT2FIX(SEEK_CUR));
    rb_define_const(rb_cPGlarge, "SEEK_END", INT2FIX(SEEK_END));
/* Large Object support */
    
    rb_cPGresult = rb_define_class("PGresult", rb_cObject);
    rb_include_module(rb_cPGresult, rb_mEnumerable);

    rb_define_const(rb_cPGresult, "EMPTY_QUERY", INT2FIX(PGRES_EMPTY_QUERY));
    rb_define_const(rb_cPGresult, "COMMAND_OK", INT2FIX(PGRES_COMMAND_OK));
    rb_define_const(rb_cPGresult, "TUPLES_OK", INT2FIX(PGRES_TUPLES_OK));
    rb_define_const(rb_cPGresult, "COPY_OUT", INT2FIX(PGRES_COPY_OUT));
    rb_define_const(rb_cPGresult, "COPY_IN", INT2FIX(PGRES_COPY_IN));
    rb_define_const(rb_cPGresult, "BAD_RESPONSE", INT2FIX(PGRES_BAD_RESPONSE));
    rb_define_const(rb_cPGresult, "NONFATAL_ERROR",INT2FIX(PGRES_NONFATAL_ERROR));
    rb_define_const(rb_cPGresult, "FATAL_ERROR", INT2FIX(PGRES_FATAL_ERROR));

    rb_define_method(rb_cPGresult, "status", pgresult_status, 0);
    rb_define_method(rb_cPGresult, "result", pgresult_result, 0);
    rb_define_method(rb_cPGresult, "each", pgresult_each, 0);
    rb_define_method(rb_cPGresult, "[]", pgresult_aref, -1);
    rb_define_method(rb_cPGresult, "fields", pgresult_fields, 0);
    rb_define_method(rb_cPGresult, "num_tuples", pgresult_num_tuples, 0);
    rb_define_method(rb_cPGresult, "num_fields", pgresult_num_fields, 0);
    rb_define_method(rb_cPGresult, "fieldname", pgresult_fieldname, 1);
    rb_define_method(rb_cPGresult, "fieldnum", pgresult_fieldnum, 1);
    rb_define_method(rb_cPGresult, "type", pgresult_type, 1);
    rb_define_method(rb_cPGresult, "size", pgresult_size, 1);
    rb_define_method(rb_cPGresult, "getvalue", pgresult_getvalue, 2);
    rb_define_method(rb_cPGresult, "getlength", pgresult_getlength, 2);
    rb_define_method(rb_cPGresult, "getisnull", pgresult_getisnull, 2);
    rb_define_method(rb_cPGresult, "cmdtuples", pgresult_cmdtuples, 0);
    rb_define_method(rb_cPGresult, "cmdstatus", pgresult_cmdstatus, 0);
    rb_define_method(rb_cPGresult, "print", pgresult_print, 2);
    rb_define_method(rb_cPGresult, "clear", pgresult_clear, 0);
}

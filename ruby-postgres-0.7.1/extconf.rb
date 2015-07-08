if VERSION < "1.3"
  print "This library is for ruby-1.3 or higher.\n"
  exit 1
end

require "mkmf"

dir_config('pgsql')

$CFLAGS = ""
$LDFLAGS = ""

have_library("wsock32", "cygwin32_socket") or have_library("socket", "socket")
have_library("inet", "gethostbyname")
have_library("nsl", "gethostbyname")
have_header("sys/un.h")
if have_func("socket") or have_func("cygwin32_socket")
  have_func("hsterror")
  unless have_func("gethostname")
    have_func("uname")
  end
  if ENV["SOCKS_SERVER"]  # test if SOCKSsocket needed
    if have_library("socks", "Rconnect")
      $CFLAGS+="-DSOCKS"
    end
  end
  incdir = ENV["POSTGRES_INCLUDE"]
  incdir ||= with_config("pgsql-include-dir")
  if incdir
    $CFLAGS += "-I#{incdir}"
    puts "Using PostgreSQL include directory: #{incdir}"
  end
  libdir = ENV["POSTGRES_LIB"]
  libdir ||= with_config("pgsql-lib-dir")
  if libdir
    $LDFLAGS += "-L#{libdir}"
    puts "Using PostgreSQL lib directory: #{libdir}"
  end
  if have_library("pq", "PQsetdbLogin")
    have_func("PQsetClientEncoding")
    have_func("pg_encoding_to_char")
    have_func("PQescapeString")
    create_makefile("postgres")
  else
    puts "Could not find PostgreSQL libraries: Makefile not created"    
  end
end

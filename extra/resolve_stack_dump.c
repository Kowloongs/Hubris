/*
   Copyright (c) 2001, 2010, Oracle and/or its affiliates

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1335  USA */

/* Resolve numeric stack dump produced by mysqld 3.23.30 and later
   versions into symbolic names. By Sasha Pachev <sasha@mysql.com>
 */

#define VER "1.4"
#include <my_global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql_version.h>
#include <errno.h>
#include <my_getopt.h>
#include <welcome_copyright_notice.h>

#define INIT_SYM_TABLE  4096
#define INC_SYM_TABLE  4096
#define MAX_SYM_SIZE   128
#define HEX_INVALID  (uchar)255

typedef ulong my_long_addr_t ; /* at some point, we need to fix configure
				* to define this for us  
				*/

typedef struct sym_entry
{
  char symbol[MAX_SYM_SIZE];
  uchar* addr;
} SYM_ENTRY;


static char* dump_fname = 0, *sym_fname = 0;
static DYNAMIC_ARRAY sym_table; /* how do you like this , static DYNAMIC ? */
static FILE* fp_dump, *fp_sym = 0, *fp_out; 

static struct my_option my_long_options[] =
{
  {"help", 'h', "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"symbols-file", 's', "Use specified symbols file", &sym_fname,
   &sym_fname, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"numeric-dump-file", 'n', "Read the dump from specified file.",
   &dump_fname, &dump_fname, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void verify_sort();
static void clean_up();

static void usage()
{
  print_version();
  printf("MariaDB Corporation, originally created by Sasha Pachev\n");
  printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
  printf("Resolve numeric stack strace dump into symbols.\n\n");
  printf("Usage: %s [OPTIONS] symbols-file [numeric-dump-file]\n",
	 my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
  printf("\n\
The symbols-file should include the output from:  'nm --numeric-sort mysqld'.\n\
The numeric-dump-file should contain a numeric stack trace from mysqld.\n\
If the numeric-dump-file is not given, the stack trace is read from stdin.\n");
}


static void die(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s: ", my_progname);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  clean_up();
  my_end(0);
  exit(1);
}

void local_exit(int error)
{
  clean_up();
  my_end(0);
  exit(error);
}


static my_bool
get_one_option(const struct my_option *opt,
	       const char *argument __attribute__((unused)),
               const char *filename __attribute__((unused)))
{
  switch(opt->id) {
  case 'V':
    print_version();
    local_exit(0);
    break;
  case '?':
    usage();
    local_exit(0);
    break;
  }
  return 0;
}


static int parse_args(int argc, char **argv)
{
  int ho_error;

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    local_exit(ho_error);

  /*
    The following code is to make the command compatible with the old
    version that required one to use the -n and -s options
  */

  if (argc == 2)
  {
    sym_fname= argv[0];
    dump_fname= argv[1];
  }
  else if (argc == 1)
  {
    if (!sym_fname)
      sym_fname = argv[0];
    else if (!dump_fname)
      dump_fname = argv[0];
    else
    {
      usage();
      local_exit(1);
    }
  }
  else if (argc != 0 || !sym_fname)
  {
    usage();
    local_exit(1);
  }
  return 0;
}


static void open_files()
{
  fp_out = stdout;
  fp_dump = stdin;

  if (dump_fname && !(fp_dump = my_fopen(dump_fname, O_RDONLY, MYF(MY_WME))))
      die("Could not open %s", dump_fname);
  /* if name not given, assume stdin*/

  if (!sym_fname)
    die("Please run nm --numeric-sort on mariadbd binary that produced stack \
trace dump and specify the path to it with -s or --symbols-file");
  if (!(fp_sym = my_fopen(sym_fname, O_RDONLY, MYF(MY_WME))))
    die("Could not open %s", sym_fname);

}

static uchar hex_val(char c)
{
  uchar l;
  if (my_isdigit(&my_charset_latin1,c))
    return c - '0';
  l = my_tolower(&my_charset_latin1,c);
  if (l < 'a' || l > 'f')
    return HEX_INVALID; 
  return (uchar)10 + ((uchar)c - (uchar)'a');
}

static my_long_addr_t read_addr(char** buf)
{
  uchar c;
  char* p = *buf;
  my_long_addr_t addr = 0;

  while((c = hex_val(*p++)) != HEX_INVALID)
      addr = (addr << 4) + c;

  *buf= p-1;
  return addr;
}

static int init_sym_entry(SYM_ENTRY* se, char* buf)
{
  char* p, *p_end;
  se->addr = (uchar*)read_addr(&buf);

  if (!se->addr)
    return -1;
  buf++;
  while (my_isspace(&my_charset_latin1,*buf++))
    /* empty */;

  while (my_isspace(&my_charset_latin1,*buf++))
    /* empty - skip more space */;
  --buf;
  /* now we are on the symbol */
  for (p = se->symbol, p_end = se->symbol + sizeof(se->symbol) - 1;
       *buf != '\n' && *buf && p < p_end; ++buf,++p)
    *p = *buf;
  *p = 0;
  if (!strcmp(se->symbol, "gcc2_compiled."))
    return -1;
  return 0;
}

static void init_sym_table()
{
  char buf[512];
  if (my_init_dynamic_array(PSI_NOT_INSTRUMENTED, &sym_table, sizeof(SYM_ENTRY),
                            INIT_SYM_TABLE, INC_SYM_TABLE, MYF(0)))
    die("Failed in my_init_dynamic_array() -- looks like out of memory problem");

  while (fgets(buf, sizeof(buf), fp_sym))
  {
    SYM_ENTRY se;
    if (init_sym_entry(&se, buf))
      continue;
    if (insert_dynamic(&sym_table, (uchar*)&se))
      die("insert_dynamic() failed - looks like we are out of memory");
  }

  verify_sort();
}

static void clean_up()
{
  delete_dynamic(&sym_table);
  if (fp_dump && fp_dump != stdin)
    my_fclose(fp_dump, MYF(0));
  if (fp_sym)
    my_fclose(fp_sym, MYF(0));
}

static void verify_sort()
{
  uint i;
  uchar* last = 0;

  for (i = 0; i < sym_table.elements; i++)
  {
    SYM_ENTRY se;
    get_dynamic(&sym_table, (uchar*)&se, i);
    if (se.addr < last)
      die("sym table does not appear to be sorted, did you forget \
--numeric-sort arg to nm? trouble addr = %p, last = %p", se.addr, last);
    last = se.addr;
  }
}


static SYM_ENTRY* resolve_addr(uchar* addr, SYM_ENTRY* se)
{
  uint i;
  get_dynamic(&sym_table, (uchar*)se, 0);
  if (addr < se->addr)
    return 0;

  for (i = 1; i < sym_table.elements; i++)
  {
    get_dynamic(&sym_table, (uchar*)se, i);
    if (addr < se->addr)
    {
      get_dynamic(&sym_table, (uchar*)se, i - 1);
      return se;
    }
  }

  return se;
}


/*
  Resolve anything that starts with [0x or (+0x or 0x
  Skip '_end' as this is an indication of a wrong symbol (stack?)
*/

static void do_resolve()
{
  char buf[1024], *p;
  while (fgets(buf, sizeof(buf), fp_dump))
  {
    for (p= buf ; *p ; p++)
    {
      int found= 0;
      if (p[0] == '[' && p[1] == '0' && p[2] == 'x')
        found= 3;
      if (p[0] == '(' && p[1] == '+' && p[2] == '0' && p[3] == 'x')
        found= 4;
      if (p[0] == '0' && p[1] == 'x')
        found= 2;

      if (found)
      {
        SYM_ENTRY se ;
        uchar *addr;
        char *tmp= p + found;
        addr= (uchar*)read_addr(&tmp);
        if (resolve_addr(addr, &se) && strcmp(se.symbol, "_end"))
        {
          found-= 2;               /* Don't print 0x as it's added by %p */
          while (found--)
            fputc(*p++, stdout);
          fprintf(fp_out, "%p %s + %d", addr,
                  se.symbol, (int) (addr - se.addr));
          p= tmp-1;
        }
        else
          fputc(*p, stdout);
      }
      else
        fputc(*p, stdout);
    }
  }
}


int main(int argc, char** argv)
{
  MY_INIT(argv[0]);
  parse_args(argc, argv);
  open_files();
  init_sym_table();
  do_resolve();
  clean_up();
  my_end(0);
  return 0;
}

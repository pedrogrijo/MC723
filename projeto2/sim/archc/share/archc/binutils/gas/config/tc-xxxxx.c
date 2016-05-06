/* ex: set tabstop=2 expandtab: 
   -*- Mode: C; tab-width: 2; indent-tabs-mode nil -*-
*/
/* `tc-'___arch_name___`.c' -- Assemble code for ___arch_name___
   Copyright 2005, 2006, 2007, 2008 --- The ArchC Team
 
   This file is automatically retargeted by ArchC binutils generation 
   tool. This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/*
 * written by:
 *   Alexandro Baldassin            
 *   Rafael Auler
 */


#include "as.h"
#include "config.h"
#include "subsegs.h"
#include "safe-ctype.h"
#include `"tc-'___arch_name___`.h"'
#include `"elf/'___arch_name___`.h"'
#include `"opcode/'___arch_name___`.h"'
#include `"share-'___arch_name___`.h"'

/*
 * The gas source in binutils 2.15 signals fatal error
 * when unclosed parenthesis is found, and would not generate output file 
 * even when assembly is finished successful.
 * When trying to parse an invalid syntax while finding the correct one,
 * our parser may call gas's expression() (expr.c) which will halt with an
 * unexpected fatal error.
 */
#define FIX_GASEXPERR

//#define DEBUG_ON

#ifdef DEBUG_ON
#  include <stdio.h>
   extern int indent_level;

#  define dbg_printf(il, args...) {fprintf(stderr, "%*s+ ", il*4, ""); \
                                  fprintf(stderr, args);}
#  define dbg_print_insn(il, insn)  print_opcode_structure(stderr, il*4, insn)
#  define dbg_print_operand_info(il, opid) print_operand_info(stderr, il*4, opid)
#  define dbg_print_fixup(il, fixup) print_internal_fixup(stderr, il, fixup)
#  define dbg_print_expression(il, exp) {indent_level = il; \
                                     print_expr_1(stderr, exp); \
                                     fprintf(stderr, "\n");}
#  define dbg_print_gasfixup(il, fixP) print_fixup(stderr, il, fixP)
#  define dbg_print_symbolS(il, sym) {indent_level = il; \
                                     print_symbol_value_1(stderr, sym); \
                                     fprintf(stderr, "\n");}
#else 
#  define dbg_printf(il, args...)
#  define dbg_print_insn(il, insn) 
#  define dbg_print_operand_info(il, opid)
#  define dbg_print_fixup(il, fixup)
#  define dbg_print_expression(il, exp)
#  define dbg_print_gasfixup(il, fixP)
#  define dbg_print_symbolS(il, sym)
#endif


#ifdef __STDC__
#define INTERNAL_ERROR() as_fatal (_("Internal error: line %d, %s"), __LINE__, __FILE__)
#else
#define INTERNAL_ERROR() as_fatal (_("Internal error"));
#endif

typedef struct _acfixuptype
{
  expressionS ref_expr_fix; /* symbol in which the relocation points to */
  unsigned int operand_id;

  struct _acfixuptype *next;
} acfixuptype;

/* Linked list to store aliases created with .req directive */
typedef struct _acaliastype
{
  char * old_name;
  char * new_name;

  struct _acaliastype *next;
} acaliastype;

static bfd_reloc_code_real_type ac_parse_reloc PARAMS((void));
static void ac_parse_req (char *str);
static void s_cons(int byte_size);
static void s_bss (int);
static void s_req (int);
static void s_dummy (int);
static void s_unreq (int);
static void archc_show_information(void);
static void pseudo_assemble(void);
static void emit_insn(void);
static int get_expression(expressionS *, char **);
static int get_immediate(expressionS *ep, char **str);
static int get_address(expressionS *, char **);
static int get_userdefoper(unsigned int *val, const char *opclass, char **str);
static int ac_symbol_dispensable(const char *marker, unsigned int *value);
static int ac_valid_symbol(const char *ac_symbol, char *parser_symbol, 
                           unsigned int *value);
static int ac_parse_operands(char *s_pos, char *args);
static int ac_parse_mnemonic_suffixes(char **s_pos, char **args);
static void create_fixup(unsigned oper_id);
static void strtolower(char *str);
static void clean_out_insn(void);
static void free_all_fixups(acfixuptype *fix);
#ifdef DEBUG_ON
static void print_internal_fixup(FILE *stream, unsigned int il, acfixuptype *fix);
static void print_fixup(FILE *stream, unsigned int il, fixS *fixP);
static void print_req_aliases PARAMS((void));
#endif
static void encode_field(unsigned int val_const, unsigned int oper_id, list_op_results lr);
void md_apply_fix3 (fixS *fixP ATTRIBUTE_UNUSED, valueT *valP ATTRIBUTE_UNUSED, segT seg ATTRIBUTE_UNUSED);

/*
  variables used by the core
*/
const char comment_chars[] = "___comment_chars___";  // # - MIPS, ! - SPARC, @ - ARM
const char line_comment_chars[] = "___line_comment_chars___";
const char line_separator_chars[] = ";";

const char EXP_CHARS[] = "eE";
const char FLT_CHARS[] = "rRsSfFdDxXpP";
const pseudo_typeS md_pseudo_table[] = 
{ 
  {"half",  s_cons, AC_WORD_SIZE/8 /2},
  {"dword", s_cons, AC_WORD_SIZE/8 *2},
  {"byte",  s_cons, 1},
  {"hword", s_cons, AC_WORD_SIZE/8 /2},
  {"int",   s_cons, AC_WORD_SIZE/8},
  {"long",  s_cons, AC_WORD_SIZE/8},
  {"octa",  s_cons, AC_WORD_SIZE/8 *4},
  {"quad",  s_cons, AC_WORD_SIZE/8 *2},  
  {"short", s_cons, AC_WORD_SIZE/8 /2},
  {"word",  s_cons, AC_WORD_SIZE/8},
  {"bss", s_bss, 0},
  {"unreq", s_unreq, 0},
  {"req", s_req, 0},
  {"arm", s_dummy, 0},
  {"code", s_dummy, 0},
  {NULL, 0, 0},
};


typedef unsigned char uc;
static const unsigned char charmap[] = {
	(uc)'\000',(uc)'\001',(uc)'\002',(uc)'\003',(uc)'\004',(uc)'\005',(uc)'\006',(uc)'\007',
	(uc)'\010',(uc)'\011',(uc)'\012',(uc)'\013',(uc)'\014',(uc)'\015',(uc)'\016',(uc)'\017',
	(uc)'\020',(uc)'\021',(uc)'\022',(uc)'\023',(uc)'\024',(uc)'\025',(uc)'\026',(uc)'\027',
	(uc)'\030',(uc)'\031',(uc)'\032',(uc)'\033',(uc)'\034',(uc)'\035',(uc)'\036',(uc)'\037',
	(uc)'\040',(uc)'\041',(uc)'\042',(uc)'\043',(uc)'\044',(uc)'\045',(uc)'\046',(uc)'\047',
	(uc)'\050',(uc)'\051',(uc)'\052',(uc)'\053',(uc)'\054',(uc)'\055',(uc)'\056',(uc)'\057',
	(uc)'\060',(uc)'\061',(uc)'\062',(uc)'\063',(uc)'\064',(uc)'\065',(uc)'\066',(uc)'\067',
	(uc)'\070',(uc)'\071',(uc)'\072',(uc)'\073',(uc)'\074',(uc)'\075',(uc)'\076',(uc)'\077',
	(uc)'\100',(uc)'\141',(uc)'\142',(uc)'\143',(uc)'\144',(uc)'\145',(uc)'\146',(uc)'\147',
	(uc)'\150',(uc)'\151',(uc)'\152',(uc)'\153',(uc)'\154',(uc)'\155',(uc)'\156',(uc)'\157',
	(uc)'\160',(uc)'\161',(uc)'\162',(uc)'\163',(uc)'\164',(uc)'\165',(uc)'\166',(uc)'\167',
	(uc)'\170',(uc)'\171',(uc)'\172',(uc)'\133',(uc)'\134',(uc)'\135',(uc)'\136',(uc)'\137',
	(uc)'\140',(uc)'\141',(uc)'\142',(uc)'\143',(uc)'\144',(uc)'\145',(uc)'\146',(uc)'\147',
	(uc)'\150',(uc)'\151',(uc)'\152',(uc)'\153',(uc)'\154',(uc)'\155',(uc)'\156',(uc)'\157',
	(uc)'\160',(uc)'\161',(uc)'\162',(uc)'\163',(uc)'\164',(uc)'\165',(uc)'\166',(uc)'\167',
	(uc)'\170',(uc)'\171',(uc)'\172',(uc)'\173',(uc)'\174',(uc)'\175',(uc)'\176',(uc)'\177',
	(uc)'\200',(uc)'\201',(uc)'\202',(uc)'\203',(uc)'\204',(uc)'\205',(uc)'\206',(uc)'\207',
	(uc)'\210',(uc)'\211',(uc)'\212',(uc)'\213',(uc)'\214',(uc)'\215',(uc)'\216',(uc)'\217',
	(uc)'\220',(uc)'\221',(uc)'\222',(uc)'\223',(uc)'\224',(uc)'\225',(uc)'\226',(uc)'\227',
	(uc)'\230',(uc)'\231',(uc)'\232',(uc)'\233',(uc)'\234',(uc)'\235',(uc)'\236',(uc)'\237',
	(uc)'\240',(uc)'\241',(uc)'\242',(uc)'\243',(uc)'\244',(uc)'\245',(uc)'\246',(uc)'\247',
	(uc)'\250',(uc)'\251',(uc)'\252',(uc)'\253',(uc)'\254',(uc)'\255',(uc)'\256',(uc)'\257',
	(uc)'\260',(uc)'\261',(uc)'\262',(uc)'\263',(uc)'\264',(uc)'\265',(uc)'\266',(uc)'\267',
	(uc)'\270',(uc)'\271',(uc)'\272',(uc)'\273',(uc)'\274',(uc)'\275',(uc)'\276',(uc)'\277',
	(uc)'\300',(uc)'\341',(uc)'\342',(uc)'\343',(uc)'\344',(uc)'\345',(uc)'\346',(uc)'\347',
	(uc)'\350',(uc)'\351',(uc)'\352',(uc)'\353',(uc)'\354',(uc)'\355',(uc)'\356',(uc)'\357',
	(uc)'\360',(uc)'\361',(uc)'\362',(uc)'\363',(uc)'\364',(uc)'\365',(uc)'\366',(uc)'\367',
	(uc)'\370',(uc)'\371',(uc)'\372',(uc)'\333',(uc)'\334',(uc)'\335',(uc)'\336',(uc)'\337',
	(uc)'\340',(uc)'\341',(uc)'\342',(uc)'\343',(uc)'\344',(uc)'\345',(uc)'\346',(uc)'\347',
	(uc)'\350',(uc)'\351',(uc)'\352',(uc)'\353',(uc)'\354',(uc)'\355',(uc)'\356',(uc)'\357',
	(uc)'\360',(uc)'\361',(uc)'\362',(uc)'\363',(uc)'\364',(uc)'\365',(uc)'\366',(uc)'\367',
	(uc)'\370',(uc)'\371',(uc)'\372',(uc)'\373',(uc)'\374',(uc)'\375',(uc)'\376',(uc)'\377',
};

static int insensitive_symbols = 0;
static int sensitive_mno = 0;

/* hash tables: symbol and opcode */
static struct hash_control *sym_hash = NULL;
static struct hash_control *op_hash = NULL;

/* assuming max input = 64 */
static int log_table[] = {  0 /*invalid*/,  0, 1, 1, 
                            2 /* log 4 */,  2, 2, 2, 
                            3 /* log 8 */,  3, 3, 3,
                            3,              3, 3, 3,
                            4 /* log 16 */, 4, 4, 4};


/* every error msg due to a parser error is stored here 
   NULL implies no error has occured  */
static char *insn_error;

/* a flag variable - if set, the parser is in 'imm' mode, that is, getting an
   operand of type imm */
static int in_imm_mode = 0;

/* if an operand is of type expr, it will be saved here */
static expressionS ref_expr;

/* linked list to store aliases created with .req directive */
static acaliastype *req_alias = NULL;

/* out_insn is the current instruction being assembled */
static struct insn_encoding
{
  unsigned int image;
  unsigned int format;

  unsigned int is_pseudo;
  unsigned int op_num;
  void * op_pos[9*2];

  acfixuptype *fixup;
} out_insn;

#ifdef FIX_GASEXPERR
static char save_expr = 0;
#endif



/*
  code
*/


void 
md_begin()
{
  int i;
  const char *retval;

  op_hash = hash_new();
  for (i = 0; i < num_opcodes;) {
    const char *t_mnemonic = opcodes[i].mnemonic;

    retval = hash_insert(op_hash, t_mnemonic, (PTR) &opcodes[i]);

    if (retval != NULL) {
      fprintf (stderr, _("internal error: can't hash '%s': %s\n"),
         opcodes[i].mnemonic, retval);
      as_fatal (_("Broken assembler.  No assembly attempted."));
    }

    while ((++i < num_opcodes) && !strcmp(t_mnemonic, opcodes[i].mnemonic));

  }

  /* TODO:
   *  . Desalloc the memory allocated in case of 'insensitive'
   */
  sym_hash = hash_new();
  for (i=0; i < num_symbols; i++) {
    const char *t_symbol = udsymbols[i].symbol;
    char *symtoadd = NULL;

    if (insensitive_symbols) {
      symtoadd  = (char *) malloc(strlen(t_symbol)+1);
      strcpy(symtoadd, t_symbol);
      strtolower(symtoadd);
    }
    else
      symtoadd = (char *)t_symbol;

    retval = hash_insert(sym_hash, (const char *)symtoadd, (PTR) &udsymbols[i]);
    
    if (retval != NULL && strcmp("exists", retval)) {
      fprintf (stderr, _("internal error: can't hash `%s': %s\n"),
         udsymbols[i].symbol, retval);
      as_fatal (_("Broken assembler.  No assembly attempted."));
    }    
  }

  record_alignment(text_section, log_table[AC_WORD_SIZE/8]);

}

#define KEEP_GOING 1
#define STOP_INSN  0

void 
md_assemble(char *str)
{
  dbg_printf(0, "Assembling instruction \"%s\"\n", str);

  static int has_pseudo = 0;
  /* saves the last error ocurried with operands parsing. the algorithm
    will try to reduce the mnemonic size, and if fail, instead of reporting
    "unrecognized mnemonic", it must report the last error ocurried since
    a successful mnemonic hash find */
  char *last_error = NULL;
  
  /* insn we need to encode at this call */
  acasm_opcode *insn = NULL;

  /* we won't change str, so it always point to the start of the insn string.
     's_pos' will be used to parse it instead and points to the position in 
     'str' we are parsing at the moment */
  char *s_pos = str;

  /* pointer to the mnemonic string - 0-ended */
  char *p_mnemonic = sensitive_mno ? original_case_string : str;

  /* if it is a pseudo instruction we always use 'str' because
     'original_case_string' is not filled by pseudo_assemble */
  if (has_pseudo)
    p_mnemonic = str;

  /* a char used to store the value of the char in a string whenever we need 
     to break the original 'str' into substrings adding '\0', so we can
     restore the 'str' to full original string later */
  char save_c = 0;

  /* buffer used to store ArchC argument string and a pointer to scan it */
  char buffer[200];
  char *pbuffer = (char *) buffer;
  char *saved_pos;

  /* advance till first space (this indicates we should have got a mnemonic) 
     or end of string (maybe a mnemonic with no operands) */
  while (*s_pos != '\0' && !ISSPACE (*s_pos)) s_pos++;

 /* Checks for .req directives of the form 
   new_register_name .req existing_register_name */
  if (s_pos != '\0')
    {
      int i;
      char id[4];
      saved_pos = s_pos;

      while(ISSPACE(*s_pos))
	s_pos++;

      for (i = 0; i < 4; i++)
	{
	  id[i] = TOLOWER(*s_pos);
	  s_pos++;
	}

      if (strncmp(id, ".req", 4) == 0)
	{
	  /* If the second string matches ".req", then this is
	   not an instruction, it is a .req directive and we should parse
	  it as a pseudo-op. */
	  ac_parse_req(str);
	  return;
	}
      s_pos = saved_pos;
    }

  insn_error = ""; 

  /* outer loop stop condition       : -parsing was succesful (no errors) (happy end)
                                       -no more mnemonic possibilities (s_pos == str) 
                                            (condition implemented in the inner loop)
     outer loop step: s_pos--
     remember: s_pos points to the end of the mnemonic identification substring (without suffixes)
     in other words: "if there is an error and we are not trying to find a null-sized
      mnemonic, try to solve decrementing the end of the mnemonic substring and fetching
      a new mnemonic entry"

      note: rarely the outer loop will step more than once. its role is to support instruction
        sets whose mnenonics may match with a substring of the combination of other mnemonic
        plus its suffixes, causing the mnemonic finding algorithm to wrongly point the bigger
        one as the true mnemonic. this step (reducing mnemonic size) will correctly find the
        correct one, but may slow the conclusion of a bad instruction error. 
  */
  while (insn_error) {

    insn = NULL;
    /* loops in order to find the mnemonic as a substring among its suffixes */
    /* inner loop stop condition 1 of 2 (opcode found)
       inner loop step: s_pos-- */
    while (insn == NULL) {      
      /* inner loop stop condition 2 of 2 (reached substring size = 0, no more mnemonic possibilities) */
      /* comforming to the algorithm, any errors will end up here.
        (exausted all mnemonic possibilities) */
      if (s_pos == str)
        {
          if (last_error == NULL) {
            /* so we had never reached a succesful mnemonic hash, and we have no idea what is that
              mnemonic */
	          as_bad (_("unrecognized mnemonic: %s"), p_mnemonic);
	          return;
          } else {
            /* report the error when parsing the operands of the last succesful mnemonic substring */
            as_bad(_("%s in insn: '%s'"), last_error, p_mnemonic);
            return;
          }
        }
      /* Prepares to mark the end of the opcode name with \0 in order to call hash_find */
      save_c = *s_pos;
      *s_pos = '\0';

      /* try finding the opcode in the hash table */
      insn = (acasm_opcode *) hash_find (op_hash, p_mnemonic);

      /* done, so restore the original character */
      *s_pos = save_c;
      /* decrement the pointer indicating the character where the opcode name ends 
       (trying to find the original opcode without suffixes appended to it) */
      s_pos--; // next step
    }
  
    /* reverting innerloop step */
    s_pos++;
  
    /* saves end of mnemonic - parsing start position */
    saved_pos = s_pos;
  
    /* main encoding loop */
    for (;;) {     
      dbg_printf(1, "opcode = ");
      dbg_print_insn(0, insn);
  
      /* starts insn encoding */
      strcpy(buffer, insn->args);
      s_pos = saved_pos;
      pbuffer = (char *) buffer;
      clean_out_insn();
      out_insn.image = insn->image;
      out_insn.format = insn->format_id;
      out_insn.is_pseudo = insn->pseudo_idx;
      insn_error = NULL;
  
      /* no support for mnemonic suffixes in pseudo instructions */
      if (insn->pseudo_idx)
      {
        if (*s_pos != '\0')
          pbuffer++;
      }
      else
      {
        dbg_printf(1, "Parsing (mnemonic suffixes) \"%s\" with \"%s\"\n", s_pos, pbuffer);  
  
        if (insensitive_symbols) {
          strtolower(s_pos);
          strtolower(buffer);
        }    
        ac_parse_mnemonic_suffixes(&s_pos, &pbuffer); /* parse the mnemonic suffixes */
        /* try the next insn (overload) if the current failed */
        if (insn_error && (insn+1 < &opcodes[num_opcodes]) && 
            (!strcmp(insn->mnemonic, insn[1].mnemonic))) {
          insn++;
          dbg_printf(1, "\n");
          dbg_printf(1, "Trying another opcode. Error: %s\n", insn_error);
          dbg_printf(1, "\n");
          continue;
        }
        else if (insn_error) break;

        insn_error = NULL;  
      
        /* if mnemonic suffix parsing was successful and there are remaining operands to parse,
         remaining whitespaces in both pbuffer and s_pos exists. */
        if (*pbuffer != '\0') pbuffer++; // eats whitespace
      }
      while (ISSPACE(*s_pos)) s_pos++;

      dbg_printf(1, "Parsing (operands) \"%s\" with \"%s\"\n", s_pos, pbuffer);  
    
      int go_next = ac_parse_operands(s_pos, pbuffer); /* parse the operands */
  
      if (insn_error)
	{
	  /* in the outer loop step, we'll try another mnemonic size. if this search fails, we must
	     report the last error since a succesful mnemonic hash find (correct mnemonic size) */
	  last_error = insn_error;
	}
      if (go_next == STOP_INSN)
        break;
    
      /* try the next insn (overload) if the current failed */
      if (insn_error && (insn+1 < &opcodes[num_opcodes]) && 
          (!strcmp(insn->mnemonic, insn[1].mnemonic))) {
        insn++;
        dbg_printf(1, "\n");
        dbg_printf(1, "Trying another opcode. Error: %s\n", insn_error);
        dbg_printf(1, "\n");
      }
      else break;
    } /* main encoding loop end */

    /* outer loop next step */
    s_pos = saved_pos;
    s_pos--;

  } /* outer loop end */

  if (insn->pseudo_idx) {
    if (has_pseudo != 0) {
      as_bad(_("Pseudo-insn %s called by another pseudo-insn. Not assembled.'\n"), p_mnemonic);
      return;    
    }
    has_pseudo = 1;
    pseudo_assemble(); /* main code to handle pseudo instruction */
    has_pseudo = 0;
  }
  else 
    emit_insn();  /* save the instruction encoded in the the current frag */
}


/* Auxiliary function to help create nodes of the linked list list_op_results, a 
data structure to store information about the operands parsed in case a list operator
is specified */
static node_list_op_results *create_list_results_node(node_list_op_results ** list,
                                               node_list_op_results ** tail, int result)
{
  if (*list == NULL)
  {
    *list = (node_list_op_results *) malloc(sizeof(node_list_op_results));
    *tail = *list;
    (*list)->result = result;
    (*list)->separator = 0;
    (*list)->next = NULL;
    return *list;
  }
  else
  {
    (*tail)->next = (node_list_op_results *) malloc(sizeof(node_list_op_results));
    *tail = (*tail)->next;
    (*tail)->result = result;
    (*tail)->separator = 0;
    (*tail)->next = NULL;
    return *tail;
  }
}

/*
  Operands Parser. Actually, it does a semantic job as well.

  args -> points to the beginning of the argument list (from opcode table) 

  s_pos -> points to the beginning of the insn operand string

  The 'args' string comes from the opcode table generated automatically from an
  ArchC model and tells us how we should expect the syntax of the operands of
  an instruction.
*/  
static 
int ac_parse_operands(char *s_pos, char *args)
{
  /* an ArchC symbol value (gas symbols use expressionS) */
  unsigned int symbol_value; 

  /* a buffer to hold the current conversion specifier found in args */
  char op_buf[30]; 
  char *ob = op_buf;
  
  int is_list = 0; // Is our operand specified as a list of operands? (See list operator)
  list_op_results list_results = NULL; /* Pointer to data structure carrying out parsing
				       information about the list elements in case of list op.*/
  list_op_results list_results_tail = NULL;
  int listloopcontrol = 0; /* Controls wether we need to loop in order to parse more data */

  insn_error = NULL;
  ref_expr.X_op = O_absent;

  /* while argument list is not empty, try parsing the operands */
  while (*args != '\0') { 

    /* a whitespace in the args string means that we need at least one
       whitespace in the s_pos string */
    if (ISSPACE(*args)){
      if (!ISSPACE(*s_pos)) {
        insn_error = "invalid instruction syntax";
        return KEEP_GOING;
      }
      args++;
      s_pos++;
      /* note that it's not possible to have a space as the last char of an
         'args' string, so we dont need to check for '\0' */
    }

    /* eats any space from s_pos so that multiples whitespaces can be handled correctly between
     syntatic elements - the gas pre-processor does this, but we must be sure xD */
    while (ISSPACE(*s_pos)) s_pos++; 

    /* if we've reached the end of insn operand string and not 'args', then
       some operands are missing like: "add" when we were expecting something
       like "add $03,$04,$05" */
    if (*s_pos == '\0') { 
      insn_error = "invalid instruction syntax";
      return KEEP_GOING;
    }

    /*
      '%' is the start of a well-formed sentence. It goes like this:

      '%'<conversion specifier>':'<insn field ID>':<reloc_number>' 
    */
    if (*args == '%') {    
      args++;

      /* save the start of the operand string in case a pseudo is called later */
      out_insn.op_pos[out_insn.op_num++] = (char *) s_pos;

      /* ob points to the conversion specifier buffer - null terminated */
      ob = op_buf;
      while (*args != ':') { 
        *ob = *args;
        ob++; args++;
      }
      *ob = '\0';

      args++;                  /* next literal character/operand */

      unsigned int operand_id = atoi(op_buf);

      dbg_printf(2, "Using operand[%d] to parse \"%s\"\n", operand_id, s_pos);
      dbg_printf(3, "operand[%d] = ", operand_id);
      dbg_print_operand_info(0, operand_id);


      if (operand_id >= num_oper_id)
        INTERNAL_ERROR();

      /* List operator loop. If the operand has a list operator, it will loop until
         there is input satisfying its type. Else, it will run only once.*/
      is_list = operands[operand_id].is_list;
      listloopcontrol = 1;
      
      while (listloopcontrol)
      {
        if (!is_list) /* if this is not a list, parse only once */
          listloopcontrol = 0; 
          
	switch (operands[operand_id].type) {
  
	/* EXP */ 
	  case op_exp:
  
	    if (!get_expression(&ref_expr, &s_pos))
            {
              if (is_list)
                free_list_results(&list_results);
	      return KEEP_GOING;
            }
  
	    switch (ref_expr.X_op) {
	       case O_symbol: {  /* X_add_symbol + X_add_number */
	      bfd_reloc_code_real_type reloc;
	      /* ac_parse_reloc works on input_line_pointer */
	      char *save = input_line_pointer;
	      input_line_pointer = s_pos;
	      if ( *s_pos == '('
		   && (reloc = ac_parse_reloc ()) != BFD_RELOC_UNUSED) {
		s_pos = input_line_pointer;
		input_line_pointer = save;
		if (reloc != `R_'___arch_name___`_PLT)'
		  {
		    insn_error = "only PLT relocations supported on instruction operand";
		    return STOP_INSN;
		  }
		create_fixup(operand_id);
	      }
	      else {
		s_pos = input_line_pointer;
		input_line_pointer = save;
		create_fixup(operand_id);
	      }
	      }
	      break;
	      
		  
	      case O_constant: /* X_add_number */
	        if (!is_list)
		  encode_field(ref_expr.X_add_number, operand_id, NULL);
		else
		  create_list_results_node(&list_results, &list_results_tail,
		                           ref_expr.X_add_number);
		break;
  
	      case O_uminus:          /* (- X_add_symbol) + X_add_number.  */
	      case O_bit_not:         /* (~ X_add_symbol) + X_add_number.  */
	      case O_logical_not:     /* (! X_add_symbol) + X_add_number.  */
	      case O_multiply:        /* (X_add_symbol * X_op_symbol) + X_add_number.  */
	      case O_divide:          /* (X_add_symbol / X_op_symbol) + X_add_number.  */
	      case O_modulus:         /* (X_add_symbol % X_op_symbol) + X_add_number.  */
	      case O_left_shift:      /* (X_add_symbol << X_op_symbol) + X_add_number. */
	      case O_right_shift:     /* (X_add_symbol >> X_op_symbol) + X_add_number. */
	      case O_bit_inclusive_or:/* (X_add_symbol | X_op_symbol) + X_add_number.  */
	      case O_bit_or_not:      /* (X_add_symbol |~ X_op_symbol) + X_add_number. */
	      case O_bit_exclusive_or:/* (X_add_symbol ^ X_op_symbol) + X_add_number.  */
	      case O_bit_and:         /* (X_add_symbol & X_op_symbol) + X_add_number.  */
		insn_error = "operation not supported with relocatable symbol";
		if (is_list)
                  free_list_results(&list_results);
		return STOP_INSN;
		break;
  
	    default:
		insn_error = "invalid expression";
		if (is_list)
                  free_list_results(&list_results);
		return KEEP_GOING;
	    } /* end of switch(ref_expr.X_op) */
	    break; 
  
  
	  case op_imm:
  
	    in_imm_mode = 1;
	    if (!get_immediate(&ref_expr, &s_pos)) {
	      in_imm_mode = 0;
	      if (is_list)
                free_list_results(&list_results);
	      return KEEP_GOING;
	    }
	    in_imm_mode = 0;
  
	    dbg_printf(3, "Immediate value: %ld\n", ref_expr.X_add_number);
	    
            if (!is_list)
	      encode_field(ref_expr.X_add_number, operand_id, NULL);
	    else
	      create_list_results_node(&list_results, &list_results_tail,
		                           ref_expr.X_add_number);
  
	    break;
  
	  case op_addr:
  
	    if (!get_address(&ref_expr, &s_pos)) return KEEP_GOING;
  
	    if (ref_expr.X_op != O_symbol) {    // X_add_symbol + X_add_number
	      insn_error = "invalid operand, expected a symbolic address";
	      return KEEP_GOING;
	    }
	      if (!is_list) {
	      bfd_reloc_code_real_type reloc;
	      char *save = input_line_pointer;
	      input_line_pointer = s_pos; /* ac_parse_reloc works on input_line_pointer */
	      if ( *s_pos == '('
		   && (reloc = ac_parse_reloc ()) != BFD_RELOC_UNUSED) {
		s_pos = input_line_pointer;
		input_line_pointer = save;
		if (reloc != `R_'___arch_name___`_PLT)'
		  {
		    insn_error = "only PLT relocations supported on instruction operand";
		    return STOP_INSN;
		  }
		create_fixup(operand_id);
	      }
	      else {
		s_pos = input_line_pointer;
		input_line_pointer = save;
		create_fixup(operand_id);
	      }
	    }
	    else {
	      // assembler is malformed, cannot have list operator with relocatable elements!
	      INTERNAL_ERROR();	
            }
  
	    break;
  
	  case op_userdef:
  
	    if (!get_userdefoper(&symbol_value, operands[operand_id].name, &s_pos)) 
            {
              if (is_list)
                free_list_results(&list_results);
	      return KEEP_GOING;
            }
  
  
	  /* NEVER!!! discomment the line below O.O */
	  /* if (out_insn.is_pseudo) return; */
  
	  /* if we have reached here, we sucessfully got a valid symbol's value; so we encode it */
	    if (!is_list)
              encode_field(symbol_value, operand_id, NULL);
            else
              create_list_results_node(&list_results, &list_results_tail,
		                           symbol_value);
  
	    break;
  
	  default:
	    INTERNAL_ERROR();
  
	} /* end of switch*/
	
	/* processes some list-specific parsing features */
	if (is_list)
        {
          while (ISSPACE(*s_pos)) s_pos++;
          if(*s_pos == ',') {
            list_results_tail->separator = ',';
            s_pos++;
          }
          else if (*s_pos == '-') {
            list_results_tail->separator = '-';
            s_pos++;
          }
          else 
          {
            listloopcontrol = 0; /* the list has ended, nothing more to parse*/
            /* Encode the list parsed */
            encode_field(symbol_value, operand_id, list_results);
            free_list_results(&list_results);
            list_results = NULL;
            list_results_tail = NULL;
          }
          while (ISSPACE(*s_pos)) s_pos++;
        }
      }/* end of while (listloopcontrol) */

      out_insn.op_pos[out_insn.op_num++] = (char *)s_pos;

    } /* end of if (*args == '%') */
    else {  
      /* args string must equal s_pos string in case its not a '%' */    

      /* scape */
      if (*args == '\\') {
        args++;   
      }

      if (*s_pos != *args) {  
        insn_error = "invalid instruction syntax";
        return KEEP_GOING;
      }
      args++;
      s_pos++;

    }
    
  } /* end while */

  if (*s_pos != '\0') {  /* have add $3,$2,$2[more] - expected add $3,$2,$2 */
    insn_error = "invalid instruction syntax";
    return KEEP_GOING;
  }

  return KEEP_GOING;
}

/* 
  Similar to ac_parse_operands (Operands parser), but this version is limited to
  user defined operands (the only ones permitted as mnemonic suffixes) without
  ignoring whitespaces, like the operands parser.

  In fact, the mnemonic suffix must not contain any whitespace, and a recursive
  algorithm is used (splitting the suffix) in order to try to guess a valid parse.
 */
static 
int ac_parse_mnemonic_suffixes(char **s_pos, char **args)
{
  /* an ArchC symbol value (gas symbols use expressionS) */
  unsigned int symbol_value; 

  /* a buffer to hold the current conversion specifier found in args */
  char op_buf[30]; 
  char *ob = op_buf;

  insn_error = NULL;

  /* while mnemonic suffix list is not empty, try parsing the operands */
  while (**args != '\0' && !ISSPACE(**args)) { 

    /*      '%' is the start of a well-formed sentence.    */
    if (**args == '%') {    
      (*args)++;

      /* ob points to the conversion specifier buffer - null terminated */
      ob = op_buf;
      while (**args != ':') { 
        *ob = **args;
        ob++; (*args)++;
      }
      *ob = '\0';

      (*args)++;                  /* next literal character/operand */

      unsigned int operand_id = atoi(op_buf);

      dbg_printf(2, "Using operand[%d] to parse mnemonic suffix \"%s\"\n", operand_id, *s_pos);
      dbg_printf(3, "operand[%d] = ", operand_id);
      dbg_print_operand_info(0, operand_id);


      /* As mnemonic suffixes, exp addr and imm aren't permitted */
      if (operand_id >= num_oper_id ||
          operands[operand_id].type == op_exp ||
          operands[operand_id].type == op_addr ||
          operands[operand_id].type == op_imm)
        INTERNAL_ERROR();

      if (operands[operand_id].type == op_userdef)
      {
        char saved_char = '\0';
        char *saved_s_pos = NULL;
        char *saved_args = NULL;
        char *suffix_split = *s_pos;
        char *s_pos_begin = *s_pos;

        /* suffix_split = end of mnemonic suffix (first space or end of string) 
          s_pos_begin to suffix_split will be parsed here
          suffix_split until the end will be parsed in a recursive call */
        while (!ISSPACE(*suffix_split) && *suffix_split != '\0')
          suffix_split++;
  
        /* loop stop condition: suffix_split == s_pos_begin (could not parse without discarding current arg) */
        while (suffix_split != s_pos_begin) {  
          saved_char = *suffix_split;
          *suffix_split = '\0';
          saved_s_pos = *s_pos;
          get_userdefoper(&symbol_value, operands[operand_id].name, s_pos);
          if (saved_char != '\0')
            *suffix_split = saved_char;
          if (saved_s_pos == *s_pos) {            
            suffix_split--;
            continue;
          }
          saved_args = *args;
          ac_parse_mnemonic_suffixes(s_pos, args);
          if (!insn_error)
            break;
          insn_error = NULL;
          *s_pos = saved_s_pos;
          *args = saved_args;
          suffix_split--;
        }
        
        /* if couldn't parse considering current arg, check if it is a mandatory field (hasn't "" value entry) */
        if (suffix_split == s_pos_begin) {
          if (!get_userdefoper(&symbol_value, operands[operand_id].name, s_pos)) {
            insn_error = "unrecognized mnemonic";
            return KEEP_GOING; //failed
          }
          /* if field is not mandatory, try to parse the rest with remaining args */
          saved_s_pos = *s_pos;
          saved_args = *args;                    
          ac_parse_mnemonic_suffixes(s_pos, args);
          if (insn_error){
            *s_pos = saved_s_pos;
            *args = saved_args;
            return KEEP_GOING; //failed
          }
        }

        /* if we have reached here, we sucessfully got a valid symbol's value; so we encode it */
        encode_field(symbol_value, operand_id, NULL);
      }
      else
          INTERNAL_ERROR();

      out_insn.op_pos[out_insn.op_num++] = (char *)*s_pos;

    } /* end of if (*args == '%') */
    else {  
      /* args string must equal s_pos string in case its not a '%' */    

      /* scape */
      if (**args == '\\') {
        (*args)++;   
      }

      if (**s_pos != **args) {  
        insn_error = "unrecognized mnemonic";
        return KEEP_GOING;
      }
      (*args)++;
      (*s_pos)++;

    }
    
  } /*end while */

  /* algorithm stop condition: a whitespace in the args string means that there are no more
      suffixes to parse */
  if (ISSPACE(**args)){
    /* if the suffixes in string are fully parsed, it must contain a whitespace too */
    if (!ISSPACE(**s_pos)) {
      insn_error = "unrecognized mnemonic";
      return KEEP_GOING;
    }
    return KEEP_GOING; // job finished!
  }

  return KEEP_GOING;
}


static void pseudo_assemble() {

  char op_string[9][50];
  unsigned int i;
  unsigned int j;

  /* extract the operand strings */
  for (i=0; i<out_insn.op_num/2; i++) {
     
    unsigned int str_count = (((char *)out_insn.op_pos[i*2+1]) - ((char *)out_insn.op_pos[i*2]));
    if (str_count >= 50) 
      INTERNAL_ERROR(); /* out buffer is too small */

    for (j=0; j<str_count; j++) 
    {      
      op_string[i][j] = *(char *)(out_insn.op_pos[i*2]+j);      
    }

    op_string[i][str_count] = '\0';

  }

  /* from now on the code must be reentrant (recursive calls to md_assemble) */
  int num_operands = (unsigned int) out_insn.op_num/2;
  int pseudo_ind = out_insn.is_pseudo;
  if (pseudo_ind > num_pseudo_instrs) INTERNAL_ERROR();

  char new_insn[50];

  while (pseudo_instrs[pseudo_ind] != NULL) {
    const char *pseudoP = pseudo_instrs[pseudo_ind];
    char *n_ind = new_insn;

    while (*pseudoP != '\0') {
      
      if (*pseudoP == '\\') 
         pseudoP++;
      else if (*pseudoP == '%') {
       pseudoP++;
       if ((*pseudoP < '0') || (*pseudoP > '9') || (*pseudoP-'0' >= num_operands)) 
         INTERNAL_ERROR();

       strcpy(n_ind, op_string[*pseudoP-'0']);
       n_ind += strlen(op_string[*pseudoP-'0']);

       pseudoP++;
       continue;
      }

      *n_ind = *pseudoP;
      n_ind++;
      pseudoP++;

    }
    *n_ind = '\0';
      
    md_assemble(new_insn);

    pseudo_ind++;
  }
}


static void emit_insn() 
{
  /* frag address where we emit the insn encoding (call frag_more() to get the address) */
    char *frag_address;
  
  dbg_printf(1, "Emitting image 0x%08X (%ld bits) at address 0x%08lX\n", 
                out_insn.image, 
                get_insn_size(out_insn.format), 
                frag_now_fix());

  frag_address = frag_more(get_insn_size(out_insn.format)/8);

  md_number_to_chars(frag_address, out_insn.image, get_insn_size(out_insn.format)/8);

  dbg_printf(2, "Frag info: addr = %ld, fix = %ld, var = %ld\n", 
                  frag_now->fr_address,
                  frag_now->fr_fix,
                  frag_now->fr_var);

  while (out_insn.fixup != NULL) {

    fixS *fixP;

    ref_expr = out_insn.fixup->ref_expr_fix;

    if (ref_expr.X_op != O_symbol)
      INTERNAL_ERROR();
    
    fixP = fix_new_exp (frag_now, frag_address - frag_now->fr_literal /* where */, get_insn_size(out_insn.format)/8 /* size */,
    &ref_expr, 0 /* pcrel -> always FALSE */, operands[out_insn.fixup->operand_id].reloc_id);
    
    if (!fixP)
      INTERNAL_ERROR();

    fixP->tc_fix_data = out_insn.fixup->operand_id;

    /* don't let GAS core mess with our fixup (adjust_reloc_syms) */
    fixP->fx_done = TRUE;

    acfixuptype *p = out_insn.fixup;
    out_insn.fixup = out_insn.fixup->next;
    free(p); 

    dbg_printf(1, "gas fixS created:\n");
    dbg_print_gasfixup(2, fixP);
  }

}



/*

  Function used to write 'val' in 'buf' using 'n' bytes with a correct machine endian.
  Usually it's called to emit instructions to a frag.

*/
void
md_number_to_chars(char *buf, valueT val, int n)
{
#ifdef AC_BIG_ENDIAN
    number_to_chars_bigendian (buf, val, n);  
#else
    number_to_chars_littleendian (buf, val, n);
#endif
}



/*

  Called by fixup_segment() (write.c) to apply a fixup to a frag.

  fixP -> a pointer to the fixup
  valP -> a pointer to the value to apply (with symbol value added)
  seg  -> segment which the fix is attached to

*/
void
md_apply_fix3 (fixS *fixP ATTRIBUTE_UNUSED, valueT *valP ATTRIBUTE_UNUSED, segT seg ATTRIBUTE_UNUSED)
{

  INTERNAL_ERROR(); // should never get called
  
}


int
___arch_name___`_validate_fix'(struct fix *fixP ATTRIBUTE_UNUSED, asection *seg ATTRIBUTE_UNUSED)
{
  dbg_printf(0, "Validating fix\n");
  dbg_print_gasfixup(1, fixP);

  if (fixP->fx_addsy == NULL)
    INTERNAL_ERROR();

  resolve_symbol_value(fixP->fx_addsy);
  symbol_mark_used_in_reloc(fixP->fx_addsy);

  if (fixP->fx_subsy != NULL) {
    resolve_symbol_value(fixP->fx_subsy);

    fixP->fx_offset -= S_GET_VALUE(fixP->fx_subsy);
    fixP->fx_offset += fixP->fx_frag->fr_address / OCTETS_PER_BYTE;

    switch (fixP->fx_r_type) {
      case `R_'___arch_name___`_8:'
        fixP->fx_r_type = `R_'___arch_name___`_REL8;' 
        break;
      case `R_'___arch_name___`_16:'
        fixP->fx_r_type = `R_'___arch_name___`_REL16;' 
        break;
      case `R_'___arch_name___`_32:'
        fixP->fx_r_type = `R_'___arch_name___`_REL32;' 
        break;

      default:
        INTERNAL_ERROR();
    }
  }

  fixP->fx_done = FALSE;

  return 0;
}


void
___arch_name___`_adjust_reloc_count'(struct fix *fixP, long seg_reloc_count)
{
  return;

  /* NOTE: seg_reloc_count is actually not used in BFD_ASSEMBLER mode */
  for (; fixP; fixP = fixP->fx_next) {

    if (fixP->fx_done) 
      continue;

    ++seg_reloc_count;

    if (fixP->fx_addsy == NULL)
      fixP->fx_addsy = abs_section_sym;

    symbol_mark_used_in_reloc (fixP->fx_addsy);

    if (fixP->fx_subsy != NULL)
      symbol_mark_used_in_reloc (fixP->fx_subsy);
  }
}



/*

  Called by write_relocs() (write.c) for each fixup so that a BFD arelent structure can be build
and returned do be applied through 'bfd_install_relocation()' which in turn will call a backend
routine to apply the fix. 
*/
arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixp)
{
  arelent *rel;
  bfd_reloc_code_real_type r_type;

  rel = (arelent *) xmalloc (sizeof (arelent));
  rel->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *rel->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  rel->address = fixp->fx_frag->fr_address + fixp->fx_where;

  r_type = fixp->fx_r_type;
  //rel->addend = fixp->fx_addnumber;
  rel->addend = fixp->fx_offset;
//  rel->addend = fixp->tc_fix_data.addend;
  rel->howto = bfd_reloc_type_lookup (stdoutput, r_type);

  if (rel->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
        _("Cannot represent relocation type %s"),
        bfd_get_reloc_code_name (r_type));
      /* Set howto to a garbage value so that we can keep going.  */
      rel->howto = bfd_reloc_type_lookup (stdoutput, BFD_RELOC_32);
      assert (rel->howto != NULL);
    }

  return rel;
}

char *
___arch_name___`_canonicalize_symbol_name'(char *c)
{
  if (insensitive_symbols) {
    strtolower(c);
  }

  return c;  
}

void
___arch_name___`_handle_align' (fragS *fragp  ATTRIBUTE_UNUSED)
{
/* We need to define this function so that in write.c, in routine 
 subsegs_finish, the variable alignment get the right size and the last frag 
 can be align.  
   I think there is another way to handle the alignment stuff without defining
this function (which is not mandatory). We just need to make md_section_align check 
the bfd alignment, and return the next aligned address. I've not tested that tho ;)
 */
}

void
___arch_name___`_symbol_new_hook' (symbolS *sym ATTRIBUTE_UNUSED)
{
  dbg_printf(0, "Symbol Created!!!\n%*s", 4, "");
  dbg_print_symbolS(1, sym);
}





/*
   Called (by emit_expr()) when symbols might get their values taken like:
   .data
   .word label2
*/
extern void 
___arch_name___`_cons_fix_new'(struct frag *frag, int where, unsigned int nbytes, struct expressionS *exp)
{
  fixS *fixP = NULL;

  switch (nbytes) {
    case 1:
      fixP = fix_new_exp (frag, where, (int) nbytes, exp, 0, `R_'___arch_name___`_8');
      break;
    case 2:
      fixP = fix_new_exp (frag, where, (int) nbytes, exp, 0, `R_'___arch_name___`_16');
      break;
    case 4:
      fixP = fix_new_exp (frag, where, (int) nbytes, exp, 0, `R_'___arch_name___`_32');
      break;
    default:
      INTERNAL_ERROR();
  }

 
  fixP->fx_done = FALSE;


  if (!fixP) INTERNAL_ERROR();
}
 
/* ---------------------------------------------------------------------------------------
 Static functions
*/
 

/* ac_parse_reloc() is used to parse special symbol modifiers, parenthesis next to a symbol
  name with "got", "gotoff" or "plt" markers to flag the use of a different relocation
  for this symbol. ac_parse_reloc() uses input_line_pointer to parse these constructs
  and returns the apropriate relocation type for this symbol.

  If no special relocation marker is found, it returns BFD_RELOC_UNUSED.   

  This code was based on gas/tc-arm.c, the binutils arm model.
*/
static bfd_reloc_code_real_type
ac_parse_reloc ()
{
  char         id [16];
  char *       ip;
  unsigned int i;
  static struct
  {
    char * str;
    int    len;
    bfd_reloc_code_real_type reloc;
  }
  reloc_map[] =
  {
#define MAP(str,reloc) { str, sizeof (str) - 1, reloc }
    MAP ("(got)",    `R_'___arch_name___`_GOT),'
    MAP ("(gotoff)", `R_'___arch_name___`_GOTOFF),'
    MAP ("(plt)",    `R_'___arch_name___`_PLT),'
    { NULL, 0,         BFD_RELOC_UNUSED }
#undef MAP
  };

  for (i = 0, ip = input_line_pointer;
       i < sizeof (id) && (ISALNUM (*ip) || ISPUNCT (*ip));
       i++, ip++)
    id[i] = TOLOWER (*ip);

  for (i = 0; reloc_map[i].str; i++)
    if (strncmp (id, reloc_map[i].str, reloc_map[i].len) == 0)
      break;

  input_line_pointer += reloc_map[i].len;

  return reloc_map[i].reloc;
}

 /* Handles pseudo-ops like .word, etc. */

static void
s_cons (int byte_size)
{
  expressionS exp;

  if (byte_size > 1) {
    frag_align(log_table[byte_size], 0, 0);
    record_alignment(now_seg, log_table[byte_size]);
  }

  /* Start of code similar to cons_worker() in read.c 
     s_cons used to call it instead of replicating its code,
     but to support GOT/PLT relocations,
     we need to modify the expression parsing logic */
#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  /* No parameters to our constructor pseudo-operator, so we silently return */
  if (is_it_end_of_statement ())
    {
      demand_empty_rest_of_line ();
      return;
    }

#ifdef md_cons_align
  md_cons_align (byte_size);
#endif

  /* Parameter parsing loop, iterates through each expression separated by comma */
  do
    {
      bfd_reloc_code_real_type reloc;

      expression (& exp);

      if (exp.X_op == O_symbol
	  && * input_line_pointer == '('
	  && (reloc = ac_parse_reloc ()) != BFD_RELOC_UNUSED)
	{
	  reloc_howto_type *howto = bfd_reloc_type_lookup (stdoutput, reloc);
	  int size = bfd_get_reloc_size (howto);

	  if (size > byte_size)
	    as_bad ("%s relocations do not fit in %d bytes",
		    howto->name, byte_size);
	  else
	    {
	      char *p = frag_more ((int) byte_size);
	      int offset = byte_size - size;

	      fix_new_exp (frag_now, p - frag_now->fr_literal + offset, size,
			   &exp, 0, reloc);
	    }
	}
      else
	emit_expr (&exp, (unsigned int) byte_size);
    }
  while (*input_line_pointer++ == ',');

  /* Put terminator back into stream.  */
  input_line_pointer --;
  demand_empty_rest_of_line ();
}

static void
s_bss (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  subseg_set (bss_section, 0);
  demand_empty_rest_of_line ();
}
 

static int
get_expression (expressionS *ep, char **str)
{
  char *save_in;

  save_in = input_line_pointer;

  input_line_pointer = *str;
#ifdef FIX_GASEXPERR
  save_expr = **str;
#endif
  expression (ep);

#ifdef DEBUG_ON
  char save_char = *input_line_pointer;
  *input_line_pointer = '\0';
#endif
  dbg_printf(3, "Expression string: \"%s\"\n%*s", *str, 4*4, "");
  dbg_print_expression(4, ep);
#ifdef DEBUG_ON
  *input_line_pointer = save_char;
#endif

  *str = input_line_pointer;

  input_line_pointer = save_in;
  
  if (insn_error) return 0;
  return 1;
}


static int
get_immediate (expressionS *ep, char **str)
{
  char *save_in;

  save_in = input_line_pointer;

  input_line_pointer = *str;

  ep->X_op = O_constant;
  ep->X_add_number = get_single_number();

#ifdef DEBUG_ON
  char save_char = *input_line_pointer;
  *input_line_pointer = '\0';
#endif
  dbg_printf(3, "Immediate string: \"%s\"\n", *str);
#ifdef DEBUG_ON
  *input_line_pointer = save_char;
#endif

  *str = input_line_pointer;

  input_line_pointer = save_in;

  if (insn_error) return 0;
  return 1;
}

static int
get_address (expressionS *ep, char **str)
{
  char *save_in;
 
  save_in = input_line_pointer;
 
  input_line_pointer = *str;
  expression (ep);
 
#ifdef DEBUG_ON
  char save_char = *input_line_pointer;
  *input_line_pointer = '\0';
#endif
  dbg_printf(3, "Address string: \"%s\"\n%*s", *str, 4*4, "");
  dbg_print_expression(4, ep);
#ifdef DEBUG_ON
  *input_line_pointer = save_char;
#endif

  *str = input_line_pointer;

  input_line_pointer = save_in;
  
  if (insn_error) return 0;

  return 1;
}

static int 
get_userdefoper(unsigned int *val, const char *opclass, char **str)
{
  char *s_pos = *str;

  if (*s_pos == '\0') { 
    /* Verifies if opclass can be ignored (can assume "") */
    if (!ac_symbol_dispensable(opclass, val)) {
      insn_error = "operand missing";
      return 0;
    } else {
      dbg_printf(3, "\"\" = %u\n", *val);
      return 1; // Operand ignored, exit without update str
    }
  }
 
  /* get the operand string (sync with ac_asm_map identifiers) */
  if ( (*s_pos >= 'a' && *s_pos <= 'z') ||
       (*s_pos >= 'A' && *s_pos <= 'Z') ||
       (*s_pos >= '0' && *s_pos <= '9') ||
       (*s_pos == '%' || *s_pos == '!' ||
        *s_pos == '@' || *s_pos == '#' ||
        *s_pos == '$' || *s_pos == '&' ||
        *s_pos == '*' || *s_pos == '-' ||
        *s_pos == '+' || *s_pos == '=' ||
        *s_pos == '|' || *s_pos == ':' ||
        *s_pos == '<' || *s_pos == '>' ||
        *s_pos == '^' || *s_pos == '~' ||
        *s_pos == '?' || *s_pos == '/') ||
        *s_pos == ',' || *s_pos == '_')
    s_pos++;
  else {
    /* Verifies if opclass can be ignored (can assume "") */
    if (!ac_symbol_dispensable(opclass, val)) {
      insn_error = "unrecognized operand symbol";
      return 0;
    } else {
      dbg_printf(3, "\"\" = %u\n", *val);
      return 1; // Operand ignored, exit without update str
    }
  }
  
  while ( (*s_pos >= 'a' && *s_pos <= 'z') ||
          (*s_pos >= 'A' && *s_pos <= 'Z') ||
          (*s_pos >= '0' && *s_pos <= '9') ||
           *s_pos == '.' || *s_pos == '_' )
    s_pos++;

  char save_c = *s_pos;
  *s_pos = '\0';
 
  if (!ac_valid_symbol(opclass, *str, val)) {
    /* Verifies if opclass can be ignored (can assume "") */
    if (!ac_symbol_dispensable(opclass, val)) {
      insn_error = "unrecognized operand symbol";
      *s_pos = save_c;
      return 0;
    } else {
      dbg_printf(3, "\"\" = %u\n", *val);
      *s_pos = save_c;
      return 1; // Operand ignored, exit without update str
    }
  }

  dbg_printf(3, "\"%s = %u\"\n", *str, *val);
 
  *s_pos = save_c;
  *str = s_pos;

  return 1;
}


/* 
  Checks if parser_symbol "" is valid with a specified ac_symbol
*/
static int ac_symbol_dispensable(const char *marker, unsigned int *value)
{
  acasm_symbol *msymb = NULL;

  if (marker == NULL || strcmp(marker, "") == 0)
    INTERNAL_ERROR();

  msymb = (acasm_symbol *) hash_find (sym_hash, "");

  if (msymb == NULL) {
    return 0;
  }

  while (strcmp(msymb->cspec, marker)!=0 && (msymb+1 < &udsymbols[num_symbols]) &&
      strcmp(msymb[1].symbol, "")==0)
  {
    msymb++;
  }
  
  if (strcmp(msymb->cspec, marker)==0 && strcmp(msymb->symbol, "")==0)
  {
    *value = msymb->value;
    return 1;
  }

  return 0;
}



static int ac_valid_symbol(const char *ac_symbol, char *parser_symbol, unsigned int *value)
{
  acasm_symbol *msymb = NULL;
  acaliastype *p;
  
  if (ac_symbol == NULL)
    INTERNAL_ERROR();
  
  msymb = (acasm_symbol *) hash_find (sym_hash, parser_symbol);

  // symbol not found
  if (msymb == NULL)
    {
      /* symbol not found, check for aliases */
      p = req_alias;
      while (p != NULL)
	{
	  if (strncmp(p->new_name, parser_symbol, strlen(p->new_name))==0)
	    break;
	  p = p->next;
	}
      if (p != NULL)
	{
	  /* in order to avoid infinite looping here, req_alias linked list
	     cannot contain any illegal aliases (new_name referencing to an
	     existing symbol name). this checking is done in .req directive parsing. */
	  dbg_printf(4, "Operand \"%s\" was not found. Trying its alias \"%s\"\n", parser_symbol, p->old_name);
	  return ac_valid_symbol(ac_symbol, p->old_name, value);
	}
      return 0;
    }
  
  if (strcmp(ac_symbol, "")==0) // any map marker will do
  {
    *value = msymb->value;
    return 1;
  }
  else               // specific map marker is supplied
  {
    while (strcmp(msymb->cspec, ac_symbol)!=0 && (msymb+1 < &udsymbols[num_symbols]) &&
        strcmp(msymb[1].symbol, parser_symbol)==0)
    {
      msymb++;
    }
    
    if (strcmp(msymb->cspec, ac_symbol)==0 && strcmp(msymb->symbol, parser_symbol)==0)
    {
      *value = msymb->value;
      return 1;
    }
  
    return 0;
  }
}




//-------------------------------------------------------------------------------------
// Necessary stuff
//-----------------------



/*
--------------------------------------------------------------------------------------
 Command-line parsing stuff


  'void parse_args(int *, char ***)' in <as.c> is the main routine called to parse command line options for the main GAS
 program. The machine dependent (md) part might extend the short and long options of the main GAS ones by means of these 
 variables: 

 (obrigatory variables)
    const char *md_shortopts  (short options)
    struct option md_longopts (long options)
    size_t md_longopts_size  (this must be set to the long options' size)

   Two other routines *MUST* be set:

 (obrigatory)
   int md_parse_option(int c, char *arg)
      'c' may be a character (in case of a short option) or a value in the 'val' field of a long option structure. If 'arg'
      is not NULL, then it holds a string related somehow to 'c' (an argument to 'c'). Strictly speaking, 'c' is the value
      returned by a call to the getopt_long_only(...) by parse_args.
      Return 0 to indicate the option 'c' was not recognized or !0 otherwise.

 (obrigatory)
   md_show_usage (FILE *stream)
      This is called by the main GAS show_usage routine and should display this assembler machine dependent options in 
      'stream'

   Still, there might be defined one more routine to do special parsing handling or md setting right after the options 
      parsing (its called at the end of 'parse_args' from <as.c>):

 (optional)
   void md_after_parse_args ()
--------------------------------------------------------------------------------------
*/
const char *md_shortopts = "is";

struct option md_longopts[] =
{
  {"insensitive-syms", no_argument, NULL, OPTION_MD_BASE},
  {"sensitive-mno", no_argument, NULL, OPTION_MD_BASE+1},
  {"archc", no_argument, NULL, OPTION_MD_BASE+2} 
};
size_t md_longopts_size = sizeof (md_longopts);


static void
archc_show_information()
{
  fprintf (stderr, _("GNU assembler automatically generated by acbinutils 2.2.\n\
Architecture name: ___arch_name___.\n"));
}


int
md_parse_option (int c, char *arg ATTRIBUTE_UNUSED)
{
  switch (c) 
  {
    case OPTION_MD_BASE+0:
    case 'i':
      insensitive_symbols = 1;
      break;
    
    case OPTION_MD_BASE+1:
    case 's':
      sensitive_mno = 1;
      break;
     
    case OPTION_MD_BASE+2: /* display archc version information; */  
      archc_show_information();
      exit (EXIT_SUCCESS); 
      break;
     
    default:
      return 0;
  }

  return 1;
}


void
md_show_usage (FILE *stream)
{
  fprintf (stream, "md options:\n\n");

  fprintf (stream, _("\
  -i,--insensitive-syms   use case-insensitive symbols\n"));

  fprintf (stream, _("\
  -s,--sensitive-mno      use case-sensitive mnemonic strings\n"));
  
  fprintf (stream, _("\
  --archc                 display ArchC information\n"));
}
/*---------------------------------------------------------------------------*/


char *
md_atof (type, litP, sizeP)
     int type ATTRIBUTE_UNUSED;
     char *litP ATTRIBUTE_UNUSED;
     int *sizeP ATTRIBUTE_UNUSED;
{
  return NULL;
}


/* Convert a machine dependent frag.  */
void
md_convert_frag (abfd, asec, fragp)
     bfd *abfd ATTRIBUTE_UNUSED;
     segT asec ATTRIBUTE_UNUSED;
     fragS *fragp ATTRIBUTE_UNUSED;
{
  return;
}


valueT
md_section_align (seg, addr)
     asection *seg ATTRIBUTE_UNUSED;
     valueT addr;
{
  return addr;
}


int
md_estimate_size_before_relax (fragp, segtype)
     fragS *fragp ATTRIBUTE_UNUSED;
     asection *segtype ATTRIBUTE_UNUSED;
{
  return 0;
}


long
md_pcrel_from (fixP)
     fixS *fixP ATTRIBUTE_UNUSED;
{
  /* should not be called */
  INTERNAL_ERROR();

//  if (fixP->fx_addsy == NULL)
//    INTERNAL_ERROR();

//  return fixP->fx_where + fixP->fx_frag->fr_address + fixP->tc_fix_data.pcrel_add;
}

symbolS *md_undefined_symbol (char *name ATTRIBUTE_UNUSED)
{
  return NULL;
}

void md_operand (expressionS *expressionP)
{
  while (*input_line_pointer != '\0') input_line_pointer++;

  insn_error = "bad expression";
  expressionP->X_op = O_constant;
}


int ___arch_name___`_parse_name'(char *name, expressionS *expP, char *c ATTRIBUTE_UNUSED) {

  unsigned int dummy;

  if (in_imm_mode) { /* no name allowed when getting an 'imm' operand */
    insn_error = "invalid operand, expected a number";

    expP->X_op = O_absent;       /* some machines crash without these lines */
    expP->X_add_symbol = NULL; 
    return 1;
  }

  if (ac_valid_symbol("", name, &dummy)) { /* symbol found */
#ifdef FIX_GASEXPERR
    if (save_expr == '[' || save_expr == '(')
      {
	*input_line_pointer = *c;
	char *str_p = input_line_pointer;
	while (*str_p != '\0')
	  {
	    if ((save_expr == '[' && *str_p == ']')
		|| (save_expr == '(' && *str_p == ')'))
	      break;
	    str_p++;
	  }
	if (*str_p == '\0')
	 {
	   insn_error = "invalid symbol in expression";
	   return 1;
	 }
	else 
	 {
	    input_line_pointer = str_p;
	    *c = *str_p;	    
	    insn_error = "invalid symbol in expression";
	    return 1;
	 }
      }
#endif
    expP->X_op = O_absent;
    insn_error = "invalid symbol in expression";
    return 1;
  }

  return 0;
}

void ___arch_name___`_frob_label'(symbolS *sym) {

  unsigned int dummy;

  dbg_printf(0, "Label Created!!!\n%*s", 4, "");
  dbg_print_symbolS(1, sym);

  if (ac_valid_symbol("",  (char *)S_GET_NAME(sym), &dummy))  /* symbol found as label -.- */
    as_warn(_("symbol name used as label may create lead to ambiguous code: '%s'"), S_GET_NAME(sym));
  
}



static void 
create_fixup(unsigned int oper_id) 
{
  if (out_insn.is_pseudo) return;

  acfixuptype *fixups;
  
  if (out_insn.fixup == NULL)
  {
    out_insn.fixup = (acfixuptype *) malloc(sizeof(acfixuptype));
    out_insn.fixup->next = NULL;  
    fixups = out_insn.fixup;
  }
  else {
    fixups = out_insn.fixup;
    while (fixups->next != NULL) fixups = fixups->next;
    fixups->next = (acfixuptype *) malloc(sizeof(acfixuptype));
    fixups = fixups->next;
    fixups->next = NULL;
  }

  fixups->ref_expr_fix = ref_expr;
  fixups->operand_id = oper_id;
  
  dbg_printf(3,"Fixup = ");
  dbg_print_fixup(0, fixups);
}

static void 
encode_field(unsigned int val_const, unsigned int oper_id, list_op_results lr)
{
  mod_parms mp;
  char secname[] = "no symbol";

  if (out_insn.is_pseudo) return;

  mp.input   = val_const;
  mp.address = frag_now_fix();
  mp.section = secname; 
  mp.list_results = lr;

  dbg_printf(3, "image 0x%08X + %u ==> ", out_insn.image, val_const);
  encode_cons_field(&out_insn.image, &mp, oper_id);
  dbg_printf(0, "0x%08X\n", out_insn.image);  

  if (mp.error) 
    as_bad(_("invalid operand value: '%u'"), val_const);

}



static void strtolower(char *str)
{
  while (*str != '\0') {
    *str = (uc)charmap[(uc)*str];
    str++;
  }
}

static void clean_out_insn()
{
  out_insn.image = 0;
  out_insn.format = 0;
  out_insn.is_pseudo = 0;
  out_insn.op_num = 0;
  //out_insn.op_pos[9*2] = 0;
  free_all_fixups(out_insn.fixup);
  out_insn.fixup = NULL;
}

static void free_all_fixups(acfixuptype *fix)
{
  acfixuptype *current_fixP = fix;
  acfixuptype *last_fixP;

  while (current_fixP != NULL) {
    last_fixP = current_fixP;
    current_fixP = current_fixP->next; 

    free (last_fixP);
  }
}

#ifdef DEBUG_ON
static void print_internal_fixup(FILE *stream, unsigned int il, acfixuptype *fix)
{
//  expressions ref_expr_fix; /* symbol in which the relocation points to */
  fprintf(stream, "%*s<(Addend) %d, (fields) 0x%X, (mod) %u, (reloc) %u>\n", 
                    il*4, "",
                    operands[fix->operand_id].mod_addend,
                    operands[fix->operand_id].fields,
                    operands[fix->operand_id].mod_type,
                    operands[fix->operand_id].reloc_id);
}
#endif

#ifdef DEBUG_ON
static void print_fixup(FILE *stream, unsigned int il, fixS *fixP)
{
  fprintf(stream, "%*s%lu - [%s,%u] - pcrel %d, done %d, rel %u\n", 
                  il*4, "",
                  (unsigned long) fixP,
                  fixP->fx_file, fixP->fx_line,
                  fixP->fx_pcrel,
                  fixP->fx_done,
                  (unsigned int) fixP->fx_r_type);

  fprintf(stream, "%*ssize %u, where %ld, offset %ld, dot %lu, addnumber %ld\n", 
                  il*4, "",
                  fixP->fx_size, 
                  fixP->fx_where,
                  fixP->fx_offset,
                  fixP->fx_dot_value,
                  fixP->fx_addnumber);


  extern int indent_level;
  indent_level = il+1;
 
  if (fixP->fx_addsy) {
    fprintf(stream, "%*s", (il+1)*4, "");
    print_symbol_value_1(stderr, fixP->fx_addsy); 
    fprintf(stderr, "\n");
  }

  if (fixP->fx_subsy) {
    fprintf(stream, "%*s", (il+1)*4, "");
    print_symbol_value_1(stderr, fixP->fx_subsy); 
    fprintf(stderr, "\n");
  }

  fprintf(stream, "%*smd: operand type %u\n", 
                il*4, "",
                fixP->tc_fix_data);
}
#endif



#ifdef DEBUG_ON
static void print_req_aliases()
{
  acaliastype *p = req_alias;

  unsigned int i = 1;

  dbg_printf(3, "Now dumping .req aliases linked list content\n");
  while (p != NULL)
    {
      dbg_printf(4, "Element #%d, new_name=\"%s\", old_name=\"%s\"\n", i, p->new_name, p->old_name);
      p = p->next;
      i++;
    }
}
#endif

/* parses .req directives of the form 
 new_register_name .req existing_register

 Creates the alias as a node of the linked list
 req_alias of type acaliastype. When an userdef
 symbol is queried and not found, this list
 will be checked.
*/
static void ac_parse_req (char *str)
{
  char *s_pos = str; // Works on s_pos, avoid changing str so we have the full string
                     // to report on error
  char *saved_pos;
  char *operand1; // Stores operand1 string (new symbol name)
  char *operand2; // Stores the second operand (existing symbol name)
  char directive[5]; // Parses .req directive for sanity check
  unsigned int i;

  dbg_printf(0, "Parsing .req directive: \"%s\"\n", str);

  while (ISSPACE(*s_pos)) s_pos++;
  if (*s_pos == '\0')
    INTERNAL_ERROR();

  /* Calculates first operand size */
  i = 0;
  saved_pos = s_pos;
  while (ISALNUM(*s_pos))
    {
      s_pos++;
      i++;
    }
  s_pos = saved_pos;

  if (i > 128)
    {
      as_bad(".req directive first operand is too long in size");
      return;
    }
  /* Allocate first operand */
  operand1 = (char *) malloc(sizeof(char) * (i+1));
  i = 0;
  
  /* Parses first operand */
  while (ISALNUM(*s_pos))
    {
      operand1[i] = TOLOWER(*s_pos);
      i++;
      s_pos++;
    }
  operand1[i] = '\0';

  if (!ISSPACE(*s_pos))
    {
      as_bad("bad character \"%c\" in .req directive: %s", *s_pos, str);
      free(operand1);
      return;
    }

  while (ISSPACE(*s_pos)) s_pos++;
  
  /* Checks for .req directive presence - sanity check */
  i = 0;
  while (ISALNUM(*s_pos) || ISPUNCT(*s_pos))
    {
      directive[i] = TOLOWER(*s_pos);
      s_pos++;
      i++;
      if (i >= 5)
	{
	  as_bad("malformed .req directive: %s", str);
	  free(operand1);
	  return;
	}
    }
  if (strncmp(directive, ".req", 4) || !ISSPACE(*s_pos))
    {
      as_bad("malformed .req directive: %s", str);
      free(operand1);
      return;
    }

  /* Calculates operand2 size */
  while (ISSPACE(*s_pos)) s_pos++;

  i = 0;
  saved_pos = s_pos;
  while (ISALNUM(*s_pos))
    {
      s_pos++;
      i++;
    }
  s_pos = saved_pos;

 if (i > 128)
    {
      as_bad(".req directive second operand is too long in size");
      free(operand1);
      return;
    }
  /* Allocates second operand */
  operand2 = (char *) malloc(sizeof(char) * (i+1));
  i = 0;
  
  /* Parses second operand */
  while (ISALNUM(*s_pos))
    {
      operand2[i] = TOLOWER(*s_pos);
      i++;
      s_pos++;
    }
  operand2[i] = '\0';

  while (ISSPACE(*s_pos)) s_pos++;

  if (*s_pos != '\0')
    {
      as_bad("junk at end of line in .req directive: %s", str);
      free(operand1);
      free(operand2);
      return;
    } 

  dbg_printf(1, ".req directive with operands new_name=\"%s\" old_name=\"%s\"\n", operand1, operand2);

  /* Verify if operand2 is really an existing register(in fact any other symbol category will do) */
  if (!ac_valid_symbol("", operand2, &i))
    {
      as_bad("\"%s\" in .req directive does not exists", operand2);
      free(operand1);
      free(operand2);
      return;
    }

  /* Verify if operand1 does not reference an existing register */
  if (ac_valid_symbol("", operand1, &i))
    {
      as_warn("ignoring redefinition of symbol alias \"%s\"", operand1);
      free(operand1);
      free(operand2);
      return;
    }

  /* Create alias */
  if (req_alias == NULL)
    {
      req_alias = (acaliastype *) malloc(sizeof(acaliastype));
      req_alias->next = NULL;      
    }
  else
    {
      acaliastype *temp;
      temp = (acaliastype *) malloc (sizeof(acaliastype));
      temp->next = req_alias;
      req_alias = temp;
    }
  req_alias->new_name = strdup(operand1);
  req_alias->old_name = strdup(operand2);

  free(operand1);
  free(operand2);

#ifdef DEBUG_ON
  print_req_aliases();
#endif
}

/* should not be used
 .req directive does not start a line - prints error */
static void s_req (int a ATTRIBUTE_UNUSED)
{
  as_bad("invalid syntax for .req directive");

  while (!(*input_line_pointer == '\n' || *input_line_pointer == '\0'))
    input_line_pointer++;
  return;
}

/* .unreq directive, unregister an alias created with
 .req directive. */
static void s_unreq (int a ATTRIBUTE_UNUSED) 
{
  char *unregister_name;

  char *s_pos, saved_char;
  unsigned int i;

  acaliastype *p, *ant;

  s_pos = input_line_pointer;
  while (ISSPACE(*s_pos))s_pos++;

  unregister_name = s_pos;
  i = 0;
  while (ISALNUM(*s_pos))
    {
      i++;
      s_pos++;
    }
  if (i > 128)
    {
      as_bad(".unreq operand too long");
      while (!(*input_line_pointer == '\n' || *input_line_pointer == '\0'))
	input_line_pointer++;
      return;
    }
  saved_char = *s_pos;
  *s_pos = '\0';

  dbg_printf(0, "Parsing .unreq directive, operand: \"%s\"\n", unregister_name);
  
  /* Finds the alias */
  ant = req_alias;
  p = req_alias;
  while (p != NULL)
    {
      if (strncmp(p->new_name, unregister_name, i)==0)
	break;
      ant = p;
      p = p->next;
    }
  
  /* if not found */
  if (p == NULL)
    {
      as_bad("\"%s\" symbol in .unreq directive does not exist", unregister_name);
      *s_pos = saved_char;
      input_line_pointer = s_pos;
      demand_empty_rest_of_line();
      return;
    }
  /* deletes it */
  if (p == req_alias)
    req_alias = req_alias->next;
  else 
    ant->next = p->next;
    
  free(p->new_name);
  free(p->old_name);
  free(p);

  *s_pos = saved_char;

  input_line_pointer = s_pos;
  demand_empty_rest_of_line();
  
}

static void s_dummy (int a ATTRIBUTE_UNUSED){
while (!(*input_line_pointer == '\n' || *input_line_pointer == '\0'))
	input_line_pointer++;
      return;
}


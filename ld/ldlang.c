/* Copyright (C) 1991 Free Software Foundation, Inc.
   
This file is part of GLD, the Gnu Linker.

GLD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GLD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GLD; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* $Id$ 
 *
*/



#include "sysdep.h" 
#include "bfd.h"

#include "ld.h"
#include "ldmain.h"
#include "ldsym.h"
#include "ldgram.tab.h"
#include "ldmisc.h"
#include "ldlang.h"
#include "ldexp.h"
#include "ld-emul.h"
#include "ldlex.h"

/* EXPORTS */

extern char  *default_target;

extern unsigned int undefined_global_sym_count;

static CONST  char *startup_file;
static lang_input_statement_type *first_file;
lang_statement_list_type statement_list;
lang_statement_list_type *stat_ptr = &statement_list;
lang_statement_list_type lang_output_section_statement;
lang_statement_list_type input_file_chain;
lang_statement_list_type file_chain;
extern char *current_file;
static boolean placed_commons = false;

boolean lang_float_flag;

static lang_output_section_statement_type *default_common_section;


/* FORWARDS */
PROTO(static void, print_statements,(void));
PROTO(static void, print_statement,(lang_statement_union_type *,
				     lang_output_section_statement_type *));



/* EXPORTS */
boolean lang_has_input_file = false;


extern bfd *output_bfd;
size_t largest_section;


extern enum bfd_architecture ldfile_output_architecture;
extern unsigned long ldfile_output_machine;
extern char *ldfile_output_machine_name;


extern ldsym_type *symbol_head;

bfd_vma print_dot;
unsigned int commons_pending;




extern args_type command_line;
extern ld_config_type config;

CONST char *entry_symbol;



lang_output_section_statement_type *create_object_symbols;

extern boolean had_script;
static boolean map_option_f;


boolean had_output_filename = false;
extern boolean write_map;






size_t longest_section_name = 8;


lang_input_statement_type *script_file;

section_userdata_type common_section_userdata;
asection common_section;

#ifdef __STDC__
#define cat(a,b) a##b
#else
#define cat(a,b) a/**/b
#endif

#define new_stat(x,y) (cat(x,_type)*) new_statement(cat(x,_enum), sizeof(cat(x,_type)),y)

#define outside_section_address(q) ( (q)->output_offset + (q)->output_section->vma)

#define outside_symbol_address(q) ((q)->value +   outside_section_address(q->section))

boolean option_longmap = false;

/*----------------------------------------------------------------------
  lang_for_each_statement walks the parse tree and calls the provided
  function for each node
*/

static void 
DEFUN(lang_for_each_statement_worker,(func,  s),
      void (*func)() AND
      lang_statement_union_type *s)
{
  for (; s != (lang_statement_union_type *)NULL ; s = s->next) 
      {
	func(s);

	switch (s->header.type) {
	case lang_output_section_statement_enum:
	  lang_for_each_statement_worker
	    (func, 
	     s->output_section_statement.children.head);
	  break;
	case lang_wild_statement_enum:
	  lang_for_each_statement_worker
	    (func, 
	     s->wild_statement.children.head);
	  break;
	case lang_data_statement_enum:
	case lang_object_symbols_statement_enum:
	case lang_output_statement_enum:
	case lang_target_statement_enum:
	case lang_input_section_enum:
	case lang_input_statement_enum:
	case lang_fill_statement_enum:
	case lang_assignment_statement_enum:
	case lang_padding_statement_enum:
	case lang_address_statement_enum:
	  break;
	default:
	  FAIL();
	  break;
	}
      }
}

void
DEFUN(lang_for_each_statement,(func),
      void (*func)())
{
  lang_for_each_statement_worker(func,
				 statement_list.head);
}
/*----------------------------------------------------------------------*/
static void 
DEFUN(lang_list_init,(list),
      lang_statement_list_type *list)
{
list->head = (lang_statement_union_type *)NULL;
list->tail = &list->head;
}

/*----------------------------------------------------------------------
  Functions to print the link map 
 */

static void 
DEFUN(print_section,(name),
      CONST char *CONST name)
{
  printf("%*s", -longest_section_name, name);
}
static void 
DEFUN_VOID(print_space)
{
  printf(" ");
}
static void 
DEFUN_VOID(print_nl)
{
  printf("\n");
}
static void 
DEFUN(print_address,(value),
      bfd_vma value)
{
  printf("%8lx", value);
}
static void 
DEFUN(print_size,(value),
      size_t value)
{
  printf("%5x", (unsigned)value);
}
static void 
DEFUN(print_alignment,(value),
      unsigned int value)
{
  printf("2**%2u",value);
}

static void 
DEFUN(print_fill,(value),
      fill_type value)
{
  printf("%04x",(unsigned)value);
}

/*----------------------------------------------------------------------
  
  build a new statement node for the parse tree

 */

static
lang_statement_union_type*
DEFUN(new_statement,(type, size, list),
      enum statement_enum type AND
      size_t size AND
      lang_statement_list_type *list)
{
  lang_statement_union_type *new = (lang_statement_union_type *)
    ldmalloc(size);
  new->header.type = type;
  new->header.next = (lang_statement_union_type *)NULL;
  lang_statement_append(list, new, &new->header.next);
  return new;
}

/*
  Build a new input file node for the language. There are several ways
  in which we treat an input file, eg, we only look at symbols, or
  prefix it with a -l etc.

  We can be supplied with requests for input files more than once;
  they may, for example be split over serveral lines like foo.o(.text)
  foo.o(.data) etc, so when asked for a file we check that we havn't
  got it already so we don't duplicate the bfd.

 */
static lang_input_statement_type *
DEFUN(new_afile, (name, file_type, target),
      CONST char *CONST name AND
      CONST lang_input_file_enum_type file_type AND
      CONST char *CONST target)
{
  lang_input_statement_type *p = new_stat(lang_input_statement, 
					  stat_ptr);
  lang_has_input_file = true;
  p->target = target;
  switch (file_type) {
  case  lang_input_file_is_symbols_only_enum:
    p->filename = name;
    p->is_archive =false;
    p->real = true;
    p->local_sym_name= name;
    p->just_syms_flag = true;
    p->search_dirs_flag = false;
    break;
  case lang_input_file_is_fake_enum:
    p->filename = name;
    p->is_archive =false;
    p->real = false;
    p->local_sym_name= name;
    p->just_syms_flag = false;
    p->search_dirs_flag =false;
    break;
  case lang_input_file_is_l_enum:
    p->is_archive = true;
    p->filename = name;
    p->real = true;
    p->local_sym_name = concat("-l",name,"");
    p->just_syms_flag = false;
    p->search_dirs_flag = true;
    break;
  case lang_input_file_is_search_file_enum:
  case lang_input_file_is_marker_enum:
    p->filename = name;
    p->is_archive =false;
    p->real = true;
    p->local_sym_name= name;
    p->just_syms_flag = false;
    p->search_dirs_flag =true;
    break;
  case lang_input_file_is_file_enum:
    p->filename = name;
    p->is_archive =false;
    p->real = true;
    p->local_sym_name= name;
    p->just_syms_flag = false;
    p->search_dirs_flag =false;
    break;
  default:
    FAIL();
  }
  p->asymbols = (asymbol **)NULL;
  p->superfile = (lang_input_statement_type *)NULL;
  p->next_real_file = (lang_statement_union_type*)NULL;
  p->next = (lang_statement_union_type*)NULL;
  p->symbol_count = 0;
  p->common_output_section = (asection *)NULL;
  lang_statement_append(&input_file_chain,
			(lang_statement_union_type *)p,
			&p->next_real_file);
  return p;
}



lang_input_statement_type *
DEFUN(lang_add_input_file,(name, file_type, target),
      char *name AND
      lang_input_file_enum_type file_type AND
      char *target)
{
  /* Look it up or build a new one */
  lang_has_input_file = true;
#if 0
  lang_input_statement_type *p;

  for (p = (lang_input_statement_type *)input_file_chain.head;
       p != (lang_input_statement_type *)NULL;
       p = (lang_input_statement_type *)(p->next_real_file))
      {
	/* Sometimes we have incomplete entries in here */
	if (p->filename != (char *)NULL) {
	  if(strcmp(name,p->filename) == 0)  return p;
	}
     
 }
#endif
  return  new_afile(name, file_type, target);
}


/* Build enough state so that the parser can build its tree */
void
DEFUN_VOID(lang_init)
{

  stat_ptr= &statement_list;
  lang_list_init(stat_ptr);

  lang_list_init(&input_file_chain);
  lang_list_init(&lang_output_section_statement);
  lang_list_init(&file_chain);
  first_file = lang_add_input_file((char *)NULL, 
				   lang_input_file_is_marker_enum,
				   (char *)NULL);
}


/*----------------------------------------------------------------------
 A region is an area of memory declared with the 
 MEMORY {  name:org=exp, len=exp ... } 
 syntax. 

 We maintain a list of all the regions here

 If no regions are specified in the script, then the default is used
 which is created when looked up to be the entire data space
*/

static lang_memory_region_type *lang_memory_region_list;
static lang_memory_region_type **lang_memory_region_list_tail = &lang_memory_region_list;

lang_memory_region_type *
DEFUN(lang_memory_region_lookup,(name),
      CONST char *CONST name)
{

  lang_memory_region_type *p  = lang_memory_region_list;
  for (p = lang_memory_region_list;
       p != ( lang_memory_region_type *)NULL;
       p = p->next) {
    if (strcmp(p->name, name) == 0) {
      return p;
    }
  }
  if (strcmp(name,"*default*")==0) {
    /* This is the default region, dig out first one on the list */
    if (lang_memory_region_list != (lang_memory_region_type*)NULL){
      return lang_memory_region_list;
    }
  }
    {
      lang_memory_region_type *new =
	(lang_memory_region_type *)ldmalloc(sizeof(lang_memory_region_type));
      new->name = buystring(name);
      new->next = (lang_memory_region_type *)NULL;

      *lang_memory_region_list_tail = new;
      lang_memory_region_list_tail = &new->next;
      new->origin = 0;
      new->length = ~0;
      new->current = 0;
      return new;
    }
}


lang_output_section_statement_type *
DEFUN(lang_output_section_find,(name),
      CONST char * CONST name)
{
  lang_statement_union_type *u;
  lang_output_section_statement_type *lookup;

  for (u = lang_output_section_statement.head;
       u != (lang_statement_union_type *)NULL;
       u = lookup->next)
      {
	lookup = &u->output_section_statement;
	if (strcmp(name, lookup->name)==0) {
	  return lookup;
	}
      }
  return (lang_output_section_statement_type *)NULL;
}

lang_output_section_statement_type *
DEFUN(lang_output_section_statement_lookup,(name),
      CONST char * CONST name)
{
  lang_output_section_statement_type *lookup;
  lookup =lang_output_section_find(name);
  if (lookup == (lang_output_section_statement_type *)NULL) {
    
    lookup =(lang_output_section_statement_type *)
      new_stat(lang_output_section_statement, stat_ptr);
    lookup->region = (lang_memory_region_type *)NULL;
    lookup->fill = 0;
    lookup->block_value = 1;
    lookup->name = name;

    lookup->next = (lang_statement_union_type*)NULL;  
    lookup->bfd_section = (asection *)NULL;
    lookup->processed = false;
    lookup->addr_tree = (etree_type *)NULL;
    lang_list_init(&lookup->children);

    lang_statement_append(&lang_output_section_statement,
			  (lang_statement_union_type *)lookup,
			  &lookup->next);
  }
  return lookup;
}





static void
DEFUN(print_flags, (outfile, ignore_flags),
      FILE *outfile AND
      lang_section_flags_type *ignore_flags)
{
  fprintf(outfile,"(");
#if 0
  if (flags->flag_read) fprintf(outfile,"R");
  if (flags->flag_write) fprintf(outfile,"W");
  if (flags->flag_executable) fprintf(outfile,"X");
  if (flags->flag_loadable) fprintf(outfile,"L");
#endif
  fprintf(outfile,")");
}

void
DEFUN(lang_map,(outfile),
      FILE *outfile)
{
  lang_memory_region_type *m;
  fprintf(outfile,"**MEMORY CONFIGURATION**\n\n");

  fprintf(outfile,"name\t\torigin\t\tlength\t\tattributes\n");
  for (m = lang_memory_region_list;
       m != (lang_memory_region_type *)NULL;
       m = m->next) 
    {
      fprintf(outfile,"%-16s", m->name);

      fprintf(outfile,"%08lx\t%08lx\t", m->origin, m->length);
      print_flags(outfile, &m->flags);
      fprintf(outfile,"\n");
    }
  fprintf(outfile,"\n\n**LINK EDITOR MEMORY MAP**\n\n");
  fprintf(outfile,"output\t\tinput\t\tvirtual\n");
  fprintf(outfile,"section\t\tsection\t\taddress\tsize\n\n");

  print_statements();

}

/*
 *
 */
static void 
DEFUN(init_os,(s),
      lang_output_section_statement_type *s)
{
  section_userdata_type *new =
    (section_userdata_type *)
      ldmalloc(sizeof(section_userdata_type));

  s->bfd_section = bfd_make_section(output_bfd, s->name);
  s->bfd_section->output_section = s->bfd_section;
  s->bfd_section->flags = SEC_NO_FLAGS;
  /* We initialize an output sections output offset to minus its own */
  /* vma to allow us to output a section through itself */
  s->bfd_section->output_offset = 0;
  get_userdata( s->bfd_section) = new;
}

/***********************************************************************
  The wild routines.

  These expand statements like *(.text) and foo.o to a list of
  explicit actions, like foo.o(.text), bar.o(.text) and
  foo.o(.text,.data) .

  The toplevel routine, wild, takes a statement, section, file and
  target. If either the section or file is null it is taken to be the
  wildcard. Seperate lang_input_section statements are created for
  each part of the expanstion, and placed after the statement provided.

*/
 
static void
DEFUN(wild_doit,(ptr, section, output, file),
      lang_statement_list_type *ptr AND
      asection *section AND
      lang_output_section_statement_type *output AND
      lang_input_statement_type *file)
{
  if(output->bfd_section == (asection *)NULL)
    {
      init_os(output);
    }

  if (section != (asection *)NULL 
      && section->output_section == (asection *)NULL) {
    /* Add a section reference to the list */
    lang_input_section_type *new = new_stat(lang_input_section, ptr);

    new->section = section;
    new->ifile = file;
    section->output_section = output->bfd_section;
    section->output_section->flags |= section->flags;
    if (section->alignment_power > output->bfd_section->alignment_power) {
	output->bfd_section->alignment_power = section->alignment_power;
      }
  }
}

static asection *
DEFUN(our_bfd_get_section_by_name,(abfd, section),
bfd *abfd AND
CONST char *section)
{
  return bfd_get_section_by_name(abfd, section);
}

static void 
DEFUN(wild_section,(ptr, section, file , output),
      lang_wild_statement_type *ptr AND
      CONST char *section AND
      lang_input_statement_type *file AND
      lang_output_section_statement_type *output)
{
  asection *s;
  if (file->just_syms_flag == false) {
    if (section == (char *)NULL) {
      /* Do the creation to all sections in the file */
      for (s = file->the_bfd->sections; s != (asection *)NULL; s=s->next)  {
	wild_doit(&ptr->children, s, output, file);
      }
    }
    else {
      /* Do the creation to the named section only */
      wild_doit(&ptr->children,
		our_bfd_get_section_by_name(file->the_bfd, section),
		output, file);
    }
  }
}


/* passed a file name (which must have been seen already and added to
   the statement tree. We will see if it has been opened already and
   had its symbols read. If not then we'll read it.

   Archives are pecuilar here. We may open them once, but if they do
   not define anything we need at the time, they won't have all their
   symbols read. If we need them later, we'll have to redo it.
   */
static
lang_input_statement_type *
DEFUN(lookup_name,(name),
      CONST char * CONST name)
{
  lang_input_statement_type *search;
  for(search = (lang_input_statement_type *)input_file_chain.head;
      search != (lang_input_statement_type *)NULL;
      search = (lang_input_statement_type *)search->next_real_file)
      {
	if (search->filename == (char *)NULL && name == (char *)NULL) {
	  return search;
	}
	if (search->filename != (char *)NULL && name != (char *)NULL) {
	  if (strcmp(search->filename, name) == 0)  {
	    ldmain_open_file_read_symbol(search);
	    return search;
	  }
	}
      }

  /* There isn't an afile entry for this file yet, this must be 
     because the name has only appeared inside a load script and not 
     on the command line  */
  search = new_afile(name, lang_input_file_is_file_enum, default_target);
  ldmain_open_file_read_symbol(search);
  return search;


}

static void
DEFUN(wild,(s, section, file, target, output),
      lang_wild_statement_type *s AND
      CONST char *CONST section AND
      CONST char *CONST file AND
      CONST char *CONST target AND
      lang_output_section_statement_type *output)
{
  lang_input_statement_type *f;
  if (file == (char *)NULL) {
    /* Perform the iteration over all files in the list */
    for (f = (lang_input_statement_type *)file_chain.head;
	 f != (lang_input_statement_type *)NULL;
	 f = (lang_input_statement_type *)f->next) {
      wild_section(s,  section, f, output);
    }
  }
  else {
    /* Perform the iteration over a single file */
    wild_section( s, section, lookup_name(file), output);
  }
  if (strcmp(section,"COMMON") == 0 	
      && default_common_section == (lang_output_section_statement_type*)NULL) 
      {
	/* Remember the section that common is going to incase we later
	   get something which doesn't know where to put it */
	default_common_section = output;
      }
}

/*
  read in all the files 
  */
static bfd *	
DEFUN(open_output,(name, target),
      CONST char *CONST name AND
      CONST char *CONST target)
{
  extern CONST char *output_filename;
  bfd * output = bfd_openw(name, target);
  output_filename = name;	    
  if (output == (bfd *)NULL) 
      {
	if (bfd_error == invalid_target) {
	  info("%P%F target %s not found\n", target);
	}
	info("%P%F problem opening output file %s, %E", name);
      }
  
  output->flags |= D_PAGED;
  bfd_set_format(output, bfd_object);
  return output;
}


static CONST  char *current_target;

static void
DEFUN(ldlang_open_output,(statement),
      lang_statement_union_type *statement)
{
  switch (statement->header.type) 
      {
      case  lang_output_statement_enum:
	output_bfd = open_output(statement->output_statement.name,current_target);
	ldemul_set_output_arch();
	break;

      case lang_target_statement_enum:
	current_target = statement->target_statement.target;
	break;
      default:
	break;
      }
}

static void
DEFUN(open_input_bfds,(statement),
      lang_statement_union_type *statement)
{
  switch (statement->header.type) 
      {
      case lang_target_statement_enum:
	current_target = statement->target_statement.target;
	break;
      case lang_wild_statement_enum:
	/* Maybe we should load the file's symbols */
	if (statement->wild_statement.filename) 
	    {
	      (void)  lookup_name(statement->wild_statement.filename);
	    }
	break;
      case lang_input_statement_enum:
	if (statement->input_statement.real == true) 
	    {
	      statement->input_statement.target = current_target;
	      lookup_name(statement->input_statement.filename);
	    }
	break;
      default:
	break;
      }
}
/* If there are [COMMONS] statements, put a wild one into the bss section */

static void
lang_reasonable_defaults()
{
#if 0
      lang_output_section_statement_lookup(".text");
      lang_output_section_statement_lookup(".data");

  default_common_section = 
    lang_output_section_statement_lookup(".bss");


  if (placed_commons == false) {
    lang_wild_statement_type *new =
      new_stat(lang_wild_statement,
	       &default_common_section->children);
    new->section_name = "COMMON";
    new->filename = (char *)NULL;
    lang_list_init(&new->children);
  }
#endif

}

/*
 Add the supplied name to the symbol table as an undefined reference.
 Remove items from the chain as we open input bfds
 */
typedef struct ldlang_undef_chain_list_struct {
  struct ldlang_undef_chain_list_struct *next;
  char *name;
} ldlang_undef_chain_list_type;

static ldlang_undef_chain_list_type *ldlang_undef_chain_list_head;

void
DEFUN(ldlang_add_undef,(name),	
      CONST char *CONST name)
{
  ldlang_undef_chain_list_type *new =
    (ldlang_undef_chain_list_type
     *)ldmalloc(sizeof(ldlang_undef_chain_list_type));

  new->next = ldlang_undef_chain_list_head;
  ldlang_undef_chain_list_head = new;

  new->name = buystring(name);
}
/* Run through the list of undefineds created above and place them
   into the linker hash table as undefined symbols belonging to the
   script file.
*/
static void
DEFUN_VOID(lang_place_undefineds)
{
  ldlang_undef_chain_list_type *ptr = ldlang_undef_chain_list_head;
  while (ptr != (ldlang_undef_chain_list_type*)NULL) {
    ldsym_type *sy = ldsym_get(ptr->name);
    asymbol *def;
    asymbol **def_ptr = (asymbol **)ldmalloc(sizeof(asymbol **));
    def = (asymbol *)bfd_make_empty_symbol(script_file->the_bfd);
    *def_ptr= def;
    def->name = ptr->name;
    def->flags = BSF_UNDEFINED;
    def->section = (asection *)NULL;
    Q_enter_global_ref(def_ptr);
    ptr = ptr->next;
  }
}



/* Copy important data from out internal form to the bfd way. Also
   create a section for the dummy file
 */

static void
DEFUN_VOID(lang_create_output_section_statements)
{
  lang_statement_union_type*os;
  for (os = lang_output_section_statement.head;
       os != (lang_statement_union_type*)NULL;
       os = os->output_section_statement.next) {
    lang_output_section_statement_type *s =
      &os->output_section_statement;
    init_os(s);
  }

}

static void
DEFUN_VOID(lang_init_script_file)
{
  script_file = lang_add_input_file("script file",
				    lang_input_file_is_fake_enum,
				    (char *)NULL);
  script_file->the_bfd = bfd_create("script file", output_bfd);
  script_file->symbol_count = 0;
  script_file->the_bfd->sections = output_bfd->sections;
}




/* Open input files and attatch to output sections */
static void
DEFUN(map_input_to_output_sections,(s, target, output_section_statement),
      lang_statement_union_type *s AND
      CONST char *target AND
      lang_output_section_statement_type *output_section_statement)
{
  for (; s != (lang_statement_union_type *)NULL ; s = s->next) 
    {
      switch (s->header.type) {
      case lang_wild_statement_enum:
 	wild(&s->wild_statement, s->wild_statement.section_name,
	     s->wild_statement.filename, target,
	     output_section_statement);

	break;

      case lang_output_section_statement_enum:
	map_input_to_output_sections(s->output_section_statement.children.head,
			target,
			&s->output_section_statement);
	break;
      case lang_output_statement_enum:
	break;
      case lang_target_statement_enum:
	target = s->target_statement.target;
	break;
      case lang_fill_statement_enum:
      case lang_input_section_enum:
      case lang_object_symbols_statement_enum:
      case lang_data_statement_enum:
      case lang_assignment_statement_enum:
      case lang_padding_statement_enum:
	break;
      case lang_afile_asection_pair_statement_enum:
	FAIL();
	break;
      case lang_address_statement_enum:
	/* Mark the specified section with the supplied address */
	{
	  lang_output_section_statement_type *os =
	    lang_output_section_statement_lookup
	      (s->address_statement.section_name);
	  os->addr_tree = s->address_statement.address;
	}
	break;
      case lang_input_statement_enum:
	/* A standard input statement, has no wildcards */
	/*	ldmain_open_file_read_symbol(&s->input_statement);*/
	break;
      }
    }
}





static void 
DEFUN(print_output_section_statement,(output_section_statement),
      lang_output_section_statement_type *output_section_statement)
{
  asection *section = output_section_statement->bfd_section;
  print_nl();
  print_section(output_section_statement->name);

  if (section) {
    print_dot = section->vma;
    print_space();
    print_section("");
    print_space();
    print_address(section->vma);
    print_space();
    print_size(section->size);
    print_space();
    print_alignment(section->alignment_power);
    print_space();
#if 0
    printf("%s flags", output_section_statement->region->name);
    print_flags(stdout, &output_section_statement->flags);
#endif

  }
  else {
    printf("No attached output section");
  }
  print_nl();
  print_statement(output_section_statement->children.head,
		  output_section_statement);

}

static void 
DEFUN(print_assignment,(assignment, output_section),
      lang_assignment_statement_type *assignment AND
      lang_output_section_statement_type *output_section)
{
  etree_value_type result;
  print_section("");
  print_space();
  print_section("");
  print_space();
  print_address(print_dot);
  print_space();
  result = exp_fold_tree(assignment->exp->assign.src,
			 output_section,
			 lang_final_phase_enum,
			 print_dot,
			 &print_dot);
			 
  if (result.valid) {
    print_address(result.value);
  }
  else 
      {
	printf("*undefined*");
      }
  print_space();
  exp_print_tree(stdout, assignment->exp);
  printf("\n");
}

static void
DEFUN(print_input_statement,(statm),
      lang_input_statement_type *statm)
{
  if (statm->filename != (char *)NULL) {
    printf("LOAD %s\n",statm->filename);
  }
}

static void 
DEFUN(print_symbol,(q),
      asymbol *q)
{
  print_section("");
  printf(" ");
  print_section("");
  printf(" ");
  print_address(outside_symbol_address(q));
  printf("              %s", q->name ? q->name : " ");
  print_nl();
}

static void 
DEFUN(print_input_section,(in),
      lang_input_section_type *in)
{
  asection *i = in->section;

  if(i->size != 0) {
    print_section("");
    printf(" ");
    print_section(i->name);
    printf(" ");
    if (i->output_section) {
      print_address(i->output_section->vma + i->output_offset);
      printf(" ");
      print_size(i->size);
      printf(" ");
      print_alignment(i->alignment_power);
      printf(" ");
      if (in->ifile) {

	bfd *abfd = in->ifile->the_bfd;
	if (in->ifile->just_syms_flag == true) {
	  printf("symbols only ");
	}

	printf(" %s ",abfd->xvec->name);
	if(abfd->my_archive != (bfd *)NULL) {
	  printf("[%s]%s", abfd->my_archive->filename,
		 abfd->filename);
	}
	else {
	  printf("%s", abfd->filename);
	}
	print_nl();

	/* Find all the symbols in this file defined in this section */
	  {
	    asymbol **p;
	    for (p = in->ifile->asymbols; *p; p++) {
	      asymbol *q = *p;
	
	      if (bfd_get_section(q) == i && q->flags & BSF_GLOBAL) {
		print_symbol(q);
	      }
	    }
	  }
      }
      else {
	print_nl();
      }


      print_dot = outside_section_address(i) + i->size;
    }
    else {
      printf("No output section allocated\n");
    }
  }
}

static void
DEFUN(print_fill_statement,(fill),
      lang_fill_statement_type *fill)
{
  printf("FILL mask ");
  print_fill( fill->fill);
}

static void
DEFUN(print_data_statement,(data),
      lang_data_statement_type *data)
{
/*  bfd_vma value; */
  print_section("");
  print_space();
  print_section("");
  print_space();
  ASSERT(print_dot == data->output_vma);

  print_address(data->output_vma);
  print_space();
  print_address(data->value);
  print_space();
  switch (data->type) {
  case BYTE :
    printf("BYTE ");
    print_dot += BYTE_SIZE;
    break;
  case SHORT:
    printf("SHORT ");
    print_dot += SHORT_SIZE;
    break;  
  case LONG:
    printf("LONG ");
    print_dot += LONG_SIZE;
    break;
  }

  exp_print_tree(stdout, data->exp);
  
  printf("\n");
}


static void
DEFUN(print_padding_statement,(s),
      lang_padding_statement_type *s)
{
  print_section("");
  print_space();
  print_section("*fill*");
  print_space();
  print_address(s->output_offset + s->output_section->vma);
  print_space();
  print_size(s->size);
  print_space();
  print_fill(s->fill);
  print_nl();
}

static void 
DEFUN(print_wild_statement,(w,os),
      lang_wild_statement_type *w AND
      lang_output_section_statement_type *os)
{
  if (w->filename != (char *)NULL) {
    printf("%s",w->filename);
  }
  else {
    printf("*");
  }
  if (w->section_name != (char *)NULL) {
    printf("(%s)",w->section_name);
  }
  else {
    printf("(*)");
  }
  print_nl();
  print_statement(w->children.head, os);

}
static void
DEFUN(print_statement,(s, os),
      lang_statement_union_type *s AND
      lang_output_section_statement_type *os)
{
  while (s) {
    switch (s->header.type) {
    case lang_wild_statement_enum:
      print_wild_statement(&s->wild_statement, os);
      break;
    default:
      printf("Fail with %d\n",s->header.type);
      FAIL();
      break;
    case lang_address_statement_enum:
      printf("address\n");
      break;
      break;
    case lang_object_symbols_statement_enum:
      printf("object symbols\n");
      break;
    case lang_fill_statement_enum:
      print_fill_statement(&s->fill_statement);
      break;
    case lang_data_statement_enum:
      print_data_statement(&s->data_statement);
      break;
    case lang_input_section_enum:
      print_input_section(&s->input_section);
      break;
    case lang_padding_statement_enum:
      print_padding_statement(&s->padding_statement);
      break;
    case lang_output_section_statement_enum:
      print_output_section_statement(&s->output_section_statement);
      break;
    case lang_assignment_statement_enum:
      print_assignment(&s->assignment_statement,
		       os);
      break;


    case lang_target_statement_enum:
      printf("TARGET(%s)\n", s->target_statement.target);
      break;
    case lang_output_statement_enum:
      printf("OUTPUT(%s)\n", s->output_statement.name);
      break;
    case lang_input_statement_enum:
      print_input_statement(&s->input_statement);
      break;
    case lang_afile_asection_pair_statement_enum:
      FAIL();
      break;
    }
    s = s->next;
  }
}


static void
DEFUN_VOID(print_statements)
{
  print_statement(statement_list.head,
		  (lang_output_section_statement_type *)NULL);
}

static bfd_vma
DEFUN(insert_pad,(this_ptr, fill, power, output_section_statement, dot),
      lang_statement_union_type **this_ptr AND
      fill_type fill AND
      unsigned int power AND
      asection * output_section_statement AND
      bfd_vma dot)
{
  /* Align this section first to the 
     input sections requirement, then
     to the output section's requirement.
     If this alignment is > than any seen before, 
     then record it too. Perform the alignment by
     inserting a magic 'padding' statement.
     */

  unsigned int alignment_needed =  align_power(dot, power) - dot;

  if (alignment_needed != 0) 
    {
      lang_statement_union_type *new = 
	(lang_statement_union_type *)
	  ldmalloc(sizeof(lang_padding_statement_type));
      /* Link into existing chain */
      new->header.next = *this_ptr;
      *this_ptr = new;
      new->header.type = lang_padding_statement_enum;
      new->padding_statement.output_section = output_section_statement;
      new->padding_statement.output_offset =
	dot - output_section_statement->vma;
      new->padding_statement.fill = fill;
      new->padding_statement.size = alignment_needed;
    }


  /* Remember the most restrictive alignment */
  if (power > output_section_statement->alignment_power) {
    output_section_statement->alignment_power = power;
  }
  output_section_statement->size += alignment_needed;
  return alignment_needed + dot;

}

/* Work out how much this section will move the dot point */
static bfd_vma 
DEFUN(size_input_section, (this_ptr, output_section_statement, fill,  dot),
      lang_statement_union_type **this_ptr AND
      lang_output_section_statement_type*output_section_statement AND
      unsigned short fill AND
      bfd_vma dot)
{
  lang_input_section_type *is = &((*this_ptr)->input_section);
  asection *i = is->section;
 
  if (is->ifile->just_syms_flag == false) {	    
  dot =  insert_pad(this_ptr, fill, i->alignment_power,
		    output_section_statement->bfd_section, dot);

  /* remember the largest size so we can malloc the largest area */
  /* needed for the output stage */
  if (i->size > largest_section) {
    largest_section = i->size;
  }

  /* Remember where in the output section this input section goes */
  i->output_offset = dot - output_section_statement->bfd_section->vma;

  /* Mark how big the output section must be to contain this now */
  dot += i->size;
  output_section_statement->bfd_section->size =
    dot - output_section_statement->bfd_section->vma;
}

  return dot ;
}


/* Work out the size of the output sections 
 from the sizes of the input sections */
static bfd_vma
DEFUN(lang_size_sections,(s, output_section_statement, prev, fill, dot),
      lang_statement_union_type *s AND
      lang_output_section_statement_type * output_section_statement AND
      lang_statement_union_type **prev AND
      unsigned short fill AND
      bfd_vma dot)
{
  /* Size up the sections from their constituent parts */
  for (; s != (lang_statement_union_type *)NULL ; s = s->next) 
      {
	switch (s->header.type) {
	case lang_output_section_statement_enum:
	    {
	      bfd_vma after;
	      lang_output_section_statement_type *os =
		&(s->output_section_statement);
	      /* The start of a section */
	  
	      if (os->addr_tree == (etree_type *)NULL) {
		/* No address specified for this section, get one
		   from the region specification
		   */
		if (os->region == (lang_memory_region_type *)NULL) {
		  os->region = lang_memory_region_lookup("*default*");
		}
		dot = os->region->current;
	      }
	      else {
		etree_value_type r ;
		r = exp_fold_tree(os->addr_tree,
				  (lang_output_section_statement_type *)NULL,
				  lang_allocating_phase_enum,
				  dot, &dot);
		if (r.valid == false) {
		  info("%F%S: non constant address expression for section %s\n",
		       os->name);
		}
		dot = r.value;
	      }
	      /* The section starts here */
	      /* First, align to what the section needs */

	      dot = align_power(dot, os->bfd_section->alignment_power);
	      os->bfd_section->vma = dot;
	      os->bfd_section->output_offset = 0;

	      (void)  lang_size_sections(os->children.head, os, &os->children.head,
					 os->fill, dot);
	      /* Ignore the size of the input sections, use the vma and size to */
	      /* align against */


	      after = ALIGN(os->bfd_section->vma +
			    os->bfd_section->size,
			    os->block_value) ;


	      os->bfd_section->size = after - os->bfd_section->vma;
	      dot = os->bfd_section->vma + os->bfd_section->size;
	      os->processed = true;

	      /* Replace into region ? */
	      if (os->addr_tree == (etree_type *)NULL 
		  && os->region !=(lang_memory_region_type*)NULL ) {
		os->region->current = dot;
	      }
	    }

	  break;

	case lang_data_statement_enum: 
	    {
	      unsigned int size;
	      s->data_statement.output_vma = dot;
	      s->data_statement.output_section =
		output_section_statement->bfd_section;

	      switch (s->data_statement.type) {
	      case LONG:
		size = LONG_SIZE;
		break;
	      case SHORT:
		size = SHORT_SIZE;
		break;
	      case BYTE:
		size = BYTE_SIZE;
		break;

	      }
	      dot += size;
	      output_section_statement->bfd_section->size += size;
	    }
	  break;

	case lang_wild_statement_enum:

	  dot =	lang_size_sections(s->wild_statement.children.head,
				   output_section_statement,
				   &s->wild_statement.children.head,

				   fill, dot);

	  break;

	case lang_object_symbols_statement_enum:
	  create_object_symbols = output_section_statement;
	  break;
	case lang_output_statement_enum:
	case lang_target_statement_enum:
	  break;
	case lang_input_section_enum:
	  dot =	size_input_section(prev,
				   output_section_statement,
				   output_section_statement->fill, dot);
	  break;
	case lang_input_statement_enum:
	  break;
	case lang_fill_statement_enum:
	  fill = s->fill_statement.fill;
	  break;
	case lang_assignment_statement_enum:
	    {
	      bfd_vma newdot = dot;
	      exp_fold_tree(s->assignment_statement.exp,
			    output_section_statement,
			    lang_allocating_phase_enum,
			    dot,
			    &newdot);

	      if (newdot != dot) 
		/* We've been moved ! so insert a pad */
		  {
		    lang_statement_union_type *new = 
		      (lang_statement_union_type *)
			ldmalloc(sizeof(lang_padding_statement_type));
		    /* Link into existing chain */
		    new->header.next = *prev;
		    *prev = new;
		    new->header.type = lang_padding_statement_enum;
		    new->padding_statement.output_section =
		      output_section_statement->bfd_section;
		    new->padding_statement.output_offset =
		      dot - output_section_statement->bfd_section->vma;
		    new->padding_statement.fill = fill;
		    new->padding_statement.size = newdot - dot;
		    output_section_statement->bfd_section->size +=
		      new->padding_statement.size;
		    dot =   newdot;
		  }
	    }

	  break;
	case lang_padding_statement_enum:
	  FAIL();
	  break;
	default:
	  FAIL();
	  break;
	case lang_address_statement_enum:
	  break;
	}
	prev = &s->header.next;      
      }
  return dot;
}


static bfd_vma
DEFUN(lang_do_assignments,(s, output_section_statement, fill, dot),
      lang_statement_union_type *s AND
      lang_output_section_statement_type * output_section_statement AND
      unsigned short fill AND
      bfd_vma dot)
{

  for (; s != (lang_statement_union_type *)NULL ; s = s->next) 
    {
      switch (s->header.type) {
      case lang_output_section_statement_enum:
	{
	  lang_output_section_statement_type *os =
	    &(s->output_section_statement);
	  dot = os->bfd_section->vma;
	  (void) lang_do_assignments(os->children.head, os, os->fill, dot);
	  dot = os->bfd_section->vma + os->bfd_section->size;
	}
	break;
      case lang_wild_statement_enum:

	dot = lang_do_assignments(s->wild_statement.children.head,
				    output_section_statement,
				    fill, dot);

	break;

      case lang_object_symbols_statement_enum:
      case lang_output_statement_enum:
      case lang_target_statement_enum:
#if 0
      case lang_common_statement_enum:
#endif
	break;
      case lang_data_statement_enum:
	{
	  etree_value_type value ;
	  value =   exp_fold_tree(s->data_statement.exp,
				  0, lang_final_phase_enum, dot, &dot);
	  s->data_statement.value = value.value;
	  if (value.valid == false) info("%F%P: Invalid data statement\n");
	}
	switch (s->data_statement.type) {
	case LONG:
	  dot += LONG_SIZE;
	  break;
	case SHORT:
	  dot += SHORT_SIZE;
	  break;
	case BYTE:
	  dot += BYTE_SIZE;
	  break;
	}
	break;
      case lang_input_section_enum:
	{
	  asection *in =    s->input_section.section;
	  dot += in->size;
	}
	break;

      case lang_input_statement_enum:
	break;
      case lang_fill_statement_enum:
	fill = s->fill_statement.fill;
	break;
      case lang_assignment_statement_enum:
	{
	  exp_fold_tree(s->assignment_statement.exp,
			output_section_statement,
			lang_final_phase_enum,
			dot,
			&dot);
	}

	break;
      case lang_padding_statement_enum:
	dot += s->padding_statement.size;
	break;
      default:
	FAIL();
	break;
      case lang_address_statement_enum:
	break;
      }

    }
  return dot;
}



static void 
DEFUN_VOID(lang_relocate_globals) 
{ 

  /*
    Each ldsym_type maintains a chain of pointers to asymbols which
    references the definition.  Replace each pointer to the referenence
    with a pointer to only one place, preferably the definition. If
    the defintion isn't available then the common symbol, and if
    there isn't one of them then choose one reference.
    */

  FOR_EACH_LDSYM(lgs) {
    asymbol *it;
    if (lgs->sdefs_chain) {
      it = *(lgs->sdefs_chain);
    }
    else if (lgs->scoms_chain != (asymbol **)NULL) {
      it = *(lgs->scoms_chain);
    }
    else if (lgs->srefs_chain != (asymbol **)NULL) {
      it = *(lgs->srefs_chain);
    }
    else {
      /* This can happen when the command line asked for a symbol to
	 be -u */
      it = (asymbol *)NULL;
    }
    if (it != (asymbol *)NULL)
	{
	  asymbol **ptr= lgs->srefs_chain;

	  while (ptr != (asymbol **)NULL) {
	    asymbol *ref = *ptr;
	    *ptr = it;
	    ptr = (asymbol **)(ref->udata);
	  }
	}
  }
}



static void
DEFUN_VOID(lang_finish)
{
  ldsym_type *lgs;

  if (entry_symbol == (char *)NULL) {
    /* No entry has been specified, look for start */
    entry_symbol = "start";
  }
  lgs = ldsym_get_soft(entry_symbol);
  if (lgs && lgs->sdefs_chain) {
    asymbol *sy = *(lgs->sdefs_chain);
    /* We can set the entry address*/
    bfd_set_start_address(output_bfd,
			  outside_symbol_address(sy));

  }
  else {
    /* Can't find anything reasonable, 
       use the first address in the text section 
       */
    asection *ts = bfd_get_section_by_name(output_bfd, ".text");
    if (ts) {
      bfd_set_start_address(output_bfd, ts->vma);
    }
  }
}

/* By now we know the target architecture, and we may have an */
/* ldfile_output_machine_name */
static void
DEFUN_VOID(lang_check)
{
  lang_statement_union_type *file;

  
  for (file = file_chain.head;
       file != (lang_statement_union_type *)NULL;
       file=file->input_statement.next) 
      {
	/* Inspect the architecture and ensure we're linking like
	   with like
	   */

	if (bfd_arch_compatible( file->input_statement.the_bfd,
				output_bfd,
				&ldfile_output_architecture,
				&ldfile_output_machine)) {
	  bfd_set_arch_mach(output_bfd,
			    ldfile_output_architecture, ldfile_output_machine);
	}
	else {
	  enum bfd_architecture this_architecture =
	    bfd_get_architecture(file->input_statement.the_bfd);
	  unsigned long this_machine =
	    bfd_get_machine(file->input_statement.the_bfd);
	
	  info("%I: architecture %s",
	       file,
	       bfd_printable_arch_mach(this_architecture, this_machine));
	  info(" incompatible with output %s\n",
	       bfd_printable_arch_mach(ldfile_output_architecture,
				       ldfile_output_machine));
	  ldfile_output_architecture = this_architecture;
	  ldfile_output_machine = this_machine;
	  bfd_set_arch_mach(output_bfd,
			    ldfile_output_architecture,
			    ldfile_output_machine);


	}
      }
}


/*
 * run through all the global common symbols and tie them 
 * to the output section requested.
 */

static void
DEFUN_VOID(lang_common)
{
  ldsym_type *lgs;
  if (config.relocateable_output == false ||
      command_line.force_common_definition== true) {
    for (lgs = symbol_head;
	 lgs != (ldsym_type *)NULL;
	 lgs=lgs->next)
	{
	  asymbol *com ;
	  unsigned  int power_of_two;
	  size_t size;
	  size_t align;
	  if (lgs->scoms_chain != (asymbol **)NULL) {
	    com = *(lgs->scoms_chain);
	    size = com->value;
	    switch (size) {
	    case 0:
	    case 1:
	      align = 1;
	      power_of_two = 0;
	      break;
	    case 2:
	      power_of_two = 1;
	      align = 2;
	      break;
	    case 3:
	    case 4:
	      power_of_two =2;
	      align = 4;
	      break;
	    case 5:
	    case 6:
	    case 7:
	    case 8:
	      power_of_two = 3;
	      align = 8;
	      break;
	    default:
	      power_of_two = 4;
	      align = 16;
	      break;
	    }


	    /* Change from a common symbol into a definition of
	       a symbol */
	    lgs->sdefs_chain = lgs->scoms_chain;
	    lgs->scoms_chain = (asymbol **)NULL;
	    commons_pending--;
	    /* Point to the correct common section */
	    com->section =
	      ((lang_input_statement_type *)
	       (com->the_bfd->usrdata))->common_section;
	    /*  Fix the size of the common section */
	    com->section->size = ALIGN(com->section->size, align);

	    /* Remember if this is the biggest alignment ever seen */
	    if (power_of_two > com->section->alignment_power) {
	      com->section->alignment_power = power_of_two;
	    }


	    com->flags = BSF_EXPORT | BSF_GLOBAL;

	    if (write_map) 
		{
		  printf ("Allocating common %s: %x at %x\n",
			  lgs->name, 
			  (unsigned) size,
			  (unsigned)  com->section->size);
		}
	    com->value = com->section->size;
	    com->section->size += size;

	  }
	}
  }


}

/*
run through the input files and ensure that every input 
section has somewhere to go. If one is found without
a destination then create an input request and place it
into the statement tree.
*/

static void 
DEFUN_VOID(lang_place_orphans)
{
  lang_input_statement_type *file;
  for (file = (lang_input_statement_type*)file_chain.head;
       file != (lang_input_statement_type*)NULL;
       file = (lang_input_statement_type*)file->next) {
    asection *s;  
    for (s = file->the_bfd->sections;
	 s != (asection *)NULL;
	 s = s->next) {
      if ( s->output_section == (asection *)NULL) {
	/* This section of the file is not attatched, root
	   around for a sensible place for it to go */

	if (file->common_section == s) {
	  /* This is a lonely common section which must
	     have come from an archive. We attatch to the
	     section with the wildcard  */
	  if (config.relocateable_output != true 
	      && command_line.force_common_definition == false) {
	    if (default_common_section ==
		(lang_output_section_statement_type *)NULL) {
	      info("%P: No [COMMON] command, defaulting to .bss\n");

	      default_common_section = 
		lang_output_section_statement_lookup(".bss");

	    }
	    wild_doit(&default_common_section->children, s, 
		      default_common_section, file);	  
	  }
	}
	else {
	  lang_output_section_statement_type *os = 
	    lang_output_section_statement_lookup(s->name);

	  wild_doit(&os->children, s, os, file);
	}
      }
    }
  }
}


void
DEFUN(lang_set_flags,(ptr, flags),
      lang_section_flags_type *ptr AND
      CONST char *flags)
{
  boolean state = true;
  ptr->flag_read = false;
  ptr->flag_write = false;
  ptr->flag_executable = false;
  ptr->flag_loadable= false;
  while (*flags)
      {
	if (*flags == '!') {
	  state = false;
	  flags++;
	}
	else state = true;
	switch (*flags) {
	case 'R':
	  ptr->flag_read = state; 
	  break;
	case 'W':
	  ptr->flag_write = state; 
	  break;
	case 'X':
	  ptr->flag_executable= state;
	  break;
	case 'L':
	  ptr->flag_loadable= state;
	  break;
	default:
	  info("%P%F illegal syntax in flags\n");
	  break;
	}
	flags++;
      }
}



void
DEFUN(lang_for_each_file,(func),
      PROTO(void, (*func),(lang_input_statement_type *)))
{
  lang_input_statement_type *f;
  for (f = (lang_input_statement_type *)file_chain.head; 
       f != (lang_input_statement_type *)NULL;
       f = (lang_input_statement_type *)f->next)  
      {
	func(f);
      }
}


void
DEFUN(lang_for_each_input_section, (func),
      PROTO(void ,(*func),(bfd *ab, asection*as)))
{
  lang_input_statement_type *f;
  for (f = (lang_input_statement_type *)file_chain.head; 
       f != (lang_input_statement_type *)NULL;
       f = (lang_input_statement_type *)f->next)  
    {
      asection *s;
      for (s = f->the_bfd->sections;
	   s != (asection *)NULL;
	   s = s->next) {
	func(f->the_bfd, s);
      }
    }
}



void 
DEFUN(ldlang_add_file,(entry),
      lang_input_statement_type *entry)
{

  lang_statement_append(&file_chain,
			(lang_statement_union_type *)entry,
			&entry->next);
}



void
DEFUN(lang_add_output,(name),
      CONST char *name)
{
  lang_output_statement_type *new = new_stat(lang_output_statement,
					     stat_ptr);
  new->name = name;
  had_output_filename = true;
}


static lang_output_section_statement_type *current_section;

void
DEFUN(lang_enter_output_section_statement,
      (output_section_statement_name,
       address_exp,
       block_value),
      char *output_section_statement_name AND
      etree_type *address_exp AND
      bfd_vma block_value)
{
  lang_output_section_statement_type *os;
  current_section = 
    os =
      lang_output_section_statement_lookup(output_section_statement_name);


  /* Add this statement to tree */
  /*  add_statement(lang_output_section_statement_enum,
      output_section_statement);*/
  /* Make next things chain into subchain of this */

  if (os->addr_tree ==
      (etree_type *)NULL) {
    os->addr_tree =
      address_exp;
  }
  os->block_value = block_value;
  stat_ptr = & os->children;

}


void 
DEFUN_VOID(lang_final)
{
  if (had_output_filename == false) {
    lang_add_output("a.out");
  }
}





asymbol *
DEFUN(create_symbol,(name, flags, section),
      CONST char *name AND
      flagword flags AND
      asection *section)
{
  extern lang_input_statement_type *script_file;
  asymbol **def_ptr = (asymbol **)ldmalloc(sizeof(asymbol **));
  /* Add this definition to script file */
  asymbol *def =  (asymbol  *)bfd_make_empty_symbol(script_file->the_bfd);
  def->name = buystring(name);
  def->udata = 0;
  def->flags = flags;
  def->section = section;

  *def_ptr = def;
  Q_enter_global_ref(def_ptr);
  return def;
}


void
DEFUN_VOID(lang_process)
{               
  if (had_script == false) {
    parse_line(ldemul_get_script());
  }
  lang_reasonable_defaults();
  current_target = default_target;

  lang_for_each_statement(ldlang_open_output); /* Open the output file */
  /* For each output section statement, create a section in the output
     file */
  lang_create_output_section_statements();

  /* Create a dummy bfd for the script */
  lang_init_script_file();	

  /* Add to the hash table all undefineds on the command line */
  lang_place_undefineds();

  /* Create a bfd for each input file */
  current_target = default_target;
  lang_for_each_statement(open_input_bfds);

  common_section.userdata = &common_section_userdata;

  /* Run through the contours of the script and attatch input sections
     to the correct output sections 
     */
  map_input_to_output_sections(statement_list.head, (char *)NULL, 
			       ( lang_output_section_statement_type *)NULL);

  /* Find any sections not attatched explicitly and handle them */
  lang_place_orphans();

  /* Size up the common data */
  lang_common();

  ldemul_before_allocation();

  /* Size up the sections */
  lang_size_sections(statement_list.head,
		     (lang_output_section_statement_type *)NULL,
		     &(statement_list.head), 0, (bfd_vma)0);

  /* See if anything special should be done now we know how big
     everything is */
  ldemul_after_allocation();

  /* Do all the assignments, now that we know the final restingplaces
     of all the symbols */

  lang_do_assignments(statement_list.head,
		      (lang_output_section_statement_type *)NULL,
		      0, (bfd_vma)0);

  /* Make sure that we're not mixing architectures */

  lang_check();

  /* Move the global symbols around */
  lang_relocate_globals();

  /* Final stuffs */
  lang_finish();
}


/* EXPORTED TO YACC */

void
DEFUN(lang_add_wild,(section_name, filename),
      CONST char *CONST section_name AND
      CONST char *CONST filename)
{
  lang_wild_statement_type *new = new_stat(lang_wild_statement,
					   stat_ptr);

  if (section_name != (char *)NULL && strcmp(section_name,"COMMON") == 0)
      {
	placed_commons = true;
      }
  if (filename != (char *)NULL) {
    lang_has_input_file = true;
  }
  new->section_name = section_name;
  new->filename = filename;
  lang_list_init(&new->children);
}
void
DEFUN(lang_section_start,(name, address),
      CONST char *name AND
      etree_type *address)
{
  lang_address_statement_type *ad =new_stat(lang_address_statement, stat_ptr);
  ad->section_name = name;
  ad->address = address;
}

void 
DEFUN(lang_add_entry,(name),
      CONST char *name)
{
  entry_symbol = name;
}

void
DEFUN(lang_add_target,(name),
      CONST char *name)
{
  lang_target_statement_type *new = new_stat(lang_target_statement,
					    stat_ptr);
  new->target = name;

}




void
DEFUN(lang_add_map,(name),
      CONST char *name)
{
  while (*name) {
    switch (*name) {
    case 'F':
      map_option_f = true;
      break;
    }
    name++;
  }
}

void 
DEFUN(lang_add_fill,(exp),
      int exp)
{
  lang_fill_statement_type *new =   new_stat(lang_fill_statement, 
					      stat_ptr);
  new->fill = exp;
}

void 
DEFUN(lang_add_data,(type, exp),
      int type AND
      union etree_union *exp)
{

 lang_data_statement_type *new = new_stat(lang_data_statement,
					    stat_ptr);
 new->exp = exp;
 new->type = type;

}
void
DEFUN(lang_add_assignment,(exp),
      etree_type *exp)
{
  lang_assignment_statement_type *new = new_stat(lang_assignment_statement,
					    stat_ptr);
  new->exp = exp;
}

void
DEFUN(lang_add_attribute,(attribute),
      enum statement_enum attribute)
{
  new_statement(attribute, sizeof(lang_statement_union_type),stat_ptr);
}



void 
DEFUN(lang_startup,(name),
      CONST char *name)
{
  if (startup_file != (char *)NULL) {
   info("%P%FMultiple STARTUP files\n");
  }
  first_file->filename = name;
  first_file->local_sym_name = name;

  startup_file= name;
}
void 
DEFUN(lang_float,(maybe),
      boolean maybe)
{
  lang_float_flag = maybe;
}

void 
DEFUN(lang_leave_output_section_statement,(fill, memspec),
      bfd_vma fill AND
      CONST char *memspec)
{
  current_section->fill = fill;
  current_section->region = lang_memory_region_lookup(memspec);
  stat_ptr = &statement_list;
}
/*
 Create an absolute symbol with the given name with the value of the
 address of first byte of the section named.

 If the symbol already exists, then do nothing.
*/
void
DEFUN(lang_abs_symbol_at_beginning_of,(section, name),
      CONST char *section AND
      CONST char *name)
{
  if (ldsym_undefined(name)) {
    extern bfd *output_bfd;
    extern asymbol *create_symbol();
    asection *s = bfd_get_section_by_name(output_bfd, section);
    asymbol *def = create_symbol(name,
				 BSF_GLOBAL | BSF_EXPORT |
				 BSF_ABSOLUTE,
				 (asection *)NULL);
    if (s != (asection *)NULL) {
      def->value = s->vma;
    }
    else {
      def->value = 0;
    }
  }
}

/*
 Create an absolute symbol with the given name with the value of the
 address of the first byte after the end of the section named.

 If the symbol already exists, then do nothing.
*/
void
DEFUN(lang_abs_symbol_at_end_of,(section, name),
      CONST char *section AND
      CONST char *name)
{
  if (ldsym_undefined(name)){
    extern bfd *output_bfd;
    extern asymbol *create_symbol();
    asection *s = bfd_get_section_by_name(output_bfd, section);
    /* Add a symbol called _end */
    asymbol *def = create_symbol(name,
				 BSF_GLOBAL | BSF_EXPORT |
				 BSF_ABSOLUTE,
				 (asection *)NULL);
    if (s != (asection *)NULL) {
      def->value = s->vma + s->size;
    }
    else {
      def->value = 0;
    }
  }
}

void 
DEFUN(lang_statement_append,(list, element, field),
      lang_statement_list_type *list AND
      lang_statement_union_type *element AND
      lang_statement_union_type **field)
{
  *(list->tail) = element;
  list->tail = field;
}




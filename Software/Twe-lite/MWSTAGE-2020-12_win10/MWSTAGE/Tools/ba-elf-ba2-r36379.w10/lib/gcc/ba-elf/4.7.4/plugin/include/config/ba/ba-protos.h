/* Definitions of target machine for GNU compiler, Beyond Architecture.
   Copyright (C) 2000, 2005, 2006, 2008, 2009, 2010, 2011
   Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifndef GCC_BA_PROTOS_H
#define GCC_BA_PROTOS_H

enum ba_symbol_type {
  SYMBOL_GENERAL,
  SYMBOL_GOTOFF,
  SYMBOL_TLS,
  SYMBOL_TLSGD,
  SYMBOL_TLSLDM,
  SYMBOL_DTPREL,
  SYMBOL_GOTTPREL,
  SYMBOL_GOTTPABS,
  SYMBOL_TPREL,
  SYMBOL_FDPIC,
  SYMBOL_FDPIC_LOCAL,
  SYMBOL_SMALL_DATA
};
#define NUM_SYMBOL_TYPES (SYMBOL_SMALL_DATA + 1)

/* Functions in ba.c */
extern void override_options (void);
 
extern bool ba_symbolic_constant_p (rtx, enum ba_symbol_type *symbol_type);
extern bool ba_atomic_symbolic_constant_p (rtx);
extern int ba_regno_ok_for_base_p (int, int);
extern bool ba_legitimate_address_p (enum machine_mode, rtx, int);
extern bool ba_legitimize_move (enum machine_mode, rtx, rtx);

extern rtx ba_subword (enum machine_mode, rtx, int);
extern void ba_split_64bit_move (rtx, rtx);

extern bool ba_small_data_pattern_p (rtx);
extern rtx ba_rewrite_small_data (rtx);

extern bool ba_contains_tls_symbol_ref_p (rtx);
extern bool ba_contains_func_ref_p (rtx);

extern void ba_file_end (void);

#ifdef RTX_CODE
extern void print_operand (FILE*, rtx, int);
extern void print_operand_address (FILE*, rtx);
extern bool output_addr_const_extra (FILE*, rtx);

extern rtx ba_return_addr (int, rtx);
extern void ba_set_return_address (rtx, rtx);
extern void ba_expand_setcc (enum rtx_code code, rtx dest);
extern void ba_expand_branch (enum rtx_code code, rtx label);
extern int ba_expand_addcc (rtx * operands);
extern int ba_expand_cmove (rtx * operands);
extern void ba_expand_call (rtx retval, rtx addr, rtx memargs, bool sibcall);

extern const char *output_cmov (rtx * operands);
extern const char *output_cadd (rtx * operands);
extern const char *output_setf (rtx * operands);
extern const char *output_bf (rtx * operands);
extern const char *output_return (rtx * operands, int conditional);
extern const char *ba_output_neg_double (rtx * operands);
extern const char *ba_output_add_double (rtx * operands);
extern const char *ba_output_sub_double (rtx * operands);

extern int load_multiple_sequence (rtx *, int, int *, int *, HOST_WIDE_INT *);
extern const char *emit_ldm_seq (rtx *, int);
extern int store_multiple_sequence (rtx *, int, int *, int *, HOST_WIDE_INT *);
extern const char *emit_stm_seq (rtx *, int);

extern bool ba_expand_unaligned_load (rtx, rtx, unsigned int, int);
extern bool ba_expand_unaligned_store (rtx, rtx, unsigned int, int);

extern HOST_WIDE_INT ba_initial_elimination_offset (int, int);

extern rtx ba_libcall_value (enum machine_mode);

#ifdef TREE_CODE
extern void init_cumulative_args (CUMULATIVE_ARGS *, tree, rtx, tree);
#endif /* TREE_CODE */

#endif /* RTX_CODE */

#ifdef HAVE_ATTR_cpu
extern enum attr_cpu ba_schedule;
#endif

extern void ba_init_expanders (void);
extern int ba_register_move_cost (enum machine_mode, enum reg_class,
				  enum reg_class);
extern int ba_memory_move_cost (enum machine_mode, enum reg_class, int);
extern void ba_expand_prologue (void);
extern int ba_can_use_return_insn_p (void);
extern void ba_expand_epilogue (int sibcall);

extern int ba_asm_preferred_eh_data_format (int, int);

#endif /* ! GCC_BA_PROTOS_H */

/* Generated automatically by the program 'build/genpreds'
   from the machine description file '../../gcc-4.7.4-ba-r36379/gcc/config/ba/ba.md'.  */

#ifndef GCC_TM_PREDS_H
#define GCC_TM_PREDS_H

#ifdef HAVE_MACHINE_MODES
extern int general_operand (rtx, enum machine_mode);
extern int address_operand (rtx, enum machine_mode);
extern int register_operand (rtx, enum machine_mode);
extern int pmode_register_operand (rtx, enum machine_mode);
extern int scratch_operand (rtx, enum machine_mode);
extern int immediate_operand (rtx, enum machine_mode);
extern int const_int_operand (rtx, enum machine_mode);
extern int const_double_operand (rtx, enum machine_mode);
extern int nonimmediate_operand (rtx, enum machine_mode);
extern int nonmemory_operand (rtx, enum machine_mode);
extern int push_operand (rtx, enum machine_mode);
extern int pop_operand (rtx, enum machine_mode);
extern int memory_operand (rtx, enum machine_mode);
extern int indirect_operand (rtx, enum machine_mode);
extern int ordered_comparison_operator (rtx, enum machine_mode);
extern int comparison_operator (rtx, enum machine_mode);
extern int reg_or_0_operand (rtx, enum machine_mode);
extern int reg_no_elim_or_0_operand (rtx, enum machine_mode);
extern int const_offset_operand (rtx, enum machine_mode);
extern int bf_int_operand (rtx, enum machine_mode);
extern int move_operand (rtx, enum machine_mode);
extern int cc_reg_operand (rtx, enum machine_mode);
extern int mac_reg_operand (rtx, enum machine_mode);
extern int cmp_operator (rtx, enum machine_mode);
extern int float_cmp_operator (rtx, enum machine_mode);
extern int equality_operator (rtx, enum machine_mode);
extern int register_no_elim_operand (rtx, enum machine_mode);
extern int call_insn_operand (rtx, enum machine_mode);
extern int small_data_pattern (rtx, enum machine_mode);
extern int addsi_operand (rtx, enum machine_mode);
extern int picreg_operand (rtx, enum machine_mode);
extern int fdreg_operand (rtx, enum machine_mode);
extern int pictls_operand (rtx, enum machine_mode);
extern int got_reg (rtx, enum machine_mode);
extern int entri_pattern (rtx, enum machine_mode);
extern int rtnei_pattern (rtx, enum machine_mode);
extern int reti_pattern (rtx, enum machine_mode);
extern int load_multiple_operation (rtx, enum machine_mode);
extern int store_multiple_operation (rtx, enum machine_mode);
extern int got_operand (rtx, enum machine_mode);
extern int gottls_operand (rtx, enum machine_mode);
extern int fdpic_operand (rtx, enum machine_mode);
extern int fdpic_local_operand (rtx, enum machine_mode);
extern int arith_operand (rtx, enum machine_mode);
#endif /* HAVE_MACHINE_MODES */

#endif /* tm-preds.h */

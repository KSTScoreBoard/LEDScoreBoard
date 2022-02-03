/* Definitions of target machine for GNU compiler, Beyond Architecture.
   Copyright (C) 1987, 1988, 1992, 1995, 1996, 1999, 2000, 2001, 2002, 
   2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013
   Free Software Foundation, Inc.
   Contributed by
     Damjan Lampret <lampret@opencores.org> in 1999.
   Major optimizations by
     Matjaz Breskvar <matjaz.breskvar@beyondsemi.com> in 2005
     Uros Bizjak <urosb@beyondsemi.com> in 2005, 2006 and 2007.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Do not assume anything about header files.  */
#define NO_IMPLICIT_EXTERN_C

/* Current ABI level  */
extern int ba_abi_level;

/* Size of the RED_ZONE area.  */
extern int ba_redzone_size; 

/* Support for configure-time defaults of some command line options.  */
#define OPTION_DEFAULT_SPECS			\
  {"arch", "%{!march=*:-march=%(VALUE)}"}

/* Common target default flags. 
   (CPU specific ones are defined in ba-devices.def) */
#define TARGET_DEFAULT				\
  (MASK_SIBCALL)

/* Target CPU builtins */
#define TARGET_CPU_CPP_BUILTINS()				\
  do								\
    {								\
      builtin_define ("__BA__");				\
      builtin_define ("__ba__");				\
      builtin_define ("__BA2__");				\
      builtin_assert ("cpu=ba2");				\
      builtin_assert ("machine=ba2");				\
      if (ba_abi_level == 1)					\
	builtin_define ("__ABI__=1");				\
      else if (ba_abi_level == 2)				\
	builtin_define ("__ABI__=2");				\
      else if (ba_abi_level == 3)				\
	builtin_define ("__ABI__=3");				\
      else if (ba_abi_level == 4)				\
	builtin_define ("__ABI__=4");				\
      else							\
	gcc_unreachable ();					\
      if (TARGET_HARD_MUL)					\
	{							\
	  builtin_define ("__BA_HARD_MUL__");			\
	  if (TARGET_MULHU)					\
	    builtin_define ("__BA_MULHU__");			\
          if (TARGET_MULH)                                      \
            builtin_define ("__BA_MULH__");                     \
	}							\
      if (TARGET_HARD_DIV)					\
	builtin_define ("__BA_HARD_DIV__");			\
      if (TARGET_CARRY)						\
	{							\
	  builtin_define ("__BA_ADDC__");			\
	  if (TARGET_SUBB)					\
	    builtin_define ("__BA_SUBB__");			\
	}							\
      if (TARGET_FDPIC)						\
	builtin_define ("__FDPIC__");				\
      if (TARGET_BYTES_LITTLE_ENDIAN)				\
	builtin_define ("__BA_BYTES_LITTLE__");			\
      if (TARGET_BYTES_LITTLE_ENDIAN ^ TARGET_WORDS_UNNATURAL)	\
	builtin_define ("__BA_WORDS_LITTLE__");			\
      if (TARGET_SIXTEEN_REGS)					\
	builtin_define ("__BA_SIXTEEN_REGS__");			\
      if (TARGET_SINGLE_FLOAT)					\
	builtin_define ("__BA_SINGLE_DOUBLE__");		\
    }								\
  while (0)

/* Options passed to the assembler.  */

#undef ASM_SPEC
#define ASM_SPEC "%{G*} -mba2 %{mle:-EL} %{mno-relax:-no-relax}"

/* Options passed to the linker.  */

#undef LINK_SPEC
#define LINK_SPEC "%{G*} -mba2_elf %{mle:-EL} %{mno-relax:-no-relax}"

/* Options passed to the compiler.  */

#undef CC1_SPEC
#define CC1_SPEC "%{G*} %{mbe:%{mle: %e-mbe and -mle may not be used together}}"

/* Default to -G 0 */
#ifndef BA_DEFAULT_GVALUE
#define BA_DEFAULT_GVALUE 0
#endif

#undef TARGET_ASM_NAMED_SECTION
#define TARGET_ASM_NAMED_SECTION  default_elf_asm_named_section

/* Target machine storage layout */

#define DOUBLE_TYPE_SIZE (TARGET_SINGLE_FLOAT ? 32 : 64)

/* Define this if most significant bit is lowest numbered
   in instructions that operate on numbered bit-fields.
   This is not true on the BA. */
#define BITS_BIG_ENDIAN 0

/* Define this if most significant byte of a word is the lowest numbered. 
   Run-time selectable with -mbe & -mle. */
#define BYTES_BIG_ENDIAN (TARGET_BYTES_LITTLE_ENDIAN == 0)

/* Define this if most significant word of a multiword number is the lowest
   numbered. */
#define WORDS_BIG_ENDIAN			\
  ((TARGET_WORDS_UNNATURAL == 0)		\
   ? BYTES_BIG_ENDIAN				\
   : !BYTES_BIG_ENDIAN)

/* Width of a word, in units (bytes).  */
#define UNITS_PER_WORD 4

/* Allocation boundary (in *bits*) for storing arguments in argument list.  */
#define PARM_BOUNDARY 32

/* Boundary (in *bits*) on which stack pointer should be aligned.  */
#define STACK_BOUNDARY 64

/* Allocation boundary (in *bits*) for the code of a function.  */
#define FUNCTION_BOUNDARY 8

/* Setting STRUCTURE_SIZE_BOUNDARY to 32 produces more efficient code, but
   the value 8 produces more compact structures.  The command line option
   -mstructure-size-boundary=<n> can be used to change this value.  */
#define STRUCTURE_SIZE_BOUNDARY ba_structure_size_boundary
extern int ba_structure_size_boundary;

/* This is the value used to initialize ba_structure_size_boundary.  If a
   particular target wants to change the default value it should change
   the definition of this macro, not STRUCTURE_SIZE_BOUNDARY.  */
#ifndef DEFAULT_STRUCTURE_SIZE_BOUNDARY
#define DEFAULT_STRUCTURE_SIZE_BOUNDARY 32
#endif

/* No data type wants to be aligned rounder than this.  */
#define BIGGEST_ALIGNMENT 64

/* The best alignment to use in cases where we have a choice.  */
#define FASTEST_ALIGNMENT 32

/* Make strings word-aligned so strcpy from constants will be faster.  */

#define CONSTANT_ALIGNMENT(EXP, ALIGN)  				\
  ((TREE_CODE (EXP) == STRING_CST					\
    && (ALIGN) < BITS_PER_WORD						\
    && TARGET_ALIGN32)							\
   ? BITS_PER_WORD : (ALIGN))

/* One use of this macro is to increase alignment of medium-size
   data to make it all fit in fewer cache lines.  Another is to
   cause character arrays to be word-aligned so that `strcpy' calls
   that copy constants to character arrays can be done inline.  */         

#define DATA_ALIGNMENT(TYPE, ALIGN)                                     \
  ((((ALIGN) < BITS_PER_WORD)						\
    && (TREE_CODE (TYPE) == ARRAY_TYPE                                  \
        || TREE_CODE (TYPE) == UNION_TYPE                               \
        || TREE_CODE (TYPE) == RECORD_TYPE)				\
    && TARGET_ALIGN32)							\
   ? BITS_PER_WORD : (ALIGN))

/* A C expression to compute the alignment for a variable
   in the local store.  */

#define LOCAL_ALIGNMENT(TYPE, ALIGN)					\
  ((((ALIGN) < BITS_PER_WORD)						\
    && (TREE_CODE (TYPE) == ARRAY_TYPE                                  \
        || TREE_CODE (TYPE) == UNION_TYPE                               \
        || TREE_CODE (TYPE) == RECORD_TYPE)				\
    && TARGET_ALIGN32)							\
   ? BITS_PER_WORD : (ALIGN))

/* Align an address */
#define BA_ALIGN(n,a) (((n) + (a) - 1) & ~((a) - 1))

/* Define this if move instructions will actually fail to work
   when given unaligned data.  */
#define STRICT_ALIGNMENT 1

/* A bitfield declared as `int' forces `int' alignment for the struct.  */
#define PCC_BITFIELD_TYPE_MATTERS 1

/* Define if operations between registers always perform the operation
   on the full register even if a narrower mode is specified.  */
#define WORD_REGISTER_OPERATIONS

/* Define if loading in MODE, an integral mode narrower than BITS_PER_WORD
   will either zero-extend or sign-extend.  The value of this macro should
   be the code that says which one of the two operations is implicitly
   done, NIL if none.  */
#define LOAD_EXTEND_OP(MODE) ZERO_EXTEND

/* Define this macro if it is advisable to hold scalars in registers
   in a wider mode than that declared by the program.  In such cases,
   the value is constrained to be within the bounds of the declared
   type, but kept valid in the wider mode.  The signedness of the
   extension may differ from that of the type. */

#define PROMOTE_MODE(MODE, UNSIGNEDP, TYPE)     \
  if (GET_MODE_CLASS (MODE) == MODE_INT         \
      && GET_MODE_SIZE (MODE) < UNITS_PER_WORD) \
    (MODE) = SImode;

/* Define this macro if it is as good or better to call a constant
   function address than to call an address kept in a register.  */
#define NO_FUNCTION_CSE 1
            
/* Standard register usage.  */

#define MAC_REG 32
#define CC_FLAG 33

/* Number of actual hardware registers.
   The hardware registers are assigned numbers for the compiler
   from 0 to just below FIRST_PSEUDO_REGISTER.
   All registers that the compiler knows about must be given numbers,
   even those that are not normally considered general registers.  */
#define FIRST_PSEUDO_REGISTER 36

/* 1 for registers that have pervasive standard uses and are not
   available for the register allocator. On the ba, these are r0 as
   a hard zero, r1 as a stack pointer and faked cc, arg and frame
   pointer registers.
   Proper values are computed in the CONDITIONAL_REGISTER_USAGE.
*/

#define FIXED_REGISTERS							\
/*  r0, r1, r2, r3, r4, r5, r6, r7,  r8, r9,r10,r11,r12,r13,r14,r15 */	\
{    1,  1,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,	\
/* r16,r17,r18,r19,r20,r21,r22,r23, r24,r25,r26,r27,r28,r29,r30,r31 */	\
     0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,	\
/* MAC,flag,arg,frame */						\
     0,   1,  1,    1 }

/* 1 for registers not available across function calls.
   These must include the FIXED_REGISTERS and also any
   registers that can be used without being saved.
   The latter must include the registers where values are returned
   and the register where structure-value addresses are passed.
   Aside from that, you can include as many other registers as you like.
   Proper values are computed in the CONDITIONAL_REGISTER_USAGE.
*/

#define CALL_USED_REGISTERS						\
/*  r0, r1, r2, r3, r4, r5, r6, r7,  r8, r9,r10,r11,r12,r13,r14,r15 */	\
{    1,  1,  0,  1,  1,  1,  1,  1,   1,  0,  0,  0,  0,  0,  0,  0,	\
/* r16,r17,r18,r19,r20,r21,r22,r23, r24,r25,r26,r27,r28,r29,r30,r31 */	\
     0,  0,  0,  0,  0,  0,  0,  1,   1,  1,  1,  1,  1,  1,  1,  1,	\
/* MAC,flag,arg,frame */						\
     1,   1,  1,    1 }

#define FIRST_CALL_USED_REGNO 23

#define TEMP_REGNO (TARGET_SIXTEEN_REGS ? 7 : 23)

/* Say that the epilogue uses the link register.  Note that
   in the case of sibcalls, the values "used by the epilogue" are
   considered live at the start of the called function.  */

#define EPILOGUE_USES(REGNO) ((REGNO) == LINK_REGNUM)

/* List the order in which to allocate registers.  Each register must be
   listed once, even those in FIXED_REGISTERS.

   We allocate in the following order:
	r7-r4	   (not saved; highest used first to make less conflict)
	r23-r31	   (not saved)
	r2	   (not saved; would use r3 for DImode/DFmode)
	r3	   (not saved; return value register)
	r8	   (not saved, would use r9 for DImode/DFmode)
	r9-r22	   (saved)
	mac
	r0, r1, flag, arg, frame (fixed)

   This allocation order is optimal for ABI level 3 (default).
*/

#define REG_ALLOC_ORDER						\
  {7, 6, 5, 4,							\
   23, 24, 25, 26, 27, 28, 29, 30, 31,				\
   3,								\
   8,								\
   9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 2,	\
   32,								\
   0, 1, 33, 34, 35						\
}

/* Return number of consecutive hard regs needed starting at reg REGNO
   to hold something of mode MODE.
   This is ordinarily the length in words of a value of mode MODE
   but can be less for certain modes in special long registers.
   On the ba, all registers are one word long.  */
#define HARD_REGNO_NREGS(REGNO, MODE)   \
 ((GET_MODE_SIZE (MODE) + UNITS_PER_WORD - 1) / UNITS_PER_WORD)

/* Value is 1 if hard register REGNO can hold a value of machine-mode MODE. */
#define HARD_REGNO_MODE_OK(REGNO, MODE) 1

/* Value is 1 if it is a good idea to tie two pseudo registers
   when one has mode MODE1 and one has mode MODE2.
   If HARD_REGNO_MODE_OK could produce different values for MODE1 and MODE2,
   for any hard reg, then this must be 0 for correct output.  */
#define MODES_TIEABLE_P(MODE1, MODE2)  1

/* A C expression for the cost of moving data of mode MODE from a
   register in class FROM to one in class TO.  The classes are
   expressed using the enumeration values such as `GENERAL_REGS'.  A
   value of 2 is the default; other values are interpreted relative to
   that.  */
#define REGISTER_MOVE_COST(MODE, FROM, TO) \
  ba_register_move_cost(MODE, FROM, TO)

/* A C expression for the cost of moving data of mode MODE between a
   register of class CLASS and memory; IN is zero if the value is to
   be written to memory, nonzero if it is to be read in.  This cost
   is relative to those in `REGISTER_MOVE_COST'.  If moving between
   registers and memory is more expensive than between two registers,
   you should define this macro to express the relative cost.  */
#define MEMORY_MOVE_COST(MODE, CLASS, IN) \
  ba_memory_move_cost(MODE, CLASS, IN)

/* A C expression for the cost of a branch instruction.  A value of 1
   is the default; other values are interpreted relative to that.  */
#define BRANCH_COST(speed_p, predictable_p) ba_branch_cost

/* Specify the registers used for certain standard purposes.
   The values of these macros are register numbers.  */

/* First BA register number.  */
#define FIRST_BA_REGNUM 0

/* Last BA register number.  */
#define LAST_BA_REGNUM (TARGET_SIXTEEN_REGS ? 15 : 31)

/* Register to use for pushing function arguments.  */
#define STACK_POINTER_REGNUM 1

/* The ABI-defined global pointer.  */
#define GLOBAL_POINTER_REGNUM ((ba_abi_level == 1) ? LAST_BA_REGNUM	\
			       : (ba_abi_level == 4) ? LAST_BA_REGNUM-1	\
			       : 2)

/* The ABI4-defined thread pointer.  */
#define THREAD_POINTER_REGNUM 2

/* Link register. */
#define LINK_REGNUM 9

/* Register in which static-chain is passed to a function.  */
#define STATIC_CHAIN_REGNUM ((ba_abi_level == 1) ? 13 \
                             : TARGET_PICI && !TARGET_SIXTEEN_REGS ? 30 \
                             : LAST_BA_REGNUM)

/* Base register for access to arguments of the function.  */
#define ARG_POINTER_REGNUM 34

/* Base register for access to local variables of the function.  */
#define FRAME_POINTER_REGNUM 35

/* Base register for access to local variables of the function.  */
#define HARD_FP_REGNUM_ABI1 2
#define HARD_FP_REGNUM_FRAME 10
#define HARD_FP_REGNUM_FRAME_ABI4 11
#define HARD_FP_REGNUM_NO_FRAME 22

#define HARD_FRAME_POINTER_REGNUM			\
  ((ba_abi_level == 1)					\
   ? HARD_FP_REGNUM_ABI1				\
   : (flag_omit_frame_pointer && !TARGET_SIXTEEN_REGS)	\
   ? HARD_FP_REGNUM_NO_FRAME				\
   : (ba_abi_level == 4)				\
   ? HARD_FP_REGNUM_FRAME_ABI4				\
   : HARD_FP_REGNUM_FRAME)

#define HARD_FRAME_POINTER_IS_FRAME_POINTER 0
#define HARD_FRAME_POINTER_IS_ARG_POINTER 0

#define GP_REG_P(REGNO)					\
  (IN_RANGE((REGNO), FIRST_BA_REGNUM, LAST_BA_REGNUM))

/* Definitions for register eliminations.

   This is an array of structures.  Each structure initializes one pair
   of eliminable registers.  The "from" register number is given first,
   followed by "to".  Eliminations of the same "from" register are listed
   in order of preference.

   There are two registers that can always be eliminated on the ba.
   The frame pointer and the arg pointer can be replaced by either the
   hard frame pointer or to the stack pointer, depending upon the
   circumstances.  The hard frame pointer is not used before reload and
   so it is not eligible for elimination.  */

#define ELIMINABLE_REGS						\
  {{ ARG_POINTER_REGNUM, STACK_POINTER_REGNUM},			\
   { ARG_POINTER_REGNUM, HARD_FP_REGNUM_ABI1},			\
   { ARG_POINTER_REGNUM, HARD_FP_REGNUM_FRAME},			\
   { ARG_POINTER_REGNUM, HARD_FP_REGNUM_FRAME_ABI4},		\
   { ARG_POINTER_REGNUM, HARD_FP_REGNUM_NO_FRAME},		\
   { FRAME_POINTER_REGNUM, STACK_POINTER_REGNUM},		\
   { FRAME_POINTER_REGNUM, HARD_FP_REGNUM_ABI1},		\
   { FRAME_POINTER_REGNUM, HARD_FP_REGNUM_FRAME},		\
   { FRAME_POINTER_REGNUM, HARD_FP_REGNUM_FRAME_ABI4},		\
   { FRAME_POINTER_REGNUM, HARD_FP_REGNUM_NO_FRAME}}

/* Define the offset between two registers, one to be eliminated, and the other
   its replacement, at the start of a routine.  */

#define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET) \
  ((OFFSET) = ba_initial_elimination_offset ((FROM), (TO)))


/* -----------------------[ PHX start ]-------------------------------- */

/* Define the classes of registers for register constraints in the
   machine description.  Also define ranges of constants.

   One of the classes must always be named ALL_REGS and include all hard regs.
   If there is more than one class, another class must be named NO_REGS
   and contain no registers.

   The name GENERAL_REGS must be the name of a class (or an alias for
   another name such as ALL_REGS).  This is the class of registers
   that is allowed by "g" or "r" in a register constraint.
   Also, registers outside this class are allocated only when
   instructions express preferences for them.

   GENERAL_REGS and BASE_REGS classess are the same on ba.

   The classes must be numbered in nondecreasing order; that is,
   a larger-numbered class must never be contained completely
   in a smaller-numbered class.

   For any two classes, it is very desirable that there be another
   class that represents their union.  */
   
/* The ba has only one kind of registers, so NO_REGS, GENERAL_REGS
   and ALL_REGS are the only classes.  */

enum reg_class 
{ 
  NO_REGS,
  GENERAL_REGS,
  FDPTR_REGS,
  MAC_REGS,
  ALL_REGS,
  LIM_REG_CLASSES
};

#define N_REG_CLASSES (int) LIM_REG_CLASSES

/* Give names of register classes as strings for dump file.   */

#define REG_CLASS_NAMES							\
{									\
  "NO_REGS",								\
  "GENERAL_REGS",   							\
  "FDPTR_REGS",								\
  "MAC_REGS",								\
  "ALL_REGS"								\
}


/* Define which registers fit in which classes.
   This is an initializer for a vector of HARD_REG_SET
   of length N_REG_CLASSES.  */

/* An initializer containing the contents of the register classes,
   as integers which are bit masks.  The Nth integer specifies the
   contents of class N.  The way the integer MASK is interpreted is
   that register R is in the class if `MASK & (1 << R)' is 1.

   When the machine has more than 32 registers, an integer does not
   suffice.  Then the integers are replaced by sub-initializers,
   braced groupings containing several integers.  Each
   sub-initializer must be suitable as an initializer for the type
   `HARD_REG_SET' which is defined in `hard-reg-set.h'.  */

#define REG_CLASS_CONTENTS			     \
{						     \
  { 0x00000000, 0x00000000 }, /* NO_REGS */	     \
  { 0xffffffff, 0x00000000 }, /* GENERAL_REGS */     \
  { 0x40000000, 0x00000000 }, /* FDPTR_REGS */       \
  { 0x00000000, 0x00000001 }, /* MAC_REGS */	     \
  { 0xffffffff, 0x0000000f }  /* ALL_REGS */	     \
}

/* The same information, inverted:
   Return the class number of the smallest class containing
   reg number REGNO.  This could be a conditional expression
   or could index an array.  */

#define REGNO_REG_CLASS(REGNO)				\
  (  (REGNO) == FDPTR_REGNUM ? FDPTR_REGS		\
   : (REGNO) == MAC_REG ? MAC_REGS			\
   : (REGNO) == CC_FLAG ? NO_REGS			\
   : (REGNO) < FIRST_PSEUDO_REGISTER ? GENERAL_REGS	\
   : NO_REGS)

/* The class value for index registers, and the one for base regs.  */
#define INDEX_REG_CLASS GENERAL_REGS
#define BASE_REG_CLASS GENERAL_REGS

/* Get reg_class from a letter such as appears in the machine description.  */

#define REG_CLASS_FROM_LETTER(C)	\
  ( (C) == 'r' ? GENERAL_REGS		\
  : (C) == 'Z' ? FDPTR_REGS		\
  : (C) == 'a' ? MAC_REGS		\
  : NO_REGS)

/* True if VALUE is a signed 16-bit number.  */

#define SMALL_OPERAND(VALUE) \
  ((unsigned HOST_WIDE_INT) (VALUE) + 0x8000 < 0x10000)

/* True if VALUE is an unsigned 16-bit number.  */

#define SMALL_OPERAND_UNSIGNED(VALUE) \
  (((VALUE) & ~(unsigned HOST_WIDE_INT) 0xffff) == 0)

/* True if VALUE can be loaded into a register using MOVHI.  */

#define MOVHI_OPERAND(VALUE)			\
  (((VALUE) | 0x7fff0000) == 0x7fff0000		\
   || ((VALUE) | 0x7fff0000) + 0x10000 == 0)

/* Return a value X with the low 16 bits clear, and such that
   VALUE - X is a signed 16-bit value.  */

#define CONST_HIGH_PART(VALUE) \
  (((VALUE) + 0x8000) & ~(unsigned HOST_WIDE_INT) 0xffff)

#define CONST_LOW_PART(VALUE) \
  ((VALUE) - CONST_HIGH_PART (VALUE))

#define SMALL_INT(X) SMALL_OPERAND (INTVAL (X))
#define SMALL_INT_UNSIGNED(X) SMALL_OPERAND_UNSIGNED (INTVAL (X))
#define MOVHI_INT(X) MOVHI_OPERAND (INTVAL (X))

#if 1
/* The letters I, J, K, L and M in a register constraint string
   can be used to stand for particular ranges of immediate operands.
   This macro defines what the ranges are.
   C is the letter, and VALUE is a constant value.
   Return 1 if VALUE is in the range specified by C.  */

#define CONST_OK_FOR_LETTER_P(VALUE, C)  \
    (  (C) == 'I' ? ((VALUE) >=-32768 && (VALUE) <=32767) \
     : (C) == 'J' ? ((VALUE) >=0 && (VALUE) <=255) \
     : (C) == 'K' ? ((VALUE) >=0 && (VALUE) <=65535) \
     : (C) == 'L' ? ((VALUE) >=0 && (VALUE) <=31) \
     : (C) == 'M' ? (((VALUE) & 0xffff) == 0 )		\
     : (C) == 'N' ? ((VALUE) >=-128 && (VALUE) <=127) \
     : (C) == 'O' ? ((VALUE) == 0) \
     : (C) == 'P' ? ((VALUE) >=-8 && (VALUE) <=7) \
     : 0 )
#else

/* The letters I, J, K, L, M, N, and P in a register constraint string
   can be used to stand for particular ranges of immediate operands.
   This macro defines what the ranges are.
   C is the letter, and VALUE is a constant value.
   Return 1 if VALUE is in the range specified by C.

   `I' is a signed 16-bit constant
   `J' is a constant with only the high-order 16 bits nonzero
   `K' is a constant with only the low-order 16 bits nonzero
   `L' is a signed 16-bit constant shifted left 16 bits
   `M' is a constant that is greater than 31
   `N' is a positive constant that is an exact power of two
   `O' is the constant zero
   `P' is a constant whose negation is a signed 16-bit constant */

#define CONST_OK_FOR_LETTER_P(VALUE, C)					\
   ( (C) == 'I' ? (unsigned HOST_WIDE_INT) ((VALUE) + 0x8000) < 0x10000	\
   : (C) == 'J' ? ((VALUE) & (~ (unsigned HOST_WIDE_INT) 0xffff0000)) == 0 \
   : (C) == 'K' ? ((VALUE) & (~ (HOST_WIDE_INT) 0xffff)) == 0		\
   : (C) == 'L' ? (((VALUE) & 0xffff) == 0				\
		   && ((VALUE) >> 31 == -1 || (VALUE) >> 31 == 0))	\
   : (C) == 'M' ? (VALUE) > 31						\
   : (C) == 'N' ? (VALUE) > 0 && exact_log2 (VALUE) >= 0		\
   : (C) == 'O' ? (VALUE) == 0						\
   : (C) == 'P' ? (unsigned HOST_WIDE_INT) ((- (VALUE)) + 0x8000) < 0x10000 \
   : 0)
#endif

/* -----------------------[ PHX stop ]-------------------------------- */

/* Similar, but for floating constants, and defining letters G and H.
   Here VALUE is the CONST_DOUBLE rtx itself.  */

#define CONST_DOUBLE_OK_FOR_LETTER_P(VALUE, C)			\
   ((C) == 'G' ? (VALUE) == CONST0_RTX (GET_MODE (VALUE))	\
   : 0)

/* Letters in the range `Q' through `U' may be defined in a
   machine-dependent fashion to stand for arbitrary operand types.
   The machine description macro `EXTRA_CONSTRAINT' is passed the
   operand as its first argument and the constraint letter as its
   second operand.

   `Q' is for constant move_operands.
   `R' is for large data constants (things that don't fit small_data_pattern()
   `S' is for got_operands.  */

#define EXTRA_CONSTRAINT_STR(OP,CODE,STR)				\
  (((CODE) == 'Q')   ? (CONSTANT_P (OP)					\
			&& GET_CODE (OP) != HIGH			\
			&& move_operand (OP, VOIDmode))			\
   : ((CODE) == 'R') ? (CONSTANT_P (OP)					\
			&& !ba_small_data_pattern_p (OP))		\
   : ((CODE) == 'S') ? (got_operand(OP, VOIDmode))			\
   : 0)

/* Given an rtx X being reloaded into a reg required to be
   in class CLASS, return the class of reg to actually use.
   In general this is just CLASS; but on some machines
   in some cases it is preferable to use a more restrictive class.  */

#define PREFERRED_RELOAD_CLASS(X,CLASS)  (CLASS)


/* Return the maximum number of consecutive registers
   needed to represent mode MODE in a register of class CLASS.  */
/* On the ba, this is always the size of MODE in words,
   since all registers are the same size.  */
#define CLASS_MAX_NREGS(CLASS, MODE)	\
 ((GET_MODE_SIZE (MODE) + UNITS_PER_WORD - 1) / UNITS_PER_WORD)

/* Stack layout; function entry, exit and calling.  */

/* Define this if pushing a word on the stack
   makes the stack pointer a smaller address.  */
#define STACK_GROWS_DOWNWARD

/* Define this if the nominal address of the stack frame
   is at the high-address end of the local variables;
   that is, each additional local variable allocated
   goes at a more negative offset in the frame.  */
#define FRAME_GROWS_DOWNWARD 1

/* Offset within stack frame to start allocating local variables at.
   If FRAME_GROWS_DOWNWARD, this is the offset to the END of the
   first local allocated.  Otherwise, it is the offset to the BEGINNING
   of the first local allocated.  */
#define STARTING_FRAME_OFFSET 0

/* Offset of first parameter from the argument pointer register value.  */
#define FIRST_PARM_OFFSET(FNDECL) 0

/* Define this if stack space is still allocated for a parameter passed
   in a register.  The value is the number of bytes allocated to this
   area.  */
/*
#define REG_PARM_STACK_SPACE(FNDECL) (UNITS_PER_WORD * GP_ARG_NUM_REG)
*/
/* Define this if the above stack space is to be considered part of the
   space allocated by the caller.  */
/*
#define OUTGOING_REG_PARM_STACK_SPACE 
*/   
/* Define this macro if `REG_PARM_STACK_SPACE' is defined, but the
   stack parameters don't skip the area specified by it. */
/*
#define STACK_PARMS_IN_REG_PARM_AREA
*/
/* Define this if the maximum size of all the outgoing args is to be
   accumulated and pushed during the prologue.  The amount can be
   found in the variable current_function_outgoing_args_size.  */
#define ACCUMULATE_OUTGOING_ARGS 1

/* Minimum and maximum general purpose registers used to hold arguments.  */
#define GP_ARG_MIN_REG 3
#define GP_ARG_MAX_REG 8
#define GP_ARG_NUM_REG (GP_ARG_MAX_REG - GP_ARG_MIN_REG + 1) 

/* ABI dependent return registers */
#define GP_ARG_RETURN_ABI1 11
#define GP_ARG_RETURN_ABI3 3

/* Define this macro to be 1 if all structure and union return values
   must be in memory.  Since this results in slower code, this should
   be defined only if needed for compatibility with other compilers
   or with an ABI.  If you define this macro to be 0, then the
   conventions used for structure and union return values are decided
   by the `RETURN_IN_MEMORY' macro.

   If not defined, this defaults to the value 1.  */

#define DEFAULT_PCC_STRUCT_RETURN 0

/* Define how to find the value returned by a library function
   assuming the value has mode MODE.  */

#define LIBCALL_VALUE(MODE) ba_libcall_value (MODE)

/* 1 if N is a possible register number for function argument passing. */
     
#define FUNCTION_ARG_REGNO_P(N) \
  (IN_RANGE((N), GP_ARG_MIN_REG, GP_ARG_MAX_REG))

/* Convert from bytes to ints.  */

#define BA_NUM_INTS(X) (((X) + UNITS_PER_WORD - 1) / UNITS_PER_WORD)

/* Define intermediate macro to compute the size
   (in registers) of an argument for the ba.  */ 

#define BA_NUM_REGS(MODE, TYPE)	    \
  BA_NUM_INTS ((MODE) == BLKmode	    \
		 ? int_size_in_bytes (TYPE) \
		 : GET_MODE_SIZE (MODE))

/* A C structure for machine-specific, per-function data.
   This is added to the cfun structure.  */

#ifndef USED_FOR_TARGET
struct GTY(()) machine_function
{
  /* The number of var args that are to be found in registers */
  int var_pretend_size;
};
#endif

/* Define a data type for recording info about an argument list
   during the scan of that argument list.  This data type should
   hold all necessary information about the function itself
   and about the args processed so far, enough to enable macros
   such as FUNCTION_ARG to determine where the next arg should go.  */

typedef struct ba_args
{
  int nregs;
  int named_nregs;
  unsigned int stackstruct : 1;
} CUMULATIVE_ARGS;

/* Initialize a variable CUM of type CUMULATIVE_ARGS
   for a call to a function whose data type is FNTYPE.
   For a library call, FNTYPE is 0.
   The regs member is an integer, the number of arguments got into
   registers so far.  */

#define INIT_CUMULATIVE_ARGS(CUM, FNTYPE, LIBNAME, FNDECL, N_NAMED_ARGS) \
  init_cumulative_args (&(CUM), (FNTYPE), (LIBNAME), (FNDECL))

#define NO_PROFILE_COUNTERS 1

/* Output assembler code to FILE to increment profiler label # LABELNO
   for profiling a function entry.  */

#define FUNCTION_PROFILER(FILE, LABELNO)		\
  fprintf (FILE, "\tb.mov\tr%i,r9\n", TEMP_REGNO);	\
  if (flag_pic)						\
    fprintf (FILE, "\tb.jal\tplt(mcount)\n");		\
  else							\
    fprintf (FILE, "\tb.jal\tmcount\n");

/* EXIT_IGNORE_STACK should be nonzero if, when returning from a function,
   the stack pointer does not matter.  The value is tested only in
   functions that have frame pointers.
   No definition is equivalent to always zero.  */

#define EXIT_IGNORE_STACK 0

/* Addressing modes, and classification of registers for them.  */

/* BA doesn't have any indexed addressing. */

#define REG_OK_FOR_INDEX_P(X) 0
#define REGNO_OK_FOR_INDEX_P(X) 0

#define REGNO_OK_FOR_BASE_P(REGNO) \
  ba_regno_ok_for_base_p (REGNO, 1)

/* The macros REG_OK_FOR..._P assume that the arg is a REG rtx
   and check its validity for a certain class.
   We have two alternate definitions for each of them.
   The usual definition accepts all pseudo regs; the other rejects them all.
   The symbol REG_OK_STRICT causes the latter definition to be used.

   Most source files want to accept pseudo regs in the hope that
   they will get allocated to the class that the insn wants them to be in.
   Some source files that are used after register allocation
   need to be strict.  */

#ifdef REG_OK_STRICT
#define REG_OK_FOR_BASE_P(X) \
  ba_regno_ok_for_base_p (REGNO (X), 1)
#else
#define REG_OK_FOR_BASE_P(X) \
  ba_regno_ok_for_base_p (REGNO (X), 0)
#endif

/* Macros to check register numbers against specific register classes.  */

#define MAX_REGS_PER_ADDRESS 1

#ifdef REG_OK_STRICT
#define GO_IF_LEGITIMATE_ADDRESS(MODE, X, ADDR)	\
{						\
  if (ba_legitimate_address_p (MODE, X, 1))	\
    goto ADDR;					\
}
#else
#define GO_IF_LEGITIMATE_ADDRESS(MODE, X, ADDR)	\
{						\
  if (ba_legitimate_address_p (MODE, X, 0))	\
    goto ADDR;					\
}
#endif

/* SYMBOL_REFs and LABEL_REFs may only be loaded by special instructions */
#define LEGITIMATE_PIC_OPERAND_P(X) \
  (CONST_INT_P (X) || GET_CODE (X) == CONST_DOUBLE)

/* Check for constness inline but use ba_legitimate_address_p
   to check whether a constant really is an address.  */

#define CONSTANT_ADDRESS_P(X) \
  (CONSTANT_P (X) && ba_legitimate_address_p (Pmode, X, 0))

/* BA addresses do not depend on the machine mode. */

#define GO_IF_MODE_DEPENDENT_ADDRESS(ADDR,LABEL) {}


/* Specify the machine mode that this machine uses
   for the index in the tablejump instruction.  */
#define CASE_VECTOR_MODE SImode

/* Nonzero to include default value at
   the last place of a case vector.  */
#define CASE_VECTOR_INCLUDES_DEFAULT !TARGET_CASEI

/* Define this macro to be an expression with a nonzero value if jump
   tables (for `tablejump' insns) should be output in the text
   section, along with the assembler instructions.  Otherwise, the
   readonly data section is used.  */
#define JUMP_TABLES_IN_TEXT_SECTION \
  (!TARGET_TJ_RODATA || flag_pic)

/* The HW PIC register */
#define HW_PIC_OFFSET_TABLE_REGNUM LAST_BA_REGNUM

/* The ABI PIC register */
#define ABI_PIC_OFFSET_TABLE_REGNUM		\
  ((ba_abi_level == 4) ? 10 : 2)

#define PIC_OFFSET_TABLE_REGNUM			\
  (TARGET_PICI ? HW_PIC_OFFSET_TABLE_REGNUM	\
   : flag_pic ? ABI_PIC_OFFSET_TABLE_REGNUM	\
   : INVALID_REGNUM)

/* uClinux function descriptor pointer register.  */
#define FDPTR_REGNUM  30

/* Define this as 1 if `char' should by default be signed; else as 0.  */
#define DEFAULT_SIGNED_CHAR 1

/* This flag, if defined, says the same insns that convert to a signed fixnum
   also convert validly to an unsigned one.  */
#define FIXUNS_TRUNC_LIKE_FIX_TRUNC

/* Max number of bytes we can move from memory to memory
   in one reasonably fast instruction.  */
#define MOVE_MAX 8

/* MOVE_MAX_PIECES is the number of bytes at a time which we can
   move efficiently, as opposed to  MOVE_MAX which is the maximum
   number of bytes we can move with a single instruction.  */
#define MOVE_MAX_PIECES 4


/* Define this if zero-extension is slow (more than one real instruction).  */
/* #define SLOW_ZERO_EXTEND */

/* Nonzero if access to memory by bytes is slow and undesirable.  
   For RISC chips, it means that access to memory by bytes is no
   better than access by words when possible, so grab a whole word
   and maybe make use of that.  */
#define SLOW_BYTE_ACCESS 1

/* Define if shifts truncate the shift count
   which implies one can omit a sign-extension or zero-extension
   of a shift count.  */
/* #define SHIFT_COUNT_TRUNCATED */

/* Value is 1 if truncating an integer of INPREC bits to OUTPREC bits
   is done just by pretending it is already truncated.  */
#define TRULY_NOOP_TRUNCATION(OUTPREC, INPREC) 1

/* Specify the machine mode that pointers have.
   After generation of rtl, the compiler makes no further distinction
   between pointers and any other objects of this machine mode.  */
#define Pmode SImode

/* A function address in a call instruction
   is a byte address (for indexing purposes)
   so give the MEM rtx a byte's mode.  */
#define FUNCTION_MODE SImode

/* Compute the cost of computing a constant rtl expression RTX
   whose rtx-code is CODE.  The body of this macro is a portion
   of a switch statement.  If the code is computed here,
   return it with a return statement.  Otherwise, break from the switch.  */
#if 0
__PHX__ cleanup
#define CONST_COSTS(RTX,CODE,OUTER_CODE) \
  case CONST_INT:						\
    /* Constant zero is super cheap due to clr instruction.  */	\
    if (RTX == const0_rtx) return 0;				\
    if ((unsigned) INTVAL (RTX) < 077) return 1;		\
  case CONST:							\
  case LABEL_REF:						\
  case SYMBOL_REF:						\
    return 3;							\
  case CONST_DOUBLE:						\
    return 5;
#endif


/* Can the condition code MODE be safely reversed?  This is safe in
   all cases on this port, because at present it doesn't use the
   trapping FP comparisons (fcmpo).  */
#define REVERSIBLE_CC_MODE(MODE) 1

/* Control the assembler format that we output.  */

/* A C string constant describing how to begin a comment in the target
   assembler language.  The compiler assumes that the comment will end at
   the end of the line.  */
#define ASM_COMMENT_START "#"

/* Output at beginning of assembler file.  */
/*
__PHX__ clenup
#ifndef ASM_FILE_START
#define ASM_FILE_START(FILE) do {\
fprintf (FILE, "%s file %s\n", ASM_COMMENT_START, main_input_filename);\
fprintf (FILE, ".file\t");   \
  output_quoted_string (FILE, main_input_filename);\
  fputc ('\n', FILE);} while (0)
#endif
*/

#define TARGET_ASM_FILE_END ba_file_end

/* Output to assembler file text saying following lines
   may contain character constants, extra white space, comments, etc.  */

#define ASM_APP_ON ""

/* Output to assembler file text saying following lines
   no longer contain unusual constructs.  */

#define ASM_APP_OFF ""

/* Switch to the text or data segment.  */

/* Output before read-only data.  */
#define TEXT_SECTION_ASM_OP ".section .text"

/* Output before writable data.  */
#define DATA_SECTION_ASM_OP ".section .data"

/* Output before uninitialized data. */
#define BSS_SECTION_ASM_OP  ".section .bss"

/* How to refer to registers in assembler output.
   This sequence is indexed by compiler's hard-register-number (see above).  */

#define REGISTER_NAMES						\
{"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",		\
 "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",		\
 "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",	\
 "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",	\
 "mac", "cc-flag", "arg", "frame"}

/* Default size of the RED_ZONE area.  */
#define RED_ZONE_SIZE_DEFAULT 128

/* Reserved area of the red zone for temporaries
   (currently trampoline temporary register save area).  */
#define RED_ZONE_RESERVE 4

/* Define this to be the delimiter between SDB sub-sections.  The default
   is ";".  */
#define SDB_DELIM       "\n"
   
/* Do not break .stabs pseudos into continuations.  */
#define DBX_CONTIN_LENGTH 0
   
/* Don't try to use the  type-cross-reference character in DBX data.
   Also has the consequence of putting each struct, union or enum
   into a separate .stabs, containing only cross-refs to the others.  */
#define DBX_NO_XREFS
         
/* How to renumber registers for dbx and gdb.
   Vax needs no change in the numeration.  */

#define DBX_REGISTER_NUMBER(REGNO) (REGNO)

/* This is the char to use for continuation (in case we need to turn
   continuation back on).  */

#define DBX_CONTIN_CHAR '?'

/* Node: Label Output */

/* Globalizing directive for a label.  */
#define GLOBAL_ASM_OP "\t.global "

#define SUPPORTS_WEAK 1

/* This is how to output the definition of a user-level label named NAME,
   such as the label on a static function or variable NAME.  */

#define ASM_OUTPUT_LABEL(FILE,NAME)	\
 { assemble_name (FILE, NAME); fputs (":\n", FILE); }
#if 0
/* This is how to output a command to make the user-level label named NAME
   defined for reference from other files.  */
/*
 __PHX__ CLEANUP
#define ASM_GLOBALIZE_LABEL(FILE,NAME)	\
 { fputs ("\t.global ", FILE); assemble_name (FILE, NAME); fputs ("\n", FILE); }
*/

/* SIMON */
/*#define ASM_OUTPUT_LABELREF(stream,name)                \
 { fputc('_',stream); fputs(name,stream); }
*/
#define ASM_OUTPUT_LABELREF(stream,name)                \
{if(name[0] == '*') 					\
   fputs(name,stream);					\
else {							\
   fputc('_',stream); fputs(name,stream); 		\
}}
#endif

/* The prefix to add to user-visible assembler symbols. */

/* Remove any previous definition (elfos.h).  */
/* We use -fno-leading-underscore to remove it, when necessary.  */
#undef  USER_LABEL_PREFIX
#define USER_LABEL_PREFIX ""

/* Remove any previous definition (elfos.h).  */
#ifndef ASM_GENERATE_INTERNAL_LABEL
#define ASM_GENERATE_INTERNAL_LABEL(LABEL, PREFIX, NUM)	\
  sprintf (LABEL, "*%s%d", PREFIX, NUM)
#endif

/* The DWARF 2 CFA column which tracks the return address.  */
#define DWARF_FRAME_RETURN_COLUMN LINK_REGNUM

/* Before the prologue, RA is in LINK_REGNUM.  */
#define INCOMING_RETURN_ADDR_RTX gen_rtx_REG (VOIDmode, LINK_REGNUM)

/* Describe how we implement __builtin_eh_return.  */

#define EH_RETURN_DATA_REGNO(N) \
  ((N) < (TARGET_SIXTEEN_REGS ? 2 : 4) ? (N) + GP_ARG_MIN_REG : INVALID_REGNUM)

#define EH_RETURN_STACKADJ_RTX  \
  gen_rtx_REG (Pmode, (TARGET_SIXTEEN_REGS ? 8 : 25))

/* Select a format to encode pointers in exception handling data.  CODE
   is 0 for data, 1 for code labels, 2 for function pointers.  GLOBAL is
   true if the symbol may be affected by dynamic relocations.  */
#define ASM_PREFERRED_EH_DATA_FORMAT(CODE, GLOBAL)	\
  ba_asm_preferred_eh_data_format ((CODE), (GLOBAL))

/* Offsets recorded in opcodes are a multiple of this alignment factor.  */
#define DWARF_CIE_DATA_ALIGNMENT -4

/* This is how to output an insn to push a register on the stack.
   It need not be very fast code.  */

#define ASM_OUTPUT_REG_PUSH(FILE,REGNO)  \
  fprintf (FILE, "\tb.addi   \tr1,r1,-4\n\tb.sw    \t0(r1),%s\n", reg_names[REGNO])

/* This is how to output an insn to pop a register from the stack.
   It need not be very fast code.  */

#define ASM_OUTPUT_REG_POP(FILE,REGNO)  \
  fprintf (FILE, "\tb.lwz   \t%s,0(r1)\n\tb.addi   \tr1,r1,4\n", reg_names[REGNO])

/* This is how to output an element of a case-vector that is absolute.  */

#define ASM_OUTPUT_ADDR_VEC_ELT(FILE, VALUE)  \
  fprintf (FILE, "\t.word .L%d\n", VALUE)

/* This is how to output an element of a case-vector that is relative.  */

#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, BODY, VALUE, REL)  \
  fprintf (FILE, "\t.word .L%d-.L%d\n", VALUE, REL)

/* This is how to output an assembler line
   that says to advance the location counter
   to a multiple of 2**LOG bytes.  */

#define ASM_OUTPUT_ALIGN(FILE,LOG)  \
  if ((LOG) != 0) fprintf (FILE, "\t.align %d\n", 1 << (LOG))

/* This is how to output an assembler line
   that says to advance the location counter by SIZE bytes.  */

#ifndef ASM_OUTPUT_SKIP
#define ASM_OUTPUT_SKIP(FILE,SIZE)  \
  fprintf (FILE, "\t.space %d\n", (SIZE))
#endif

/* Need to split up .ascii directives to avoid breaking
   the linker. */

/* This is how to output a string.  */
#ifndef ASM_OUTPUT_ASCII
#define ASM_OUTPUT_ASCII(STREAM, STRING, LEN)                           \
do {                                                                    \
  register int i, c, len = (LEN), cur_pos = 17;                         \
  register unsigned char *string = (unsigned char *)(STRING);           \
									\
  fprintf ((STREAM), "\t.ascii\t\"");					\
									\
  for (i = 0; i < len; i++)						\
    {									\
      register int c = string[i];					\
									\
      if (ISPRINT (c))							\
	{								\
	  if (c == '\\' || c == '\"')					\
	    {								\
	      putc ('\\', (STREAM));					\
	      cur_pos++;						\
	    }								\
	  putc (c, (STREAM));						\
	  cur_pos++;							\
	}								\
      else								\
	{								\
	  fprintf ((STREAM), "\\%03o", c);				\
	  cur_pos += 4;							\
	}								\
									\
      if (cur_pos > 72 && i+1 < len)					\
	{								\
	  cur_pos = 17;							\
          fprintf ((STREAM), "\"\n\t.ascii\t\"");                       \
	}								\
    }									\
  fprintf ((STREAM), "\"\n");						\
} while (0)
#endif /* ASM_OUTPUT_ASCII */

/* Invoked just before function output. */

#define ASM_OUTPUT_FUNCTION_PREFIX(stream, fnname)              \
  fputs(".proc ",stream); assemble_name(stream,fnname);         \
  fputs("\n",stream);

/* This says how to output an assembler line
   to define a global common symbol.  */

#define ASM_OUTPUT_COMMON(stream,name,size,rounded)             \
{ data_section();                                               \
  fputs(".global\t",stream); assemble_name(stream,name);        \
  fputs("\n",stream); assemble_name(stream,name);               \
  fprintf(stream,":\n\t.space %d\n",rounded); }

/* This says how to output an assembler line
   to define a local common symbol.  */

#define ASM_OUTPUT_LOCAL(FILE, NAME, SIZE, ROUNDED)  \
( fputs (".bss ", (FILE)),			\
  assemble_name ((FILE), (NAME)),		\
  fprintf ((FILE), ",%d,%d\n", (SIZE),(ROUNDED)))

/* This says how to output an assembler line to define a global common symbol
   with size SIZE (in bytes) and alignment ALIGN (in bits).  */
#ifndef ASM_OUTPUT_ALIGNED_COMMON
#define ASM_OUTPUT_ALIGNED_COMMON(FILE, NAME, SIZE, ALIGN)	\
{ data_section();                                           	\
  if ((ALIGN) > 8)                                          	\
	fprintf(FILE, "\t.align %d\n", ((ALIGN) / BITS_PER_UNIT)); \
  fputs(".global\t", FILE); assemble_name(FILE, NAME);      	\
  fputs("\n", FILE);                                        	\
  assemble_name(FILE, NAME);                                	\
  fprintf(FILE, ":\n\t.space %d\n", SIZE);		    	\
}
#endif /* ASM_OUTPUT_ALIGNED_COMMON */
  
/* This says how to output an assembler line to define a local common symbol
   with size SIZE (in bytes) and alignment ALIGN (in bits).  */

#ifndef ASM_OUTPUT_ALIGNED_LOCAL
#define ASM_OUTPUT_ALIGNED_LOCAL(FILE, NAME, SIZE, ALIGN) \
{ data_section();                                               \
  if ((ALIGN) > 8)                                              \
	fprintf(FILE, "\t.align %d\n", ((ALIGN) / BITS_PER_UNIT)); \
  assemble_name(FILE, NAME);                                    \
  fprintf(FILE, ":\n\t.space %d\n", SIZE);			\
}
#endif /* ASM_OUTPUT_ALIGNED_LOCAL */
                                                     
/* Store in OUTPUT a string (made with alloca) containing
   an assembler-name for a local static variable named NAME.
   LABELNO is an integer which is different for each call.  */

#define ASM_FORMAT_PRIVATE_NAME(OUTPUT, NAME, LABELNO)	\
( (OUTPUT) = (char *) alloca (strlen ((NAME)) + 10),	\
  sprintf ((OUTPUT), "%s.%d", (NAME), (LABELNO)))

#define BA_SIGN_EXTEND(x)  ((HOST_WIDE_INT)			\
  (HOST_BITS_PER_WIDE_INT <= 32 ? (unsigned HOST_WIDE_INT) (x)	\
   : ((((unsigned HOST_WIDE_INT)(x)) & (unsigned HOST_WIDE_INT) 0xffffffff) |\
      ((((unsigned HOST_WIDE_INT)(x)) & (unsigned HOST_WIDE_INT) 0x80000000) \
       ? ((~ (unsigned HOST_WIDE_INT) 0)			\
	  & ~ (unsigned HOST_WIDE_INT) 0xffffffff)		\
       : 0))))

/* Initialize data used by insn expanders.  This is called from insn_emit,
   once for every function before code is generated.  */

#define INIT_EXPANDERS ba_init_expanders ()

/* Length in units of the trampoline for entering a nested function.  */

#define TRAMPOLINE_SIZE 16

/* The alignment of a trampoline, in bits.  */

#define TRAMPOLINE_ALIGNMENT 32

/* A C expression whose value is RTL representing the value of the return
   address for the frame COUNT steps up from the current frame.  */

#define RETURN_ADDR_RTX(COUNT, FRAME) \
  ba_return_addr (COUNT, FRAME)

enum cmodel {
  CM_MEDIUM,	/* Assumes jump offsets fit in the low 24 bits. */
  CM_LARGE	/* No assumptions.  */
};

extern enum cmodel ba_cmodel;

/* Operands for comparison.  */

extern rtx ba_compare_op0;
extern rtx ba_compare_op1;
extern rtx ba_compare_emitted;

extern int ba_branch_cost;

#include "config/ba/ba-opts.h"

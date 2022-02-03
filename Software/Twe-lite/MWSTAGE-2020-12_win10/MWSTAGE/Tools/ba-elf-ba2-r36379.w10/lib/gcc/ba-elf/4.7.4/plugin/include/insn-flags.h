/* Generated automatically by the program `genflags'
   from the machine description file `md'.  */

#ifndef GCC_INSN_FLAGS_H
#define GCC_INSN_FLAGS_H

#define HAVE_blockage 1
#define HAVE_extv_internal (TARGET_UNAL)
#define HAVE_insv_internal (TARGET_UNAL)
#define HAVE_cadd (TARGET_CADD)
#define HAVE_cmov_qi (TARGET_CMOV)
#define HAVE_cmov_hi (TARGET_CMOV)
#define HAVE_cmov_si (TARGET_CMOV)
#define HAVE_extendqisi2 1
#define HAVE_extendhisi2 1
#define HAVE_zero_extendqisi2 1
#define HAVE_zero_extendhisi2 1
#define HAVE_ashlsi3 1
#define HAVE_ashrsi3 1
#define HAVE_lshrsi3 1
#define HAVE_rotrsi3 (TARGET_ROR)
#define HAVE_andqi3 1
#define HAVE_andhi3 1
#define HAVE_andsi3 1
#define HAVE_iorqi3 1
#define HAVE_iorhi3 1
#define HAVE_iorsi3 1
#define HAVE_xorqi3 1
#define HAVE_xorhi3 1
#define HAVE_xorsi3 1
#define HAVE_one_cmplqi2 1
#define HAVE_one_cmplhi2 1
#define HAVE_one_cmplsi2 1
#define HAVE_negdi2 (TARGET_SUBB && TARGET_CARRY)
#define HAVE_negsi2 1
#define HAVE_adddi3 (TARGET_CARRY)
#define HAVE_addsi3 1
#define HAVE_subdi3 (TARGET_SUBB && TARGET_CARRY)
#define HAVE_subsi3 1
#define HAVE_mulsi3 (TARGET_HARD_MUL)
#define HAVE_mulsidi3 (TARGET_HARD_MUL && TARGET_MULH)
#define HAVE_smulsi3_highpart (TARGET_HARD_MUL && TARGET_MULH)
#define HAVE_umulsidi3 (TARGET_HARD_MUL && TARGET_MULHU)
#define HAVE_umulsi3_highpart (TARGET_HARD_MUL && TARGET_MULHU)
#define HAVE_divsi3 (TARGET_HARD_DIV)
#define HAVE_udivsi3 (TARGET_HARD_DIV)
#define HAVE_modsi3 (TARGET_HARD_DIV)
#define HAVE_umodsi3 (TARGET_HARD_DIV)
#define HAVE_casesi_internal (TARGET_CASEI)
#define HAVE_doloop_end_internal (TARGET_LOOP)
#define HAVE_call_value_ba 1
#define HAVE_return (TARGET_ENTRI \
   && ba_can_use_return_insn_p ())
#define HAVE_eh_set_lr 1
#define HAVE_ffssi2 (TARGET_FF1)
#define HAVE_clzsi2 (TARGET_CLZ)
#define HAVE_nop 1
#define HAVE_bitrev 1
#define HAVE_swab 1
#define HAVE_flb (TARGET_FLB)
#define HAVE_ba2_mac (TARGET_MAC)
#define HAVE_ba2_macs (TARGET_MAC && TARGET_DSP)
#define HAVE_ba2_macsu (TARGET_MAC && TARGET_DSP)
#define HAVE_ba2_macuu (TARGET_MAC && TARGET_DSP)
#define HAVE_ba2_smactt (TARGET_MAC && TARGET_DSP)
#define HAVE_ba2_smacbb (TARGET_MAC && TARGET_DSP)
#define HAVE_ba2_smactb (TARGET_MAC && TARGET_DSP)
#define HAVE_ba2_umactt (TARGET_MAC && TARGET_DSP)
#define HAVE_ba2_umacbb (TARGET_MAC && TARGET_DSP)
#define HAVE_ba2_umactb (TARGET_MAC && TARGET_DSP)
#define HAVE_ba2_msu (TARGET_MAC && TARGET_DSP)
#define HAVE_ba2_msus (TARGET_MAC && TARGET_DSP)
#define HAVE_ba2_mfmac (TARGET_MAC)
#define HAVE_ba2_mtmac (TARGET_MAC)
#define HAVE_addsf3 (TARGET_HARD_FLOAT)
#define HAVE_subsf3 (TARGET_HARD_FLOAT)
#define HAVE_mulsf3 (TARGET_HARD_FLOAT)
#define HAVE_divsf3 (TARGET_HARD_FLOAT)
#define HAVE_lrintsfsi2 (TARGET_HARD_FLOAT)
#define HAVE_floatsisf2 (TARGET_HARD_FLOAT)
#define HAVE_load_gotsi 1
#define HAVE_set_fdreg 1
#define HAVE_set_fdreg_local 1
#define HAVE_set_gp 1
#define HAVE_ba2_slls (TARGET_DSP)
#define HAVE_ba2_sqr (TARGET_DSP)
#define HAVE_ba2_sqra (TARGET_DSP)
#define HAVE_ba2_abs (TARGET_DSP)
#define HAVE_ba2_neg (TARGET_DSP)
#define HAVE_ba2_mulas (TARGET_DSP)
#define HAVE_ba2_muluas (TARGET_DSP)
#define HAVE_ba2_mulras (TARGET_DSP)
#define HAVE_ba2_muluras (TARGET_DSP)
#define HAVE_ba2_adds (TARGET_DSP)
#define HAVE_ba2_subs (TARGET_DSP)
#define HAVE_ba2_mulsu (TARGET_DSP)
#define HAVE_ba2_mulhsu (TARGET_DSP)
#define HAVE_ba2_mulhlsu_1 (TARGET_DSP)
#define HAVE_ba2_smultt (TARGET_DSP)
#define HAVE_ba2_smultb (TARGET_DSP)
#define HAVE_mulhisi3 (TARGET_DSP)
#define HAVE_ba2_smulbb (TARGET_DSP)
#define HAVE_ba2_smulwb (TARGET_DSP)
#define HAVE_ba2_smulwt (TARGET_DSP)
#define HAVE_ba2_umultt (TARGET_DSP)
#define HAVE_ba2_umultb (TARGET_DSP)
#define HAVE_umulhisi3 (TARGET_DSP)
#define HAVE_ba2_umulbb (TARGET_DSP)
#define HAVE_ba2_umulwb (TARGET_DSP)
#define HAVE_ba2_umulwt (TARGET_DSP)
#define HAVE_ba2_max (TARGET_DSP)
#define HAVE_ba2_min (TARGET_DSP)
#define HAVE_ba2_lim (TARGET_DSP)
#define HAVE_ba2_smadtt (TARGET_DSP)
#define HAVE_ba2_smadtb (TARGET_DSP)
#define HAVE_maddhisi4 (TARGET_DSP)
#define HAVE_ba2_smadbb (TARGET_DSP)
#define HAVE_ba2_smadwb (TARGET_DSP)
#define HAVE_ba2_smadwt (TARGET_DSP)
#define HAVE_ba2_umadtt (TARGET_DSP)
#define HAVE_ba2_umadtb (TARGET_DSP)
#define HAVE_umaddhisi4 (TARGET_DSP)
#define HAVE_ba2_umadbb (TARGET_DSP)
#define HAVE_ba2_umadwb (TARGET_DSP)
#define HAVE_ba2_umadwt (TARGET_DSP)
#define HAVE_ba2_lwza (TARGET_DSP)
#define HAVE_ba2_swa (TARGET_DSP)
#define HAVE_prologue 1
#define HAVE_epilogue 1
#define HAVE_sibcall_epilogue 1
#define HAVE_movqi 1
#define HAVE_movhi 1
#define HAVE_movsi 1
#define HAVE_movdi 1
#define HAVE_movsf 1
#define HAVE_movdf 1
#define HAVE_extv (TARGET_UNAL)
#define HAVE_extzv (TARGET_UNAL)
#define HAVE_insv (TARGET_UNAL)
#define HAVE_addsicc (TARGET_CADD)
#define HAVE_movqicc (TARGET_CMOV)
#define HAVE_movhicc (TARGET_CMOV)
#define HAVE_movsicc (TARGET_CMOV)
#define HAVE_cbranchsi4 1
#define HAVE_cbranchsf4 (TARGET_HARD_FLOAT)
#define HAVE_cstoresi4 (TARGET_SETCC)
#define HAVE_jump 1
#define HAVE_indirect_jump 1
#define HAVE_tablejump 1
#define HAVE_casesi (TARGET_CASEI)
#define HAVE_doloop_end (TARGET_LOOP)
#define HAVE_call 1
#define HAVE_call_value 1
#define HAVE_return_internal 1
#define HAVE_eh_return 1
#define HAVE_sibcall 1
#define HAVE_sibcall_value 1
#define HAVE_ba2_mulhlsu (TARGET_DSP)
#define HAVE_smaxsi3 (TARGET_DSP)
#define HAVE_sminsi3 (TARGET_DSP)
#define HAVE_sync_compare_and_swapsi 1
#define HAVE_sync_compare_and_swap_ccsi 1
#define HAVE_sync_new_addsi 1
extern rtx        gen_blockage                   (void);
extern rtx        gen_extv_internal              (rtx, rtx, rtx, rtx);
extern rtx        gen_insv_internal              (rtx, rtx, rtx, rtx);
extern rtx        gen_cadd                       (rtx, rtx, rtx, rtx, rtx);
extern rtx        gen_cmov_qi                    (rtx, rtx, rtx, rtx, rtx);
extern rtx        gen_cmov_hi                    (rtx, rtx, rtx, rtx, rtx);
extern rtx        gen_cmov_si                    (rtx, rtx, rtx, rtx, rtx);
extern rtx        gen_extendqisi2                (rtx, rtx);
extern rtx        gen_extendhisi2                (rtx, rtx);
extern rtx        gen_zero_extendqisi2           (rtx, rtx);
extern rtx        gen_zero_extendhisi2           (rtx, rtx);
extern rtx        gen_ashlsi3                    (rtx, rtx, rtx);
extern rtx        gen_ashrsi3                    (rtx, rtx, rtx);
extern rtx        gen_lshrsi3                    (rtx, rtx, rtx);
extern rtx        gen_rotrsi3                    (rtx, rtx, rtx);
extern rtx        gen_andqi3                     (rtx, rtx, rtx);
extern rtx        gen_andhi3                     (rtx, rtx, rtx);
extern rtx        gen_andsi3                     (rtx, rtx, rtx);
extern rtx        gen_iorqi3                     (rtx, rtx, rtx);
extern rtx        gen_iorhi3                     (rtx, rtx, rtx);
extern rtx        gen_iorsi3                     (rtx, rtx, rtx);
extern rtx        gen_xorqi3                     (rtx, rtx, rtx);
extern rtx        gen_xorhi3                     (rtx, rtx, rtx);
extern rtx        gen_xorsi3                     (rtx, rtx, rtx);
extern rtx        gen_one_cmplqi2                (rtx, rtx);
extern rtx        gen_one_cmplhi2                (rtx, rtx);
extern rtx        gen_one_cmplsi2                (rtx, rtx);
extern rtx        gen_negdi2                     (rtx, rtx);
extern rtx        gen_negsi2                     (rtx, rtx);
extern rtx        gen_adddi3                     (rtx, rtx, rtx);
extern rtx        gen_addsi3                     (rtx, rtx, rtx);
extern rtx        gen_subdi3                     (rtx, rtx, rtx);
extern rtx        gen_subsi3                     (rtx, rtx, rtx);
extern rtx        gen_mulsi3                     (rtx, rtx, rtx);
extern rtx        gen_mulsidi3                   (rtx, rtx, rtx);
extern rtx        gen_smulsi3_highpart           (rtx, rtx, rtx);
extern rtx        gen_umulsidi3                  (rtx, rtx, rtx);
extern rtx        gen_umulsi3_highpart           (rtx, rtx, rtx);
extern rtx        gen_divsi3                     (rtx, rtx, rtx);
extern rtx        gen_udivsi3                    (rtx, rtx, rtx);
extern rtx        gen_modsi3                     (rtx, rtx, rtx);
extern rtx        gen_umodsi3                    (rtx, rtx, rtx);
extern rtx        gen_casesi_internal            (rtx, rtx, rtx, rtx);
extern rtx        gen_doloop_end_internal        (rtx, rtx, rtx);
extern rtx        gen_call_value_ba              (rtx, rtx, rtx);
extern rtx        gen_return                     (void);
extern rtx        gen_eh_set_lr                  (rtx);
extern rtx        gen_ffssi2                     (rtx, rtx);
extern rtx        gen_clzsi2                     (rtx, rtx);
extern rtx        gen_nop                        (void);
extern rtx        gen_bitrev                     (rtx, rtx);
extern rtx        gen_swab                       (rtx, rtx);
extern rtx        gen_flb                        (rtx, rtx, rtx);
extern rtx        gen_ba2_mac                    (rtx, rtx);
extern rtx        gen_ba2_macs                   (rtx, rtx);
extern rtx        gen_ba2_macsu                  (rtx, rtx);
extern rtx        gen_ba2_macuu                  (rtx, rtx);
extern rtx        gen_ba2_smactt                 (rtx, rtx);
extern rtx        gen_ba2_smacbb                 (rtx, rtx);
extern rtx        gen_ba2_smactb                 (rtx, rtx);
extern rtx        gen_ba2_umactt                 (rtx, rtx);
extern rtx        gen_ba2_umacbb                 (rtx, rtx);
extern rtx        gen_ba2_umactb                 (rtx, rtx);
extern rtx        gen_ba2_msu                    (rtx, rtx);
extern rtx        gen_ba2_msus                   (rtx, rtx);
extern rtx        gen_ba2_mfmac                  (rtx);
extern rtx        gen_ba2_mtmac                  (rtx);
extern rtx        gen_addsf3                     (rtx, rtx, rtx);
extern rtx        gen_subsf3                     (rtx, rtx, rtx);
extern rtx        gen_mulsf3                     (rtx, rtx, rtx);
extern rtx        gen_divsf3                     (rtx, rtx, rtx);
extern rtx        gen_lrintsfsi2                 (rtx, rtx);
extern rtx        gen_floatsisf2                 (rtx, rtx);
extern rtx        gen_load_gotsi                 (rtx, rtx, rtx);
extern rtx        gen_set_fdreg                  (rtx, rtx, rtx);
extern rtx        gen_set_fdreg_local            (rtx, rtx, rtx);
extern rtx        gen_set_gp                     (rtx);
extern rtx        gen_ba2_slls                   (rtx, rtx, rtx);
extern rtx        gen_ba2_sqr                    (rtx, rtx);
extern rtx        gen_ba2_sqra                   (rtx, rtx, rtx);
extern rtx        gen_ba2_abs                    (rtx, rtx);
extern rtx        gen_ba2_neg                    (rtx, rtx);
extern rtx        gen_ba2_mulas                  (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_muluas                 (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_mulras                 (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_muluras                (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_adds                   (rtx, rtx, rtx);
extern rtx        gen_ba2_subs                   (rtx, rtx, rtx);
extern rtx        gen_ba2_mulsu                  (rtx, rtx, rtx);
extern rtx        gen_ba2_mulhsu                 (rtx, rtx, rtx);
extern rtx        gen_ba2_mulhlsu_1              (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_smultt                 (rtx, rtx, rtx);
extern rtx        gen_ba2_smultb                 (rtx, rtx, rtx);
extern rtx        gen_mulhisi3                   (rtx, rtx, rtx);
extern rtx        gen_ba2_smulbb                 (rtx, rtx, rtx);
extern rtx        gen_ba2_smulwb                 (rtx, rtx, rtx);
extern rtx        gen_ba2_smulwt                 (rtx, rtx, rtx);
extern rtx        gen_ba2_umultt                 (rtx, rtx, rtx);
extern rtx        gen_ba2_umultb                 (rtx, rtx, rtx);
extern rtx        gen_umulhisi3                  (rtx, rtx, rtx);
extern rtx        gen_ba2_umulbb                 (rtx, rtx, rtx);
extern rtx        gen_ba2_umulwb                 (rtx, rtx, rtx);
extern rtx        gen_ba2_umulwt                 (rtx, rtx, rtx);
extern rtx        gen_ba2_max                    (rtx, rtx, rtx);
extern rtx        gen_ba2_min                    (rtx, rtx, rtx);
extern rtx        gen_ba2_lim                    (rtx, rtx, rtx);
extern rtx        gen_ba2_smadtt                 (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_smadtb                 (rtx, rtx, rtx, rtx);
extern rtx        gen_maddhisi4                  (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_smadbb                 (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_smadwb                 (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_smadwt                 (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_umadtt                 (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_umadtb                 (rtx, rtx, rtx, rtx);
extern rtx        gen_umaddhisi4                 (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_umadbb                 (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_umadwb                 (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_umadwt                 (rtx, rtx, rtx, rtx);
extern rtx        gen_ba2_lwza                   (rtx, rtx, rtx);
extern rtx        gen_ba2_swa                    (rtx, rtx, rtx);
extern rtx        gen_prologue                   (void);
extern rtx        gen_epilogue                   (void);
extern rtx        gen_sibcall_epilogue           (void);
extern rtx        gen_movqi                      (rtx, rtx);
extern rtx        gen_movhi                      (rtx, rtx);
extern rtx        gen_movsi                      (rtx, rtx);
extern rtx        gen_movdi                      (rtx, rtx);
extern rtx        gen_movsf                      (rtx, rtx);
extern rtx        gen_movdf                      (rtx, rtx);
extern rtx        gen_extv                       (rtx, rtx, rtx, rtx);
extern rtx        gen_extzv                      (rtx, rtx, rtx, rtx);
extern rtx        gen_insv                       (rtx, rtx, rtx, rtx);
extern rtx        gen_addsicc                    (rtx, rtx, rtx, rtx);
extern rtx        gen_movqicc                    (rtx, rtx, rtx, rtx);
extern rtx        gen_movhicc                    (rtx, rtx, rtx, rtx);
extern rtx        gen_movsicc                    (rtx, rtx, rtx, rtx);
extern rtx        gen_cbranchsi4                 (rtx, rtx, rtx, rtx);
extern rtx        gen_cbranchsf4                 (rtx, rtx, rtx, rtx);
extern rtx        gen_cstoresi4                  (rtx, rtx, rtx, rtx);
extern rtx        gen_jump                       (rtx);
extern rtx        gen_indirect_jump              (rtx);
extern rtx        gen_tablejump                  (rtx, rtx);
extern rtx        gen_casesi                     (rtx, rtx, rtx, rtx, rtx);
extern rtx        gen_doloop_end                 (rtx, rtx, rtx, rtx, rtx);
#define GEN_CALL(A, B, C, D) gen_call ((A), (B))
extern rtx        gen_call                       (rtx, rtx);
#define GEN_CALL_VALUE(A, B, C, D, E) gen_call_value ((A), (B), (C))
extern rtx        gen_call_value                 (rtx, rtx, rtx);
extern rtx        gen_return_internal            (rtx);
extern rtx        gen_eh_return                  (rtx);
#define GEN_SIBCALL(A, B, C, D) gen_sibcall ((A), (B))
extern rtx        gen_sibcall                    (rtx, rtx);
#define GEN_SIBCALL_VALUE(A, B, C, D, E) gen_sibcall_value ((A), (B), (C))
extern rtx        gen_sibcall_value              (rtx, rtx, rtx);
extern rtx        gen_ba2_mulhlsu                (rtx, rtx, rtx);
extern rtx        gen_smaxsi3                    (rtx, rtx, rtx);
extern rtx        gen_sminsi3                    (rtx, rtx, rtx);
extern rtx        gen_sync_compare_and_swapsi    (rtx, rtx, rtx, rtx);
extern rtx        gen_sync_compare_and_swap_ccsi (rtx, rtx, rtx, rtx);
extern rtx        gen_sync_new_addsi             (rtx, rtx, rtx);

#endif /* GCC_INSN_FLAGS_H */

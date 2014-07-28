;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; Use of this source code is subject to the terms of your Microsoft Windows CE
; Source Alliance Program license form.  If you did not accept the terms of
; such a license, you are not authorized to use this source code.
;
;  Richard Chinn
;  BSQUARE CORPORATION
;  January 2, 1998
;
;  fpcommon.h common definitions used in i64 conversion routines
;


;;
;;  Operation code defines.  These must match with the FP_CODE enumerated
;;  type in fpraise.c.
;;
Fn_pos          EQU     20               ;; Function (operation) code position

_FpDToI64       EQU     ( 9 << Fn_pos)   ;; Convert Double To __int64
_FpDToU64       EQU     (12 << Fn_pos)   ;; Convert Double To unsigned __int64
_FpSToI64       EQU     (18 << Fn_pos)   ;; Convert Single To __int64
_FpSToU64       EQU     (20 << Fn_pos)   ;; Convert Single To unsigned __int64
_FpI64ToD       EQU     (22 << Fn_pos)   ;; Convert __int64 To Double
_FpI64ToS       EQU     (23 << Fn_pos)   ;; Convert __int64 To Single
_FpU64ToD       EQU     (24 << Fn_pos)   ;; Convert unsigned __int64 To Double
_FpU64ToS       EQU     (25 << Fn_pos)   ;; Convert unsigned __int64 To Single


;; FP Exception Causes (must agree with fpraise.c FPExInfo)
;; Note that this is modelled after the ARM 7500FE FPSR Cumulative exception
;; flags byte.  The semantics here are slightly different.  If a bit is set
;; here, it means that the corresponding exception occurred during the current
;; FP operation.  If a bit is clear, it means that the corresponding exception
;; did not occur during the current FP operation.  The bits are set regardless
;; of which exceptions are enabled, etc.  Raising exceptions, checking enabled
;; exceptions, and updating status and cause bits is done in fpraise.c which
;; maintains the "true" FPSCR.
INX_bit         EQU     (1 << 4)      ;; Inexact
UNF_bit         EQU     (1 << 3)      ;; Underflow
OVF_bit         EQU     (1 << 2)      ;; Overflow
DVZ_bit         EQU     (1 << 1)      ;; Zero Divide
IVO_bit         EQU     (1 << 0)      ;; Invalid Operation

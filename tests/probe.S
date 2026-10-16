/* The breakpoint probe of PACE's helper DLL, instruction for instruction:
 * the debug registers point at probe_bp. If the breakpoint fires,
 * __C_specific_handler unwinds to probe_hit and the probe returns 1,
 * otherwise it returns its return address.
 */
	.text
	.globl	probe
	.globl	probe_bp
	.def	probe;	.scl	2;	.type	32;	.endef
	.seh_proc	probe
probe:
	.seh_handler	__C_specific_handler, @except
	.seh_endprologue
	xchg	%ax, %ax
probe_bp:
	nop
	movq	(%rsp), %rax
	jmp	1f
probe_hit:
	movl	$1, %eax
1:	ret
	.seh_handlerdata
	.long	1
	.rva	probe_bp, probe_hit, probe_filter, probe_hit
	.text
	.seh_endproc

/* the __except filter: EXCEPTION_EXECUTE_HANDLER for single step only */
	.def	probe_filter;	.scl	3;	.type	32;	.endef
probe_filter:
	movq	(%rcx), %rax
	xorl	%ecx, %ecx
	cmpl	$0x80000004, (%rax)
	sete	%cl
	movl	%ecx, %eax
	ret

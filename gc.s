    global gcalloc
    global __gcal__
    extern error
    extern mns
    extern mnsweep
    extern mnscfr
    extern __heapsize__
    extern __heap__
    extern __free__
    global __frame__
    extern __gc__
    extern __gcstack__
    extern __gcframe__
    global gcbasesz
    extern foo
    section .data
; data here
gcoffset dq 0
gcretadr dq 0
gcbasept dq 0
gcbasesz dq 0
gcchnksz dq 0
gcstrrax dq 0
__gcal__ dq 0
__frame__ dq 0
    section .text
gcalloc:
    mov rdx, [__frame__]
    mov [gcstrrax], rax ;save rax
    mov rax, [__free__] ;first grab offset
    mov rbx, [__heapsize__]
    sub rbx, 256 ; good measure usually
    cmp rax, rbx ;if heap is full...
    jge gcmarkertime ;do a garbage collection return
gcalloc0:
    call mnscfr ;ensures some safety things
    mov rdi, [__free__] ;check free oncemore
    mov rbx, [__heapsize__]
    sub rbx, 256 ; good measure usually
    cmp rdi, rbx ;check if still 98% full
    jg errmemfull
    add rdi, [__heap__]   ;set it to the free location
    mov rbx, [__free__]        ;update __free__
    add rbx, 16
    mov [__free__], rbx
    mov rax, [gcstrrax] ;restore rax
    ret

gcmarkertime:
    call __gc__
    jmp gcalloc0

gcmarkpre:
    mov rax, [__gcal__]
    add rax, 1
    mov [__gcal__], rax
    ; use r15 to store __gcstack__ and r14 for __gcframe__
    mov r15, [__gcstack__]
    mov r14, [__gcframe__]
    ; we would like to restore rax briefly to push
    ; we also save r10
    mov r13, r15
    sub r13, r14
    mov rax, [gcstrrax]
    mov [r13 - 8], rax
    mov rax, r10
    mov [r13 - 16], rax
    add r14, 16
    ; next begin the loop parameters for marks
    mov r11, r14
    ;shr r11, 3 ; get stack item count
    mov r10, r15 ;r10 is our pointer to stack
    ;sub r10, 8
gcmarkloop:
    cmp r11, 0
    ;mov rdi, r11
    ;mov rsi, r10
    je gcmark1
    mov r10, r15
    mov rax, [r10] ;get next stack object

    push rax
    push rbx
    mov rdi, rax
    call foo
    pop rbx
    pop rax

    cmp rax, 0
    je gcmarkloop1 ;if zero, then we don't take address
    mov rbx, [rax]
    mov rax, rbx

    push rax
    push rbx
    mov rdi, rax
    call foo
    pop rbx
    pop rax

    and rbx, 7
    cmp rbx, 0
    je gcmarkloop1
    ;cmp rbx, 7
    ;je gcinccounter
    mov rdi, rax
    push r10
    push r11
    push rax
    push r14
    push r15
    call mns ;call marker
    pop r15
    pop r14
    pop rax
    pop r11
    pop r10
gcmarkloop1:
    mov [r10], rax ;write back to stack
    ;call bar
    sub r11, 8 ;decrease counter
    sub r10, 8
    jmp gcmarkloop ;go back to whence we came
gcmark1:
    ; time to sweep
    mov rdi, 0
    mov [__free__], rdi ;set sweep's free tracker to 0
    call mnsweep
gcmarkpost:
    ; there's nothing we need to do now, really.
gcmark0: ;return address after mark-sweep
    jmp gcalloc0 ;return to allocator

errmemfull:
    mov rdi, 0x90 ;error code for heap exhausted
    mov rsi, [__free__] ;location
    call error

errobjsize:
    mov rdi, 0xA0 ;error code for heap object too big
    mov rsi, rcx ;size
    call error

gcinccounter:
    sub r11, 16
    sub r10, 16
    jmp gcmarkloop
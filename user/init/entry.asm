global  _start

section .text
_start:
    ; By default, binaries in x64OS runs in linux compatiblity mode(kernel simulates linux syscalls)
    mov     rax, 1                     ; system call 1 is write
    mov     rdi, 1                     ; file handle 1 is stdout
    mov     rsi, message               ; address of magic string to output
    mov     rdx, message_end - message ; number of bytes
    syscall                            ; invoke operating system to do the write
    ; At this moment, x64OS kernel now thinks that this is a compatible binary and waits for exit syscall
    mov     eax, 60                    ; system call 60 is exit
    mov     rdi, _start2               ; exit code is address of _start2(x64OS sets RIP to it, linux just kills binary)
    syscall                            ; invoke operating system to "exit"
extern _start3
_start2:
    jmp _start3
.1:
    jmp .1
message:
        db      "This binary is only compatible with x64OS kernel.", 10      ; note the newline at the end
message_end:
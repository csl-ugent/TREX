#include <cstring>

extern "C" void mymemcpy();
extern "C" void f1();
extern "C" void f2();
extern "C" void f3();

asm(R"(
    .section .text
    .globl mymemcpy
    .type mymemcpy, @function

.balign 16; mymemcpy:
.balign 16;    mov rax, qword ptr [rsi]
.balign 16;    mov qword ptr [rdi], rax

.balign 16;    ret
)");

asm(R"(
    .section .text
    .globl f1
    .type f1, @function

.balign 16; f1:
.balign 16;    sub rsp, 16

.balign 16;    mov rax, 10
.balign 16;    mov qword ptr [rsp], rax

.balign 16;    lea rsi, qword ptr [rsp]
.balign 16;    lea rdi, qword ptr [rsp + 8]
.balign 16;    call mymemcpy

.balign 16;    mov rax, qword ptr [rsp + 8]

.balign 16;    add rsp, 16
.balign 16;    ret
)");

asm(R"(
    .section .text
    .globl f2
    .type f2, @function

.balign 16; f2:
.balign 16;    sub rsp, 16

.balign 16;    mov rax, 10
.balign 16;    mov qword ptr [rsp], rax

.balign 16;    lea rsi, qword ptr [rsp]
.balign 16;    lea rdi, qword ptr [rsp + 8]
.balign 16;    call mymemcpy

.balign 16;    mov rax, qword ptr [rsp + 8]

.balign 16;    add rsp, 16
.balign 16;    ret
)");

asm(R"(
    .section .text
    .globl f3
    .type f3, @function

.balign 16; f3:
.balign 16;    sub rsp, 16

.balign 16;    mov rax, 10
.balign 16;    mov qword ptr [rsp], rax

.balign 16;    lea rsi, qword ptr [rsp]
.balign 16;    lea rdi, qword ptr [rsp + 8]
.balign 16;    call mymemcpy

.balign 16;    mov rax, qword ptr [rsp + 8]

.balign 16;    add rsp, 16
.balign 16;    ret
)");

int main() {
  f1();
  f2();
  f3();

  return 0;
}

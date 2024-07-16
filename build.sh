#!/bin/bash

clang-18 -O0 -nostdlib -c -o main.o main.c
as -nostdlib -r -o hello.o hello.asm
clang-18 -o main main.o hello.o

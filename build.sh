#!/bin/bash

clang-18 -nostdlib -c -o main.o main.c
clang-18 -nostdlib -c -o hello.o hello.S
clang-18 -o main main.o hello.o

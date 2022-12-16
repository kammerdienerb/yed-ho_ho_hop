#!/usr/bin/env bash
gcc -o yule_2022.so yule_2022.c $(yed --print-cflags) $(yed --print-ldflags)

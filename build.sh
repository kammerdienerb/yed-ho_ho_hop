#!/usr/bin/env bash
gcc -o ho_ho_hop.so ho_ho_hop.c $(yed --print-cflags) $(yed --print-ldflags)

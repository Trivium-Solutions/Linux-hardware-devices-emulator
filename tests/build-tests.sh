#!/usr/bin/env sh
MODDIR=../kernel
LNXDIR=./linux
gcc -g -I. -I${MODDIR} pair_parser.c ${MODDIR}/hwe_utils.c kernel_utils.c -o pair_parser

#!/bin/sh

rust-lld "$@"

echo RUST_LLD DONE

while [ $# -gt 0 -a "$1" != "-o" ];
do
	shift;
done
OUT="$2"

echo OUT IS $OUT

armv6m-unknown-none-eabi-objcopy --dump-section .rel.nvm_data=provenance-reloc-2 --dump-section .rel.data=provenance-reloc-3 $OUT provenance-reloc
cat provenance-reloc-2 provenance-reloc-3 > provenance-reloc-4
truncate -s 5K provenance-reloc-4
armv6m-unknown-none-eabi-objcopy --update-section .rel_flash=provenance-reloc-4 $OUT


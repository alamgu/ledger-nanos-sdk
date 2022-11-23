#!/bin/sh

set -eu

LD=${LD:-rust-ldd}
# Needed because LLD gets behavior from argv[0]
LD=${LD/ld/lld}
${LD} "$@"

echo RUST_LLD DONE

while [ $# -gt 0 -a "$1" != "-o" ];
do
	shift;
done
OUT="$2"

echo OUT IS $OUT

${OBJCOPY} --dump-section .rel.nvm_data=provenance-reloc-2 --dump-section .rel.data=provenance-reloc-3 $OUT provenance-reloc || true
cat provenance-reloc-2 provenance-reloc-3 > provenance-reloc-4 || true
truncate -s 7K provenance-reloc-4
${OBJCOPY} --update-section .rel_flash=provenance-reloc-4 $OUT


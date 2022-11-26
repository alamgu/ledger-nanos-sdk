#!/usr/bin/env bash

set -eu

LD=${LD:-rust-lld}
# Needed because LLD gets behavior from argv[0]
LD=${LD/-ld/-lld}
${LD} "$@"

echo RUST_LLD DONE

while [ $# -gt 0 -a "$1" != "-o" ];
do
	shift;
done
OUT="$2"

echo OUT IS $OUT

# the relocations for the constants section are required
${OBJCOPY} --dump-section .rel.rodata=app-reloc-2 $OUT app-reloc
# there might not _be_ nonempty .data or .nvm_data sections, so there might be no relocations for it; fail gracefully.
${OBJCOPY} --dump-section .rel.data=app-reloc-3 $OUT app-reloc || true
${OBJCOPY} --dump-section .rel.nvm_data=app-reloc-4 $OUT app-reloc || true
# Concatenate the relocation sections; this should still write app-reloc-4 even if app-reloc-3 doesn't exist.
cat app-reloc-2 app-reloc-3 app-reloc-4 > app-reloc-concat || true
# pad the relocs out to size - we should probably make some way to adjust this size from the source.
truncate -s 7K app-reloc-concat
# and write the relocs to their section in the flash image.
${OBJCOPY} --update-section .rel_flash=app-reloc-concat $OUT

#!/bin/sh

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
${OBJCOPY} --dump-section .rel.nvm_data=provenance-reloc-2 $OUT provenance-reloc
# there might not _be_ a nonempty .data section, so there might be no relocations for it; fail gracefully.
${OBJCOPY} --dump-section .rel.data=provenance-reloc-3 $OUT provenance-reloc || true
# Concatenate the relocation sections; this should still write provenance-reloc-4 even if provenance-reloc-3 doesn't exist.
cat provenance-reloc-2 provenance-reloc-3 > provenance-reloc-4 || true
# pad the relocs out to size - we should probably make some way to adjust this size from the source.
truncate -s 7K provenance-reloc-4
# and write the relocs to their section in the flash image.
${OBJCOPY} --update-section .rel_flash=provenance-reloc-4 $OUT


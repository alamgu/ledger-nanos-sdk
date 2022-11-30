#include <elf.h>
#include <stdbool.h>
#include "exceptions.h"
#include "os_apilevel.h"
#include "string.h"
#include "seproxyhal_protocol.h"
#include "os_id.h"
#include "os_io_usb.h"
#ifdef HAVE_BLE
  #include "ledger_ble.h"
#endif

extern void sample_main();

void os_longjmp(unsigned int exception) {
  longjmp(try_context_get()->jmp_buf, exception);
}

struct SectionSrc;
struct SectionDst;
struct SectionTop;
struct SectionLen;

extern struct SectionDst _rodata;
extern struct SectionTop _erodata;
extern struct SectionSrc _rodata_src;

extern struct SectionDst _data;
#if defined(TARGET_NANOX) || defined(TARGET_NANOS2)
#elif defined(TARGET_NANOS)
extern struct SectionDst _sidata;
extern struct SectionTop _esidata;
#endif
extern struct SectionSrc _sidata_src;

extern struct SectionDst _nvm_data;
extern struct SectionTop _envm_data;
extern struct SectionSrc _nvm_data_src;

extern struct SectionDst _bss;
extern struct SectionTop _ebss;

io_seph_app_t G_io_app;

extern Elf32_Rel _relocs;
extern Elf32_Rel _erelocs;

// TODO get from header
void *pic(void *link_address);
void nvm_write (void *dst_adr, void *src_adr, unsigned int src_len);

void link_pass(
	struct SectionSrc *sec_src,
	struct SectionDst *sec_dst,
	struct SectionTop *sec_top)
{
	uint32_t buf[16];
	typedef typeof(*buf) link_addr_t;
	typedef typeof(*buf) install_addr_t;

	Elf32_Rel *reloc_start = pic(&_relocs);
	Elf32_Rel *reloc_end = ((Elf32_Rel*)pic(&_erelocs-1)) + 1;

    size_t sec_len = (void *)sec_top - (void *)sec_dst;

	// Loop over pages of the .rodata section,
	for (size_t i = 0; i < sec_len; i += sizeof(buf)) {
		// We will want to know if we changed each page, to avoid extra write-backs.
		bool is_changed = 0;

		size_t buf_size = sec_len - i < sizeof(buf)
			? sec_len - i
			: sizeof(buf);

		// Copy over page from *run time* address.
		memcpy(buf, pic(sec_src) + i, buf_size);

		// This is the elf load (*not* elf link or bolos run time!) address of the page
		// we just copied.
		link_addr_t page_link_addr = (link_addr_t)sec_dst + i;

		// Loop over the rodata entries - we could loop over the
		// correct seciton, but this also works.
		for (Elf32_Rel* reloc = reloc_start; reloc < reloc_end; reloc++) {
			// This is the (absolute) elf *load* address of the relocation.
			link_addr_t abs_offset = reloc->r_offset;

			// This is the relative offset on the current page, in
			// bytes.
			size_t page_offset = abs_offset - page_link_addr;

			// This is the relative offset on the current page, in words.
			//
			// Pointers in word_offset should be aligned to 4-byte
			// boundaries because of alignment, so we can just make it
			// uint32_t directly.
			size_t word_offset = page_offset / sizeof(*buf);

			// This includes word_offset < 0 because uint32_t.
			// Assuming no relocations go behind the end address.
			if (word_offset < sizeof(buf) / sizeof(*buf)) {
				link_addr_t old = buf[word_offset];
				install_addr_t new = pic(old);
				is_changed |= (old != new);
				buf[word_offset] = new;
			}
		}
		if (is_changed) {
			nvm_write(pic((void *)sec_dst + i), buf, 64);
		}
	}
}

int c_main(void) {
  __asm volatile("cpsie i");

  // Update pointers for pic(), only issuing nvm_write() if we actually changed a pointer in the block.
  link_pass(&_rodata_src, &_rodata, &_erodata);
#if defined(TARGET_NANOX) || defined(TARGET_NANOS2)
#elif defined(TARGET_NANOS)
  memcpy(&_data, pic(&_sidata), (size_t) ((void *)&_esidata - (void *)&_sidata));
  link_pass(&_sidata_src, &_sidata, &_esidata);
  link_pass(&_nvm_data_src, &_nvm_data, &_erodata);
#endif
  // Also clear the bss section to zeroes, so rust gets it's expected pattern.
  memset(&_bss, 0, (size_t) ((void *)&_ebss - (void *)&_bss));

  // formerly known as 'os_boot()'
  try_context_set(NULL);

  for(;;) {
    BEGIN_TRY {
      TRY {
        // below is a 'manual' implementation of `io_seproxyhal_init`
        check_api_level(CX_COMPAT_APILEVEL);
    #ifdef HAVE_MCU_PROTECT 
        unsigned char c[4];
        c[0] = SEPROXYHAL_TAG_MCU;
        c[1] = 0;
        c[2] = 1;
        c[3] = SEPROXYHAL_TAG_MCU_TYPE_PROTECT;
        io_seproxyhal_spi_send(c, 4);
    #ifdef HAVE_BLE
        unsigned int plane = G_io_app.plane_mode;
    #endif
    #endif
        memset(&G_io_app, 0, sizeof(G_io_app));

    #ifdef HAVE_BLE
        G_io_app.plane_mode = plane;
    #endif
        G_io_app.apdu_state = APDU_IDLE;
        G_io_app.apdu_length = 0;
        G_io_app.apdu_media = IO_APDU_MEDIA_NONE;

        G_io_app.ms = 0;
        io_usb_hid_init();

        USB_power(0);
        USB_power(1);
        
    #ifdef HAVE_BLE 
        LEDGER_BLE_init();
    #endif

        sample_main();
      }
      CATCH(EXCEPTION_IO_RESET) {
        continue;
      }
      CATCH_ALL {
        break;
      }
      FINALLY {
      }
    }
    END_TRY;
  }
  return 0;
}

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

extern char _data[];
extern void _data_len;
extern char _sidata[];
extern char _bss[];
extern void _bss_len;
extern void _rodata;
extern void _rodata_len;
extern void _rodata_src;

io_seph_app_t G_io_app;

extern Elf32_Rel _relocs;
extern Elf32_Rel _relocs_end;

// TODO get from header
void *pic(void *link_address);

void link_pass(void) {
	uint32_t buf[16];
	typedef typeof(*buf) addrdiff_t;
	typedef typeof(*buf) load_addr_t;
	typedef typeof(*buf) run_addr_t;

	Elf32_Rel *reloc_start = pic(&_relocs);
	Elf32_Rel *reloc_end = ((Elf32_Rel*)pic(&_relocs_end-1)) + 1;

	// Loop over pages of the .rodata section,
	for (addrdiff_t i = 0; i < (addrdiff_t)&_rodata_len; i += sizeof(buf)) {
		// We will want to know if we changed each page, to avoid extra write-backs.
		bool is_changed = 0;

		// Copy over page from *run time* address.
		memcpy(buf, pic(&_rodata_src) + i, sizeof(buf));

		// This is the elf load (*not* elf link or bolos run time!) address of the page
		// we just copied.
		load_addr_t page_load_addr = (load_addr_t)&_rodata + i;

		// Loop over the rodata entries - we could loop over the
		// correct seciton, but this also works.
		for (Elf32_Rel* reloc = reloc_start; reloc < reloc_end; reloc++) {
			// This is the (absolute) elf *load* address of the relocation.
			load_addr_t abs_offset = reloc->r_offset;

			// This is the relative offset on the current page, in
			// bytes.
			addrdiff_t page_offset = abs_offset - page_load_addr;

			// This is the relative offset on the current page, in words.
			//
			// Pointers in word_offset should be aligned to 4-byte
			// boundaries because of alignment, so we can just make it
			// uint32_t directly.
			size_t word_offset = page_offset / sizeof(*buf);

			// This includes word_offset < 0 because uint32_t
			if (word_offset < sizeof(buf) / sizeof(*buf)) {
				load_addr_t old = buf[word_offset];
				run_addr_t new = pic(old);
				is_changed |= (old != new);
				buf[word_offset] = new;
			}
		}
		if (is_changed) {
			nvm_write(pic(&_rodata+i), buf, 64);
		}
	}
}

int c_main(void) {
  __asm volatile("cpsie i");

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

	link_pass();

	// Yes, the length is the _address_ of _data_len, becuase it's the definition of the symbol at link time.
	memset(&_bss, 0, (int) &_bss_len);
	memcpy(&_data, pic(&_sidata), (int) &_data_len);

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

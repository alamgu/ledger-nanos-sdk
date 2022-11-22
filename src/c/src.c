#include <elf.h>
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
extern void* _data_len;
extern char _sidata[];
extern char _bss[];
extern void* _bss_len;
extern void* _rodata;
extern void* _rodata_len;
extern void* _rodata_src;

io_seph_app_t G_io_app;

extern Elf32_Rel _relocs;
extern Elf32_Rel _relocs_end;

extern dbg_int(uint32_t len);
extern dbg_mem(void* ptr, uint32_t len);

void link_pass(void) {
	uint32_t buf[16];
	Elf32_Rel *reloc_cursor = pic(&_relocs);
	Elf32_Rel *reloc_end = ((Elf32_Rel*)pic(&_relocs_end-1)) + 1;
	if (reloc_cursor == reloc_end) return;
	uint32_t offset = reloc_cursor->r_offset;

	for(int i=0; i < (uint32_t) &_rodata_len; i+=64) {
		int is_changed = 0;
		memcpy(buf, &_rodata_src + i, 64);
		// Update the pointers from the rodata_rela records
		while(offset < (uint32_t) &_rodata+i+64 && reloc_cursor < reloc_end) {
			if (offset) {
				uint32_t word_offset = (offset - (uint32_t)(&_rodata+i)) / 16;
				uint32_t old = buf[word_offset];
				uint32_t new = pic(old);
				is_changed |= (old != new);
				buf[word_offset] = new;
				reloc_cursor++;
				if ( reloc_cursor < reloc_end ) offset = reloc_cursor->r_offset;
			} else { break; }
		}
		if(is_changed) nvm_write(&_rodata+i, buf, 64);
	}
}

int c_main(void) {
  __asm volatile("cpsie i");

  // dbg_str("BOOTING\n", 8);
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

	dbg_str("LINKING\n", 8);
	link_pass();

	// Yes, the length is the _address_ of _data_len, becuase it's the definition of the symbol at link time.
	memset(&_bss, 0, (int) &_bss_len);
	memcpy(&_data, &_sidata, (int) &_data_len);

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

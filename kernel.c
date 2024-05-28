#include "keyboard_map.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT *COLUMNS_IN_LINE *LINES

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

#define ENTER_KEY_CODE 0x1C

#define PIC1_COMMAND_PORT 0x20
#define PIC2_COMMAND_PORT 0xA0
#define PIC1_DATA_PORT 0x21
#define PIC2_DATA_PORT 0xA1

#define ICW1_INIT 0x11
#define ICW2_PIC1_OFFSET 0x20
#define ICW2_PIC2_OFFSET 0x28
#define ICW3_SETUP_CASCADING 0x00
#define ICW4_ENV_INFO 0x01
#define MASK_INTERRUPTS 0xff
#define ENABLE_IRQ1 0xFD

extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

/* current cursor location */
unsigned int current_loc = 0;
/* video memory begins at address 0xb8000 */
char *video_memory = (char *)0xb8000;

struct IDT_entry
{
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
};

struct IDT_entry IDT[IDT_SIZE];

void idt_init(void)
{
	unsigned long keyboard_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];

	/* populate IDT entry of keyboard's interrupt */
	keyboard_address = (unsigned long)keyboard_handler;
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = INTERRUPT_GATE;
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;

	/* ICW1 - begin initialization */
	write_port(PIC1_COMMAND_PORT, ICW1_INIT);
	write_port(PIC2_COMMAND_PORT, ICW1_INIT);

	/* ICW2 - remap offset address of IDT */
	write_port(PIC1_DATA_PORT, ICW2_PIC1_OFFSET);
	write_port(PIC2_DATA_PORT, ICW2_PIC2_OFFSET);

	/* ICW3 - setup cascading */
	write_port(PIC1_DATA_PORT, ICW3_SETUP_CASCADING);
	write_port(PIC2_DATA_PORT, ICW3_SETUP_CASCADING);

	/* ICW4 - environment info */
	write_port(PIC1_DATA_PORT, ICW4_ENV_INFO);
	write_port(PIC2_DATA_PORT, ICW4_ENV_INFO);
	/* Initialization finished */

	/* mask interrupts */
	write_port(PIC1_DATA_PORT, MASK_INTERRUPTS);
	write_port(PIC2_DATA_PORT, MASK_INTERRUPTS);

	/* fill the IDT descriptor */
	idt_address = (unsigned long)IDT;
	idt_ptr[0] = (sizeof(struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16;

	load_idt(idt_ptr);
}

void kb_init(void)
{
	/* enables only IRQ1 (keyboard)*/
	write_port(PIC1_DATA_PORT, ENABLE_IRQ1);
}

void kprint(const char *message)
{
	unsigned int i = 0;
	while (message[i] != '\0')
	{
		video_memory[current_loc++] = message[i++];
		video_memory[current_loc++] = 0x07;
	}
}

void kprint_newline(void)
{
	unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
	current_loc = current_loc + (line_size - current_loc % (line_size));
}

void clear_screen(void)
{
	unsigned int i = 0;
	while (i < SCREENSIZE)
	{
		video_memory[i++] = ' ';
		video_memory[i++] = 0x07;
	}
}

void keyboard_handler_main(void)
{
	unsigned char status;
	char keycode;

	/* write EOI */
	write_port(PIC1_COMMAND_PORT, 0x20);

	status = read_port(KEYBOARD_STATUS_PORT);
	/* Lowest bit of status will be set if buffer is not empty */
	if (status & 0x01)
	{
		keycode = read_port(KEYBOARD_DATA_PORT);
		if (keycode < 0)
			return;

		if (keycode == ENTER_KEY_CODE)
		{
			kprint_newline();
			return;
		}

		video_memory[current_loc++] = keyboard_map[(unsigned char)keycode];
		video_memory[current_loc++] = 0x07;
	}
}

void kmain(void)
{
	const char *welcome_message = "Simple Kernel with Keyboard Support\n";
	clear_screen();
	kprint(welcome_message);
	kprint_newline();
	kprint_newline();

	idt_init();
	kb_init();

	while (1)
		;
}
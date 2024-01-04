#include "rdtsc.h"
//#DEFINE CPU_BASED_RDTSC_EXITING (1*32+ 12) & 0x1f

int tsc_scale = 10;
int tsc_reset = 500;
static int vmhook_init(void);
static void vmhook_fini(void);
MODULE_DESCRIPTION("Hook and correct KVM TSC timer on X86 platforms. multiple KVM instance supported!");
MODULE_AUTHOR("Heep (Moded by Zhao)");
MODULE_LICENSE("GPL");

module_init(vmhook_init);
module_exit(vmhook_fini);

module_param(tsc_scale,int,0660);
module_param(tsc_reset,int,0660);

static int vmhook_init(void)
{
	return rdtsc_init(tsc_scale,tsc_reset);
}

static void vmhook_fini(void)
{
	rdtsc_fini();
}

#ifndef RDTSC_SHIELD
#define RDTSC_SHIELD
	#include <linux/ktime.h>
	#include <linux/delay.h>
	#include <linux/kvm_types.h>
	#include <linux/kvm_host.h>
	#include <linux/kvm.h>
	#include "linkedList.h"
	#define printkvm(format, ...) printk(KBUILD_MODNAME": "format, ##__VA_ARGS__)
	
	int rdtsc_init(int scale,int reset);
	void rdtsc_fini(void);
#endif
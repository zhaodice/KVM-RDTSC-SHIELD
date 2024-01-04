#include "kallsyms-mod/kallsyms.c"
#include "kallsyms-mod/kallsyms.h"
#include "kallsyms-mod/ksyms.h"
#include "kernel-hook/hook.h"
#include "rdtsc.h"
#include <asm/vmx.h>

MODULE_LICENSE("GPL");
static struct Node* kvm_extra_info_linkedlist = NULL;

#define DEVICE_NAME "rdtsc"
#define CLASS_NAME "rdtsc_class"

static bool hide = 0;
static int major_number;
static struct class* rdtsc_class = NULL;
static struct device* rdtsc_device = NULL;
static ssize_t rdtsc_file_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset);
static struct file_operations rdtsc_fops = {
    .write = rdtsc_file_write,
};

static int setHideStatus(char* chars){
    int val;
    if (sscanf(chars, "%d", &val) == 1) {
        hide = val;
        printkvm("rdtsc hide status: %d\n", hide);
    } else {
        printkvm("Invalid command: %s\n", chars);
        return -EINVAL;
    }

    return 0;
}

static ssize_t rdtsc_file_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset){
    char* data = NULL;

    data = kmalloc(length, GFP_KERNEL);
    if (data == NULL)
        return -ENOMEM;

    if (copy_from_user(data, buffer, length)) {
        kfree(data);
        return -EFAULT;
    }

    setHideStatus(data);

    kfree(data);
    return length;
}

static int rdtsc_file_init(void){
    major_number = register_chrdev(0, DEVICE_NAME, &rdtsc_fops);
    if (major_number < 0) {
        printkvm("Failed to register a major number\n");
        return major_number;
    }

    rdtsc_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(rdtsc_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printkvm("Failed to create device class\n");
        return PTR_ERR(rdtsc_class);
    }

    rdtsc_device = device_create(rdtsc_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(rdtsc_device)) {
        class_destroy(rdtsc_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printkvm("Failed to create the device\n");
        return PTR_ERR(rdtsc_device);
    }

    printkvm("rdtsc module initialized\n");
    return 0;
}

static void rdtsc_file_exit(void){
    device_destroy(rdtsc_class, MKDEV(major_number, 0));
    class_unregister(rdtsc_class);
    class_destroy(rdtsc_class);
    unregister_chrdev(major_number, DEVICE_NAME);
}

u64 tsc_reset_time;
u64 start_rdtsc;

static int tsc_scale;


static struct kvm_extra_info* create_kvm_extra_info(struct kvm *kvm);
static struct kvm_extra_info* get_kvm_extra_info(struct kvm *kvm);
static struct vcpu_extra_info* get_vcpu_extra_info(struct kvm_vcpu *vcpu);

void set_vmx_exec_control(struct kvm_vcpu *vcpu,u32 control);
u32 get_vmx_exec_control(struct kvm_vcpu *vcpu);


struct vcpu_extra_info {
	struct kvm_extra_info *kvmInfo;
	struct kvm_vcpu *vcpu;
	u64 last_detected_tsc;
};

struct kvm_extra_info {
	struct kvm *kvm; // (pid_t)kvm->userspace_pid
	u64 prev_tsc;
	bool hide;
	struct vcpu_extra_info vcpus_extra_info[KVM_MAX_VCPUS];
};
	
static int error_quit(const char *msg){
	printkvm("%s\n", msg);
	return -EFAULT;
}
	
static struct kvm_extra_info* create_kvm_extra_info(struct kvm *kvm) {
	struct kvm_extra_info *kvmInfo;
    kvmInfo = (struct kvm_extra_info*)kmalloc(sizeof(struct kvm_extra_info),__GFP_NORETRY);
    if (kvmInfo == NULL) {
        error_quit("Failed to allocate memory kvm_extra_info\n");
        return NULL;
    }
	kvmInfo->kvm = kvm;
	kvmInfo->prev_tsc = 0;
	kvmInfo->hide = hide;
    return kvmInfo;
}

static struct kvm_extra_info* get_kvm_extra_info(struct kvm *kvm) {
	struct kvm_extra_info *kvmInfo = findValue(kvm_extra_info_linkedlist,kvm);
	//New VM!
	if (kvmInfo==NULL){
		kvmInfo = create_kvm_extra_info(kvm);
		insertNode(kvm_extra_info_linkedlist, kvm, kvmInfo);
		printkvm("New KVM instance detected and attached! Pool Size = %d\n",countLinkedList(kvm_extra_info_linkedlist));
	}
	return kvmInfo;
}
u64 u64_sqrt(u64 x)
{
	u64 op, res, one;

	op = x;
	res = 0;

	one = 1ULL << (BITS_PER_LONG_LONG - 2);
	while (one > op)
		one >>= 2;

	while (one != 0) {
		if (op >= res + one) {
			op = op - (res + one);
			res = res +  2 * one;
		}
		res /= 2;
		one /= 4;
	}
	return res;
}

static struct vcpu_extra_info* get_vcpu_extra_info(struct kvm_vcpu *vcpu) {
	struct vcpu_extra_info *vcpuInfo;
	struct kvm_extra_info *kvmInfo = get_kvm_extra_info(vcpu->kvm);
	vcpuInfo = &kvmInfo->vcpus_extra_info[vcpu->vcpu_idx];
	if (vcpuInfo->vcpu != vcpu) {
		vcpuInfo->vcpu = vcpu;
		//vcpuInfo->called_cpuid = false;
		vcpuInfo->kvmInfo = kvmInfo;
		if(kvmInfo->hide){
			u32 t = get_vmx_exec_control(vcpu);
			if(! (t & CPU_BASED_RDTSC_EXITING)){
				set_vmx_exec_control(vcpu,t | CPU_BASED_RDTSC_EXITING);
			}
		}
	}
	return vcpuInfo;
}

u64 inline blue_pill(struct vcpu_extra_info *vcpuInfo){
	u64 real_rdtsc = rdtsc();
	u64 diff = real_rdtsc - start_rdtsc;
	if(real_rdtsc - vcpuInfo->last_detected_tsc > tsc_reset_time){
		if(diff > tsc_reset_time){
			start_rdtsc = real_rdtsc;
			diff = 0;
			//n = 0;
		}
	}
	
	diff /= tsc_scale;
	
	/*
	diff = diff * (diff/500);
	if(start_rdtsc + diff > real_rdtsc){
		return real_rdtsc;
	}*/
	return start_rdtsc + diff;
}


DEFINE_STATIC_FUNCTION_HOOK(int, kvm_emulate_cpuid, struct kvm_vcpu *vcpu)
{
	DEFINE_ORIGINAL(kvm_emulate_cpuid);
	struct vcpu_extra_info *vcpuInfo = get_vcpu_extra_info(vcpu);
	if (vcpu->arch.regs[VCPU_REGS_RAX] == 0){
		vcpuInfo->last_detected_tsc = rdtsc();
	}
	return orig_fn(vcpu);
}

DEFINE_STATIC_FUNCTION_HOOK(void, kvm_arch_pre_destroy_vm, struct kvm *kvm)
{
	DEFINE_ORIGINAL(kvm_arch_pre_destroy_vm);

	orig_fn(kvm);
	
	printkvm("Removing destroied KVM instance...\n");
	removeValue(kvm_extra_info_linkedlist,kvm,true);
}


DEFINE_STATIC_FUNCTION_HOOK(int, kvm_get_msr_common, struct kvm_vcpu *vcpu, struct msr_data *msr_info){
	DEFINE_ORIGINAL(kvm_get_msr_common);
	//I don't know how to handle MSR_IA32_APERF , or it is not need to
	if(msr_info->index == MSR_IA32_TSC){
		msr_info->data = blue_pill(get_vcpu_extra_info(vcpu));
		return 0;
	}
	return orig_fn(vcpu,msr_info);
}

DEFINE_STATIC_FUNCTION_HOOK(int, handle_rdtsc, struct kvm_vcpu *vcpu){
	u64 fake_rdtsc = blue_pill(get_vcpu_extra_info(vcpu));
	vcpu->arch.regs[VCPU_REGS_RAX] = fake_rdtsc & -1u; //L
	vcpu->arch.regs[VCPU_REGS_RDX] = (fake_rdtsc >> 32) & -1u;  //H
	return kvm_skip_emulated_instruction(vcpu);
}

/*
DEFINE_STATIC_FUNCTION_HOOK(int, handle_rdtscp, struct kvm_vcpu *vcpu){
	DEFINE_ORIGINAL(handle_rdtscp);
	return orig_fn(vcpu);
}
*/
static const fthinit_t hook_list[] = {
	HLIST_NAME_ENTRY(handle_rdtsc),
	//HLIST_NAME_ENTRY(handle_rdtscp),
	HLIST_NAME_ENTRY(kvm_get_msr_common),
	HLIST_NAME_ENTRY(kvm_emulate_cpuid),
	HLIST_NAME_ENTRY(kvm_arch_pre_destroy_vm),
	//HLIST_NAME_ENTRY(copy_strings),
	
};

int rdtsc_init(int scale,int reset){
	int ret;
	printkvm("RDTSC Shield is loading ...\n");
	
	tsc_scale = scale;
	
	start_rdtsc = rdtsc();
	ssleep(1);
	tsc_reset_time = (rdtsc() - start_rdtsc)/1000/1000;
	
	//do spoofing if someone use "rdtsc" in [reset]us twice,it aims to calculate how much tsc 
	tsc_reset_time = tsc_reset_time*reset;

	kvm_extra_info_linkedlist = createLinkedList();

	if ((ret = init_kallsyms()))
		return ret;
	//kvm_set_tsc_khz = (typeof(kvm_set_tsc_khz))kallsyms_lookup_name("kvm_set_tsc_khz");
	ret = start_hook_list(hook_list, ARRAY_SIZE(hook_list));
	if (ret == -1)
		return error_quit("Last error: Failed to lookup symbols!");
	else if (ret == 1)
		return error_quit("Last error: Failed to call ftrace_set_filter_ip! (1)");
	else if (ret == 2)
		return error_quit("Last error: Failed to call ftrace_set_filter_ip! (2)");
	rdtsc_file_init();
	printkvm("RDTSC Shield initialized! tsc_scale=%d,tsc_reset_time=%lld\n",tsc_scale,tsc_reset_time);
	return 0;
}

void rdtsc_fini(void){
	int ret;
	printkvm("RDTSC Shield is unloading...\n");
	ret = end_hook_list(hook_list, ARRAY_SIZE(hook_list));
	if (ret) {
		error_quit("Failed to unregister the ftrace function");
		return;
	}
	
	cleanupLinkedList(kvm_extra_info_linkedlist,true);

	rdtsc_file_exit();
	printkvm("RDTSC Shield unloaded!\n");
}

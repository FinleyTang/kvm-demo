#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define CODE_START 0x0000
#define MEMORY_SIZE 0x10000

void cleanup(int kvm_fd, int vm_fd, int vcpu_fd, char *mem) {
    if (mem) {
        munmap(mem, MEMORY_SIZE);
    }
    if (vcpu_fd >= 0) {
        close(vcpu_fd);
    }
    if (vm_fd >= 0) {
        close(vm_fd);
    }
    if (kvm_fd >= 0) {
        close(kvm_fd);
    }
}

int main() {
    printf("...............kvm demo begin................\n");

    int kvm_fd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm_fd == -1) {
        perror("open /dev/kvm");
        return 1;
    }

    int version = ioctl(kvm_fd, KVM_GET_API_VERSION, 0);
    if (version == -1) {
        perror("KVM_GET_API_VERSION");
        cleanup(kvm_fd, -1, -1, NULL);
        return 1;
    }
    printf("KVM version: %d\n", version);

    int vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0);
    if (vm_fd == -1) {
        perror("KVM_CREATE_VM");
        cleanup(kvm_fd, -1, -1, NULL);
        return 1;
    }

    char *mem = mmap(NULL, MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        cleanup(kvm_fd, vm_fd, -1, NULL);
        return 1;
    }

    struct kvm_userspace_memory_region region = {
        .slot = 0,
        .guest_phys_addr = 0,
        .memory_size = MEMORY_SIZE,
        .userspace_addr = (uintptr_t)mem
    };

    if (ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &region) == -1) {
        perror("KVM_SET_USER_MEMORY_REGION");
        cleanup(kvm_fd, vm_fd, -1, mem);
        return 1;
    }

    int bin_fd = open("guest", O_RDONLY);
    if (bin_fd == -1) {
        perror("open guest.bin");
        cleanup(kvm_fd, vm_fd, -1, mem);
        return 1;
    }

    char *p = mem;
    while (1) {
        ssize_t r = read(bin_fd, p, 4096);
        if (r <= 0) {
            break;
        }
        p += r;
    }
    close(bin_fd);

    int vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, 0);
    if (vcpu_fd == -1) {
        perror("KVM_CREATE_VCPU");
        cleanup(kvm_fd, vm_fd, -1, mem);
        return 1;
    }

    struct kvm_regs regs = {0};
    struct kvm_sregs sregs = {0};
    if (ioctl(vcpu_fd, KVM_GET_SREGS, &sregs) == -1) {
        perror("KVM_GET_SREGS");
        cleanup(kvm_fd, vm_fd, vcpu_fd, mem);
        return 1;
    }

    sregs.cs.selector = CODE_START;
    sregs.cs.base = CODE_START * 16;
    sregs.ss.selector = CODE_START;
    sregs.ss.base = CODE_START * 16;
    sregs.ds.selector = CODE_START;
    sregs.ds.base = CODE_START * 16;
    sregs.es.selector = CODE_START;
    sregs.es.base = CODE_START * 16;
    sregs.fs.selector = CODE_START;
    sregs.fs.base = CODE_START * 16;
    sregs.gs.selector = CODE_START;
    sregs.gs.base = CODE_START * 16;

    if (ioctl(vcpu_fd, KVM_SET_SREGS, &sregs) == -1) {
        perror("KVM_SET_SREGS");
        cleanup(kvm_fd, vm_fd, vcpu_fd, mem);
        return 1;
    }

    regs.rflags = 2;
    regs.rip = 0;
    regs.rax = 2;
    regs.rbx = 2;

    if (ioctl(vcpu_fd, KVM_SET_REGS, &regs) == -1) {
        perror("KVM_SET_REGS");
        cleanup(kvm_fd, vm_fd, vcpu_fd, mem);
        return 1;
    }

    int runsz = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    struct kvm_run *run = mmap(NULL, runsz, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd, 0);
    if (run == MAP_FAILED) {
        perror("mmap kvm_run");
        cleanup(kvm_fd, vm_fd, vcpu_fd, mem);
        return 1;
    }

    while (1) {
        printf("test\n");
        if (ioctl(vcpu_fd, KVM_RUN, 0) == -1) {
            perror("KVM_RUN");
            break;
        }
        switch (run->exit_reason) {

        case KVM_EXIT_IO:
            printf("IO port: %x, data: %x\n", run->io.port, *(int *)((char *)run + run->io.data_offset));
            break;
        case KVM_EXIT_HLT:
            fputs("KVM_EXIT_HLT", stderr);
            return 0;
        case KVM_EXIT_SHUTDOWN:
            cleanup(kvm_fd, vm_fd, vcpu_fd, mem);
            return 0;
        }
    }

    cleanup(kvm_fd, vm_fd, vcpu_fd, mem);
    return 1;
}

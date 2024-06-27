## 代码解释
打开 KVM 设备:


```c
int kvm_fd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
if (kvm_fd == -1) {
    printf("open /dev/kvm error\n");
    return 1;
}
```
打开 KVM 设备文件，如果失败则打印错误信息并退出。

获取 KVM 版本:


```c
int version = ioctl(kvm_fd, KVM_GET_API_VERSION, 0);
printf("KVM version: %d", version);
```
通过 ioctl 获取 KVM 的 API 版本并打印。

创建虚拟机:


```c
int vm_fd;
if ((vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0)) < 0) {
    fprintf(stderr, "failed to create vm: %d\n", errno);
    return 1;
}
```
使用 ioctl 创建虚拟机实例，如果失败则打印错误信息并退出。

分配内存:


```c
char *mem = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
if(mem == NULL){
    fprintf(stderr, "memory allocation failed: %d\n", errno);
    return 1;
}
```
使用 mmap 分配 64 KB 的内存作为虚拟机的内存空间。

设置用户内存区域:


```c
struct kvm_userspace_memory_region region = {
    .slot = 0,
    .guest_phys_addr = 0,
    .memory_size = 0x10000, /* bytes */
    .userspace_addr = (uintptr_t)mem /* start of the userspace allocated memory */
};

if (ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &region) < 0) {
    fprintf(stderr, "ioctl KVM_SET_USER_MEMORY_REGION failed: %d\n", errno);
    return 1;
}
```
配置虚拟机内存区域，并通过 ioctl 设置该内存区域。

加载客户机代码:


```c
int bin_fd = open("guest.bin", O_RDONLY);
if (bin_fd < 0) {
    fprintf(stderr, "cannot open binary file: %d\n", errno);
    return 1;
}
char *p = (char *)mem;
for (;;) {
    int r = read(bin_fd, p, 4096);
    if (r <= 0) {
        break;
    }
    p += r;
}
close(bin_fd);
```
打开并读取客户机二进制文件，将其加载到分配的内存中。

创建 VCPU:


```c
int vcpu_fd;
if ((vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, 0)) < 0) {
    fprintf(stderr, "cannot create vcpu: %d\n", errno);
    return 1;
}
```
设置寄存器:
```c
struct kvm_regs regs;
struct kvm_sregs sregs;
if (ioctl(vcpu_fd, KVM_GET_SREGS, &(sregs)) < 0) {
    perror("cannot get sregs\n");
    return 1;
}

#define CODE_START 0x0000

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

if (ioctl(vcpu_fd, KVM_SET_SREGS, &sregs) < 0) {
    perror("cannot set sregs");
    return 1;
}

regs.rflags = 2;
regs.rip = 0;

if (ioctl(vcpu_fd, KVM_SET_REGS, &(regs)) < 0) {
    perror("KVM SET REGS");
    return 1;
}
```



运行虚拟机:

``` c
int runsz = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
struct kvm_run *run = (struct kvm_run *) mmap(NULL, runsz, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd, 0);

for (;;) {
    sleep(1);
    ioctl(vcpu_fd, KVM_RUN, 0);
    switch (run->exit_reason) {
    case KVM_EXIT_IO:
        printf("IO port: %x, data: %x\n", run->io.port, *(int *)((char *)(run) + run->io.data_offset));
        break;
    case KVM_EXIT_SHUTDOWN:
        return 0;
    }
}
```





## 运行步骤
as -32 guest.S -o guest.o
ld -m elf_i386 --oformat binary -N -e _start -Ttext 0x10000 -o guest guest.o
实际上我发现直接用nasm执行： nasm -f bin guest.asm -o guest 即可

然后执行gcc main.c -o main 

sudo ./main
```
~/mylab/kvm-demo-sample/kvm-demo$ sudo ./main 
...............kvm demo begin................
KVM version: 12
test
IO port: 3f8, data: 4
test
```
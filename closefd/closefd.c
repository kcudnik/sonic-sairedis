#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>   // for user_regs_struct

#include <elf.h>
#include <link.h>

// XXX this only supports x86_64 arch not x86

// http://www.ars-informatica.com/Root/Code/2010_04_18/LinuxPTrace.aspx
// https://gist.github.com/angavrilov/926972
// http://phrack.org/issues/59/8.html
//
// https://gcc.godbolt.org/
// https://www.onlinedisassembler.com/odaweb/
// http://deroko.phearless.org/dt_gnu_hash.txt
// http://dustin.schultz.io/how-is-glibc-loaded-at-runtime.html
// http://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/
// https://stackoverflow.com/questions/25049400/x86-64-bit-assembly-linux-input
// http://web.mit.edu/freebsd/head/sys/sys/elf64.h
// https://github.com/vikasnkumar/hotpatch

#define LOG_ERROR(fmt,...) { printf("ERR " fmt "\n", ##__VA_ARGS__); exit(1); }
#define LOG_DEBUG(fmt,...)   printf("DBG " fmt "\n", ##__VA_ARGS__)

#define STATIC_ASSERT(COND) typedef char static_assertion ## __LINE__[(COND)?1:-1]

STATIC_ASSERT(sizeof(long) == 8);
STATIC_ASSERT(sizeof(unsigned long) == 8);

#define ADDR_SIZE 8

void ptrace_attach(int pid)
{
    if ((ptrace(PTRACE_ATTACH, pid, NULL, NULL)) < 0)
        LOG_ERROR("attach failed to pid %d", pid);

    if (waitpid(pid, NULL, WUNTRACED) < 0)
        LOG_ERROR("wait pid failed failed on pid %d", pid);

    LOG_DEBUG("attached to pid %d", pid);
}

void ptrace_cont(int pid)
{
    if ((ptrace(PTRACE_CONT, pid, NULL, NULL)) < 0)
        LOG_ERROR("continue failed");

    int s = 0;

    while (!WIFSTOPPED(s))
    {
        waitpid(pid, &s, WNOHANG);
    }

    LOG_DEBUG("continue succeeded ond pid %d", pid);
}

void ptrace_detach(int pid)
{
    if (ptrace(PTRACE_DETACH, pid, NULL, NULL) < 0)
        LOG_ERROR("detach failed on pid %d", pid);

    LOG_DEBUG("detatched from pid %d", pid);
}

void ptrace_read(int pid, unsigned long addr, void *data, unsigned int len)
{
    if (len % ADDR_SIZE) // 4 on x86
    {
        // XXX we need to be sure that length is 8 aligned or we will
        // override some local memory

        LOG_DEBUG("WARN ---- reading len %d is not aligned to 8 on addr 0x%lx", len, addr);
    }

    unsigned long count = 0;

    int i = 0;

    long *ptr = data;

    while (count < len)
    {
        long word = ptrace(PTRACE_PEEKTEXT, pid, addr + count, NULL);

        count += ADDR_SIZE; // 4 on x86

        ptr[i++] = word;
    }
}

void ptrace_write(int pid, unsigned long addr, const void *data, unsigned int len)
{
    if (len % ADDR_SIZE)
    {
        // XXX we need to be sure lenght is 8 bytes aligned if it's not
        // then we override some memory in t he process

        LOG_DEBUG("WARN ---- writting len %d not aligned to 8 on addr 0x%lx", len, addr);
    }

    unsigned int count = 0;

    while (count < len)
    {
        long word;

        memcpy(&word, (void*)((unsigned long)data + count), ADDR_SIZE);

        word = ptrace(PTRACE_POKETEXT, pid, addr + count, word);

        count += ADDR_SIZE;
    }
}

struct link_map find_link_map(int pid)
{
    // we hard code that, but we should get that from /proc/pid/exe

    uint64_t base_address = 0x400000; // for x86 0x08048000

    Elf64_Ehdr ehdr;

    ptrace_read(pid, base_address, &ehdr, sizeof(Elf64_Ehdr));

    if (strncmp((const char*)ehdr.e_ident, "\x7F" "ELF", 4) != 0)
        LOG_ERROR("header ad 0x%lx is not .ELF", base_address);

    unsigned long phdr_addr = base_address + ehdr.e_phoff;

    LOG_DEBUG("program header at 0x%lx: off 0x%lx", phdr_addr, ehdr.e_phoff);

    Elf64_Phdr phdr;

    ptrace_read(pid, phdr_addr, &phdr, sizeof(Elf64_Phdr));

    int i = 0;

    while (phdr.p_type != PT_DYNAMIC)
    {
        // LOG_DEBUG("looking at %lx addr", phdr_addr);

        phdr_addr += sizeof(Elf64_Phdr);

        ptrace_read(pid, phdr_addr, &phdr, sizeof(Elf64_Phdr));

        // limit loop to fixed steps in case of infinite loop

        if (i++ > 16)
            LOG_ERROR("failed to get dynamic section header for pid %d", pid);
    }

    LOG_DEBUG("got dynamic header at phdr_addr: 0x%lx phdr.p_vaddr: 0x%lx", phdr_addr, phdr.p_vaddr);

    // now go through dynamic section until we find address of the GOT

    Elf64_Dyn dyn;

    ptrace_read(pid, phdr.p_vaddr, &dyn, sizeof(Elf64_Dyn));

    unsigned long dyn_addr = phdr.p_vaddr;

    // global offset table

    i = 0;

    while (dyn.d_tag != DT_PLTGOT)
    {
        // LOG_DEBUG("looking for GOT 0x%lx", dyn_addr);

        dyn_addr += sizeof(Elf64_Dyn);

        ptrace_read(pid, dyn_addr, &dyn, sizeof(Elf64_Dyn));

        if (i++ > 128)
            LOG_ERROR("failed to get GOT in 128 steps for pid %d", pid);
    }

    struct r_debug rd;

    // find r_map pointer location in struct

    unsigned long offset = (unsigned long)&rd.r_map - (unsigned long)&rd;

    Elf64_Addr got = dyn.d_un.d_ptr + offset;

    LOG_DEBUG("GOT address 0x%lx", got);

    /* now just read first link_map item and return it */

    unsigned long map_addr;

    ptrace_read(pid, got, &map_addr, sizeof(Elf64_Addr));

    LOG_DEBUG("link map address 0x%lx", map_addr);

    struct link_map lm;

    ptrace_read(pid, map_addr, &lm, sizeof(struct link_map));

    return lm;
}

unsigned long symtab = 0;
unsigned long strtab = 0;

// we declare this as tab, sice size is 4 bytes, and ptrace is reading 8 bytes
// so this will prevent memory override
int nchains[2] = { -1, -1 };

void resolv_tables(int pid, const struct link_map *map)
{
    symtab = 0;
    strtab = 0;
    nchains[0] = -1;

    Elf64_Dyn dyn;

    unsigned long addr = (unsigned long)map->l_ld;

    ptrace_read(pid, addr, &dyn, sizeof(Elf64_Dyn));

    while (dyn.d_tag)
    {
        // XXX extracting chain start and end is not straight forward:
        // http://deroko.phearless.org/dt_gnu_hash.txt
        //
        // for our purpose we will just ingore that and read in finite loop

        switch (dyn.d_tag)
        {
            case DT_HASH:

                ptrace_read(pid, dyn.d_un.d_ptr + map->l_addr + 4, nchains, sizeof(nchains) * 2);

                LOG_DEBUG("DT_HASH chains: %d", nchains[0]);
                break;

            case DT_GNU_HASH:

                ptrace_read(pid, dyn.d_un.d_ptr + map->l_addr + 4, nchains, sizeof(nchains) * 2);

                LOG_DEBUG("DT_GNU_HASH chains: %d", nchains[0]);
                break;

            case DT_STRTAB:
                strtab = dyn.d_un.d_ptr;
                break;

            case DT_SYMTAB:
                symtab = dyn.d_un.d_ptr;
                break;

            default:
                break;
        }

        addr += sizeof(Elf64_Dyn);

        ptrace_read(pid, addr, &dyn, sizeof(Elf64_Dyn));
    }

    LOG_DEBUG("sybtab: 0x%lx strtab: 0x%lx chains: %d", symtab, strtab, nchains[0]);

    if (symtab == 0 || strtab == 0)
        LOG_ERROR("failed to resole symbol table for pid %d", pid);
}

unsigned long sym_find(int pid, const struct link_map *map, const char *sym_name)
{
    unsigned int i = 0;

    // how big symbol table is ? we don't know - look resolv_tables
    // check up to 5k symbols to look for requested one

    for (i = 0; i < 5000; ++i)
    {
        Elf64_Sym sym;

        ptrace_read(pid, symtab + (i * sizeof(Elf64_Sym)), &sym, sizeof(Elf64_Sym));

        if (ELF64_ST_TYPE(sym.st_info) != STT_FUNC)
        {
            continue;
        }

        // read symbol name from the string table

        char str[128 + 1];

        ptrace_read(pid, strtab + sym.st_name, str, 128);

        str[128] = 0;

        // if ptrace read will fail it will read 0xff in all bytes

        if (str[0] == -1)
        {
            break;
        }

        // LOG_DEBUG("checking symbol: %s", str);

        if (strcmp(str, sym_name) == 0)
        {
            return map->l_addr + sym.st_value;
        }
    }

    // no symbol found, return 0

    return 0;
}

unsigned long find_sym_in_lib(int pid , const char *sym_name , const char *lib)
{
    struct link_map lm = find_link_map(pid);

    struct link_map *map = &lm;

    unsigned long sym = 0;

    char libname[256 + 1];

    while (!sym && map->l_next)
    {
        ptrace_read(pid, (unsigned long)map->l_next, map, sizeof(struct link_map));

        ptrace_read(pid, (unsigned long)map->l_name, libname, 256);

        libname[256] = 0;

        // if lib is NULL then just check all libs
        // if not NULL, then check only specific lib

        if (lib && strstr(libname, lib) == NULL)
        {
            continue;
        }

        LOG_DEBUG("found '%s' inside lib: %s", lib, libname);

        resolv_tables(pid, map);

        sym = sym_find(pid, map, sym_name);

        LOG_DEBUG("sumbol '%s' address 0x%lx", sym_name, sym);
    }

    return sym;
}

struct user_regs_struct ptrace_get_regs(int pid)
{
    struct user_regs_struct regs;

    LOG_DEBUG("getting registers");

    if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) < 0)
        LOG_ERROR("failed to get registers for pid %d", pid);

    return regs;
}

void ptrace_set_regs(int pid, struct user_regs_struct *regs)
{
    LOG_DEBUG("setting registers");

    if (ptrace(PTRACE_SETREGS, pid, NULL, regs) < 0)
        LOG_DEBUG("failed to set registers for pid %d", pid);
}

void dump_bytes(int pid, unsigned long addr)
{
    unsigned char data[16];

    ptrace_read(pid, addr, data, 16);

    printf("dumpaddr 0x%lx[16]:", addr);

    int i = 0;

    for (i = 0; i < 16; i++)
    {
        printf(" %02X", data[i]);
    }

    printf("\n");
}

void dump_rip_bytes(int pid)
{
    struct user_regs_struct regs = ptrace_get_regs(pid);

    dump_bytes(pid, regs.rip);
}

//uint64_t freespaceaddr(pid_t pid)
//{
//    FILE *fp;
//    char filename[128];
//    char *line = NULL;
//    uint64_t addr;
//    char str[128];
//    char perms[128];
//
//    size_t len;
//    size_t read;
//
//    sprintf(filename, "/proc/%d/maps", pid);
//
//    fp = fopen(filename, "r");
//
//    if (fp == NULL)
//    {
//        printf("failed to open maps for %d\n", pid);
//        exit(1);
//    }
//
//    addr = 0;
//
//    while ((read = getline(&line, &len, fp)) != -1)
//    {
//        // printf("line: %s", line);
//        // address           perms offset   dev   inode   pathname
//        // 00693000-00694000 rw-p  00000000 00:00 0
//
//        if (sscanf(line, "%lx-%*lx %s %*s %s", &addr, perms, str) != 3)
//        {
//            perror("scanf failed\n");
//            exit(1);
//        }
//
//        if (strcmp(str, "00:00") == 0 && strcmp(perms, "rw-p") == 0)
//        {
//            printf("found: line: %s = 0x%lx - %s\n", line, addr, str);
//            // break;
//        }
//
//        addr = 0;
//    }
//
//    fclose(fp);
//
//    if (line)
//    {
//        free(line);
//    }
//
//    return addr;
//}

#define RED_ZONE_SIZE 128

int main(int argc, char **argv)
{
    if (argc < 3)
        LOG_ERROR("usage: %s pid fd", argv[0]);

    int pid;

    if (sscanf(argv[1], "%d", &pid) != 1 || pid < 2)
        LOG_ERROR("failed to parse %s as pid", argv[1]);

    int fd;

    if (sscanf(argv[2], "%d", &fd) != 1 || fd < 0)
        LOG_ERROR("failed to parse %s as fd or fd < 0", argv[2]);

    char fdpath[128];

    sprintf(fdpath, "/proc/%d/fd/%d", pid, fd);

    if (access(fdpath, F_OK) == -1)
    {
        LOG_DEBUG("file descripor %s is not accessible, noop", fdpath);
        // return 0;
    }

    // idea to close handle:
    //
    // - find "close" function address, it only takes 1 argument
    // which is FD and it's located in RDX register on x86_64 arch
    //
    // - alocate some memory on current stack and skip possible RED ZONE
    // - put on stack addres of stack +8 where we put "int 3" trap
    // so after calling "close" "ret" instruction will point to
    // int 3 trap so we can catch that and bring back previous state
    //
    // NOTE: stack needs to be code executable to allow this to happen
    //
    // - set RIP to close function address
    //
    // to extend this logic we could use "memalloc" to allocate
    // memory and put some more data and then do _dl_open to load
    // external so librarh which we can control and do a ot more
    //
    // this is already done: https://github.com/vikasnkumar/hotpatch
    // but somehow it's not working
    //

    ptrace_attach(pid);

    struct link_map lm = find_link_map(pid);

    resolv_tables(pid, &lm);

    unsigned long close_addr = find_sym_in_lib(pid, "close", "libc.so");

    if (close_addr == 0)
        LOG_ERROR("not found 'close' api address for pid %d", pid);

    LOG_DEBUG("found function 'close' addr at: 0x%lx", close_addr);

    struct user_regs_struct regs, original_regs;

    // this is from main thread, other threads are still spinning and can cause
    // some damage, closing handle (i/o) is thread safe at kernel level so this
    // should be good enough

    regs = original_regs = ptrace_get_regs(pid);

    LOG_DEBUG("original regs: RIP: 0x%llx RSP: 0x%llx RDI: 0x%llx", regs.rip, regs.rsp, regs.rdi);

    dump_rip_bytes(pid);

    // on x64 there is concept of RED ZONE on the stack 128 bytes that callee can modify
    // http://eli.thegreenplace.net/2011/09/06/stack-frame-layout-on-x86-64
    // http://eli.thegreenplace.net/2011/01/27/how-debuggers-work-part-2-breakpoints

    // in similar way we can call malloc and in that allocated memory put our code
    // to load any SO if malloc is present (in libc it should be)

    regs.rsp -= RED_ZONE_SIZE + ADDR_SIZE + 8;  // 8 bytes for RET address and extra 8 bytes for (int 3) and aligning code
    regs.rip = close_addr;              // start executing from here
    regs.rdi = (unsigned)fd;            // first argument for close
    regs.rax = 0;                       // must be zero, read description below

    // x64 ABI:
    //
    //    %rax        temporary register; with variable arguments
    //    passes information about the number of vector
    //    registers used; 1st return register
    //
    //    For the record, it happens before calls to void proc() because that
    //    signature in C actually says nothing about proc's arity, and it could
    //    as well be a variadic function, so zeroing rax is necessary. void
    //    proc() is different from void proc(void)
    //
    //    https://stackoverflow.com/questions/693788/is-it-better-to-use-c-void-arguments-void-foovoid-or-not-void-foo
    //    https://stackoverflow.com/questions/6212665/why-is-eax-zeroed-before-a-call-to-printf

    LOG_DEBUG("setting new regs: RIP: 0x%llx RSP: 0x%llx RDI: 0x%llx RAX: 0x%llx", regs.rip, regs.rsp, regs.rdi, regs.rax);

    ptrace_set_regs(pid, &regs);

    // in x64 first argument order is: rdi, rsi, rdx, rcx, r8, r9, stack

    unsigned long retAddr = regs.rsp + 8; // return address should point to "int 3" which is located, after current RSP

    LOG_DEBUG("setting 'close' return address RSP address to 0x%lx at RSP: 0x%llx", retAddr, regs.rsp);

    ptrace_write(pid, regs.rsp, &retAddr, 8);

    LOG_DEBUG("setting int3 code on stack at address 0x%lx", retAddr);

    const char int3[8] = "\xcc\xcc\xcc\xcc\xcc\xcc\xcc\xcc";

    ptrace_write(pid, retAddr, int3, sizeof(int3));

    dump_bytes(pid, regs.rsp);
    dump_bytes(pid, regs.rsp + 8);
    dump_rip_bytes(pid);

    LOG_DEBUG("let process continue to run");

    ptrace_cont(pid);

    regs = ptrace_get_regs(pid);

    LOG_DEBUG("regs after continue: RIP: 0x%llx RSP: 0x%llx RDI: 0x%llx RAX: 0x%llx", regs.rip, regs.rsp, regs.rdi, regs.rax);

    if (regs.rip != retAddr)
        LOG_ERROR("error, expected RIP: 0x%lx but got 0x%llx FATAL", retAddr, regs.rip);

    LOG_DEBUG("close operation result 0x%llx", regs.rax);

    dump_rip_bytes(pid);

    LOG_DEBUG("bring back orignal registers");

    ptrace_set_regs(pid, &original_regs);

    LOG_DEBUG("getting register for check");

    regs = ptrace_get_regs(pid);

    LOG_DEBUG("after regs back: RIP: 0x%llx RSP: 0x%llx RDI: 0x%llx", regs.rip, regs.rsp, regs.rdi);

    ptrace_detach(pid);

    if (access(fdpath, F_OK) != -1)
        LOG_DEBUG("FAILURE: file descripor %s is still present, FATAL", fdpath);

    LOG_DEBUG("SUCCESS: file descriptor %s was closed", fdpath);

    return 0;
}

//
// Dis VM
// File: Sysmod.h
// Author: arr
//

#ifndef _DISVM_SRC_VM_SYS_SYSMOD_H_
#define _DISVM_SRC_VM_SYS_SYSMOD_H_

#include <vector>
#include <disvm.h>
#include <builtin_module.h>

using namespace disvm;
using namespace disvm::runtime;

struct Sys_Qid
{
    big_t path;
    word_t vers;
    word_t qtype;
};
const byte_t Sys_Qid_map[] = { 0 };
struct Sys_Dir
{
    /* vm_string_t */ pointer_t name;
    /* vm_string_t */ pointer_t uid;
    /* vm_string_t */ pointer_t gid;
    /* vm_string_t */ pointer_t muid;
    Sys_Qid qid;
    word_t mode;
    word_t atime;
    word_t mtime;
    byte_t _pad44[4];
    big_t length;
    word_t dtype;
    word_t dev;
};
const byte_t Sys_Dir_map[] = { 0xf0, };
struct Sys_FD
{
    word_t fd;
};
const byte_t Sys_FD_map[] = { 0 };
struct Sys_Connection
{
    /* Sys_FD */ pointer_t dfd;
    /* Sys_FD */ pointer_t cfd;
    /* vm_string_t */ pointer_t dir;
};
const byte_t Sys_Connection_map[] = { 0xe0, };
using Sys_Rread = struct { /* vm_array_t */ pointer_t t0; /* vm_string_t */ pointer_t t1; };
#define Sys_Rread_map {0xc0,}
using Sys_Rwrite = struct { word_t t0; /* vm_string_t */ pointer_t t1; };
#define Sys_Rwrite_map {0x40,}
struct Sys_FileIO
{
    /* vm_channel_t */ pointer_t read;
    /* vm_channel_t */ pointer_t write;
};
using Sys_FileIO_read = struct { word_t t0; word_t t1; word_t t2; /* vm_channel_t */ pointer_t t3; };
#define Sys_FileIO_read_map {0x10,}
using Sys_FileIO_write = struct { word_t t0; /* vm_array_t */ pointer_t t1; word_t t2; /* vm_channel_t */ pointer_t t3; };
#define Sys_FileIO_write_map {0x50,}
const byte_t Sys_FileIO_map[] = { 0xc0, };
void Sys_announce(vm_registers_t &r, vm_t &vm);
struct F_Sys_announce : public vm_frame_base_alloc_t
{
    struct { word_t t0; Sys_Connection t1; }*ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t addr;
};
void Sys_aprint(vm_registers_t &r, vm_t &vm);
struct F_Sys_aprint : public vm_frame_base_alloc_t
{
    /* vm_array_t */ pointer_t* ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t s;
    byte_t vargs;
};
void Sys_bind(vm_registers_t &r, vm_t &vm);
struct F_Sys_bind : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t s;
    /* vm_string_t */ pointer_t on;
    word_t flags;
};
void Sys_byte2char(vm_registers_t &r, vm_t &vm);
struct F_Sys_byte2char : public vm_frame_base_alloc_t
{
    struct { word_t t0; word_t t1; word_t t2; }*ret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t buf;
    word_t n;
};
void Sys_char2byte(vm_registers_t &r, vm_t &vm);
struct F_Sys_char2byte : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    word_t c;
    /* vm_array_t */ pointer_t buf;
    word_t n;
};
void Sys_chdir(vm_registers_t &r, vm_t &vm);
struct F_Sys_chdir : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t path;
};
void Sys_create(vm_registers_t &r, vm_t &vm);
struct F_Sys_create : public vm_frame_base_alloc_t
{
    /* Sys_FD */ pointer_t* ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t s;
    word_t mode;
    word_t perm;
};
void Sys_dial(vm_registers_t &r, vm_t &vm);
struct F_Sys_dial : public vm_frame_base_alloc_t
{
    struct { word_t t0; Sys_Connection t1; }*ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t addr;
    /* vm_string_t */ pointer_t local;
};
void Sys_dirread(vm_registers_t &r, vm_t &vm);
struct F_Sys_dirread : public vm_frame_base_alloc_t
{
    struct { word_t t0; /* vm_array_t */ pointer_t t1; }*ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
};
void Sys_dup(vm_registers_t &r, vm_t &vm);
struct F_Sys_dup : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    word_t old;
    word_t new_;
};
void Sys_export(vm_registers_t &r, vm_t &vm);
struct F_Sys_export : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t c;
    /* vm_string_t */ pointer_t dir;
    word_t flag;
};
void Sys_fauth(vm_registers_t &r, vm_t &vm);
struct F_Sys_fauth : public vm_frame_base_alloc_t
{
    /* Sys_FD */ pointer_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
    /* vm_string_t */ pointer_t aname;
};
void Sys_fd2path(vm_registers_t &r, vm_t &vm);
struct F_Sys_fd2path : public vm_frame_base_alloc_t
{
    /* vm_string_t */ pointer_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
};
void Sys_fildes(vm_registers_t &r, vm_t &vm);
struct F_Sys_fildes : public vm_frame_base_alloc_t
{
    /* Sys_FD */ pointer_t* ret;
    byte_t temps[12];
    word_t fd;
};
void Sys_file2chan(vm_registers_t &r, vm_t &vm);
struct F_Sys_file2chan : public vm_frame_base_alloc_t
{
    /* Sys_FileIO */ pointer_t* ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t dir;
    /* vm_string_t */ pointer_t file;
};
void Sys_fprint(vm_registers_t &r, vm_t &vm);
struct F_Sys_fprint : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
    /* vm_string_t */ pointer_t s;
    byte_t vargs;
};
void Sys_fstat(vm_registers_t &r, vm_t &vm);
struct F_Sys_fstat : public vm_frame_base_alloc_t
{
    struct { word_t t0; byte_t _pad4[4]; Sys_Dir t1; }*ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
};
void Sys_fversion(vm_registers_t &r, vm_t &vm);
struct F_Sys_fversion : public vm_frame_base_alloc_t
{
    struct { word_t t0; /* vm_string_t */ pointer_t t1; }*ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
    word_t msize;
    /* vm_string_t */ pointer_t version;
};
void Sys_fwstat(vm_registers_t &r, vm_t &vm);
struct F_Sys_fwstat : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
    byte_t _pad36[4];
    Sys_Dir d;
};
void Sys_iounit(vm_registers_t &r, vm_t &vm);
struct F_Sys_iounit : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
};
void Sys_listen(vm_registers_t &r, vm_t &vm);
struct F_Sys_listen : public vm_frame_base_alloc_t
{
    struct { word_t t0; Sys_Connection t1; }*ret;
    byte_t temps[12];
    Sys_Connection c;
};
void Sys_millisec(vm_registers_t &r, vm_t &vm);
struct F_Sys_millisec : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
};
void Sys_mount(vm_registers_t &r, vm_t &vm);
struct F_Sys_mount : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
    /* Sys_FD */ pointer_t afd;
    /* vm_string_t */ pointer_t on;
    word_t flags;
    /* vm_string_t */ pointer_t spec;
};
void Sys_open(vm_registers_t &r, vm_t &vm);
struct F_Sys_open : public vm_frame_base_alloc_t
{
    /* Sys_FD */ pointer_t* ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t s;
    word_t mode;
};
void Sys_pctl(vm_registers_t &r, vm_t &vm);
struct F_Sys_pctl : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    word_t flags;
    /* vm_list_t */ pointer_t movefd;
};
void Sys_pipe(vm_registers_t &r, vm_t &vm);
struct F_Sys_pipe : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t fds;
};
void Sys_pread(vm_registers_t &r, vm_t &vm);
struct F_Sys_pread : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
    /* vm_array_t */ pointer_t buf;
    word_t n;
    byte_t _pad44[4];
    big_t off;
};
void Sys_print(vm_registers_t &r, vm_t &vm);
struct F_Sys_print : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t s;
    byte_t vargs;
};
void Sys_pwrite(vm_registers_t &r, vm_t &vm);
struct F_Sys_pwrite : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
    /* vm_array_t */ pointer_t buf;
    word_t n;
    byte_t _pad44[4];
    big_t off;
};
void Sys_read(vm_registers_t &r, vm_t &vm);
struct F_Sys_read : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
    /* vm_array_t */ pointer_t buf;
    word_t n;
};
void Sys_readn(vm_registers_t &r, vm_t &vm);
struct F_Sys_readn : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
    /* vm_array_t */ pointer_t buf;
    word_t n;
};
void Sys_remove(vm_registers_t &r, vm_t &vm);
struct F_Sys_remove : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t s;
};
void Sys_seek(vm_registers_t &r, vm_t &vm);
struct F_Sys_seek : public vm_frame_base_alloc_t
{
    big_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
    byte_t _pad36[4];
    big_t off;
    word_t start;
};
void Sys_sleep(vm_registers_t &r, vm_t &vm);
struct F_Sys_sleep : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    word_t period;
};
void Sys_sprint(vm_registers_t &r, vm_t &vm);
struct F_Sys_sprint : public vm_frame_base_alloc_t
{
    /* vm_string_t */ pointer_t* ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t s;
    byte_t vargs;
};
void Sys_stat(vm_registers_t &r, vm_t &vm);
struct F_Sys_stat : public vm_frame_base_alloc_t
{
    struct { word_t t0; byte_t _pad4[4]; Sys_Dir t1; }*ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t s;
};
void Sys_stream(vm_registers_t &r, vm_t &vm);
struct F_Sys_stream : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t src;
    /* Sys_FD */ pointer_t dst;
    word_t bufsiz;
};
void Sys_tokenize(vm_registers_t &r, vm_t &vm);
struct F_Sys_tokenize : public vm_frame_base_alloc_t
{
    struct { word_t t0; /* vm_list_t */ pointer_t t1; }*ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t s;
    /* vm_string_t */ pointer_t delim;
};
void Sys_unmount(vm_registers_t &r, vm_t &vm);
struct F_Sys_unmount : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t s1;
    /* vm_string_t */ pointer_t s2;
};
void Sys_utfbytes(vm_registers_t &r, vm_t &vm);
struct F_Sys_utfbytes : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* vm_array_t */ pointer_t buf;
    word_t n;
};
void Sys_werrstr(vm_registers_t &r, vm_t &vm);
struct F_Sys_werrstr : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t s;
};
void Sys_write(vm_registers_t &r, vm_t &vm);
struct F_Sys_write : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* Sys_FD */ pointer_t fd;
    /* vm_array_t */ pointer_t buf;
    word_t n;
};
void Sys_wstat(vm_registers_t &r, vm_t &vm);
struct F_Sys_wstat : public vm_frame_base_alloc_t
{
    word_t* ret;
    byte_t temps[12];
    /* vm_string_t */ pointer_t s;
    byte_t _pad36[4];
    Sys_Dir d;
};
const char *Sys_PATH = "$Sys";
const word_t Sys_Maxint = 0x7fffffff;
const word_t Sys_QTDIR = 0x80;
const word_t Sys_QTAPPEND = 0x40;
const word_t Sys_QTEXCL = 0x20;
const word_t Sys_QTAUTH = 0x8;
const word_t Sys_QTTMP = 0x4;
const word_t Sys_QTFILE = 0x0;
const word_t Sys_ATOMICIO = 0x2000;
const word_t Sys_SEEKSTART = 0x0;
const word_t Sys_SEEKRELA = 0x1;
const word_t Sys_SEEKEND = 0x2;
const word_t Sys_NAMEMAX = 0x100;
const word_t Sys_ERRMAX = 0x80;
const word_t Sys_WAITLEN = 0xc0;
const word_t Sys_OREAD = 0x0;
const word_t Sys_OWRITE = 0x1;
const word_t Sys_ORDWR = 0x2;
const word_t Sys_OTRUNC = 0x10;
const word_t Sys_ORCLOSE = 0x40;
const word_t Sys_OEXCL = 0x1000;
const word_t Sys_DMDIR = 0x80000000;
const word_t Sys_DMAPPEND = 0x40000000;
const word_t Sys_DMEXCL = 0x20000000;
const word_t Sys_DMAUTH = 0x8000000;
const word_t Sys_DMTMP = 0x4000000;
const word_t Sys_MREPL = 0x0;
const word_t Sys_MBEFORE = 0x1;
const word_t Sys_MAFTER = 0x2;
const word_t Sys_MCREATE = 0x4;
const word_t Sys_MCACHE = 0x10;
const word_t Sys_NEWFD = 0x1;
const word_t Sys_FORKFD = 0x2;
const word_t Sys_NEWNS = 0x4;
const word_t Sys_FORKNS = 0x8;
const word_t Sys_NEWPGRP = 0x10;
const word_t Sys_NODEVS = 0x20;
const word_t Sys_NEWENV = 0x40;
const word_t Sys_FORKENV = 0x80;
const word_t Sys_EXPWAIT = 0x0;
const word_t Sys_EXPASYNC = 0x1;
const word_t Sys_UTFmax = 0x4;
const word_t Sys_UTFerror = 0xfffd;
const word_t Sys_Runemax = 0x10ffff;
const word_t Sys_Runemask = 0x1fffff;

#endif // _DISVM_SRC_VM_SYS_SYSMOD_H_

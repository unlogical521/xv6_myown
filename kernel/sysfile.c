//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//
#include "memlayout.h"
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  if(argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
uint64
sys_link(void)
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;

  if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

uint64
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  if(argstr(0, path, MAXPATH) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;

  if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    iunlockput(ip);
    end_op();
    return -1;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  if((omode & O_TRUNC) && ip->type == T_FILE){
    itrunc(ip);
  }

  iunlock(ip);
  end_op();

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[MAXPATH];
  struct inode *ip;

  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_mknod(void)
{
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;

  begin_op();
  if((argstr(0, path, MAXPATH)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEVICE, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();
  
  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(p->cwd);
  end_op();
  p->cwd = ip;
  return 0;
}

uint64
sys_exec(void)
{
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if(argstr(0, path, MAXPATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  if(argaddr(0, &fdarray) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}
// 根据故障地址确定vma起始地址
struct vma* findvma(struct proc* p,uint64 va){
  for(int i=0;i<NVMA;i++){
    struct vma* v = &p->vmas[i];
    // 判断va是否属于这个区间
    if(v->valid && va>=v->vstart && va < (v->vstart + v->sz)){
      return v;
    }
  }
  return 0;
}
// 进行分页,成功1，失败0
int allocvma(uint64 va){
  // 每次分一页
  struct proc* p = myproc();
  struct vma* v = 0;
  if((v = findvma(p,va))==0){
    return 0;
  }
  // 分页
  void* pa;
  if((pa = kalloc())==0){
    printf("allocvma : no more memory");
    return 0;
  }
  // 先写入再建立映射？还是先映射后写入？
  memset(pa,0,PGSIZE);
  begin_op();
  //写入内存
  ilock(v->f->ip);
  // readi函数负责根据inode读取文件数据
  // 首先需要将文件数据映射到内核中
  // 其次判断目标地址是用户空间还是内核空间
  // 如果是内核，则直接memmove数据
  // 如果是用户，则需要从内核复制到用户
  // 因为内核是直接映射的，va=pa；
  // 这里就假装是内核，等写入到物理内存后，再与用户建立映射
  readi(v->f->ip,0,(uint64)pa,v->offset + PGROUNDDOWN(va-v->vstart),PGSIZE);
  iunlock(v->f->ip);
  end_op();
  int perm = PTE_U;
  if(v->limit & PROT_READ){
    perm |= PTE_R;
  }
  if(v->limit & PROT_WRITE){
    perm |= PTE_W;
  }
  if(v->limit & PROT_EXEC){
    perm |= PTE_X;
  }
  // 映射
  if(mappages(p->pagetable,va,PGSIZE,(uint64)pa,perm)<0){
    printf("allocvma : mappages err");
    return 0;
  }
  return 1;
}
// 将文件内容直接映射到进程的地址空间
uint64
sys_mmap(void){
  // 取参数
  uint64 addr,sz,offset;
  int prot,flag,fd;
  struct file* f = 0;
  if(argaddr(0,&addr)<0 || argaddr(1,&sz)<0 || argint(2,&prot)<0 || argint(3,&flag)<0 ||
  argfd(4,&fd,&f)<0  || argaddr(5,&offset)<0
){
  return -1;
}
  if((!f->readable && (prot&(PROT_READ))) || ((!f->writable && (prot & PROT_WRITE)) && (flag&MAP_PRIVATE))){
    return -1;
  }
  // 取值后赋值
  sz = PGROUNDUP(sz);
  struct proc* p = myproc();
  struct vma * v = 0;
  uint64 vend = VMAEND;
  // 加锁
  acquire(&p->lock);
  for(int i=0;i<NVMA;i++){
    struct vma* vv = &p->vmas[i];
    if(vv->valid == 0){
      // 占用最初的这个
      if(v == 0){
        v = vv;
        v->valid = 1;
      }
    }
    else{
      vend -= vv->sz;
    }
  } 
  if(v==0){
    release(&p->lock);
    printf("mmap : no more vma");
    return -1;
  }
  // 起始地址
  // 只开了空头支票
  v->vstart = vend-sz;
  v->sz = sz;
  v->offset = offset;
  v->flags = flag;
  v->f = f;
  v->limit = prot;
  filedup(f);
  release(&p->lock);
  return v->vstart;
}
uint64
sys_munmap(void)
{
  uint64 addr, len;
  struct proc *p = myproc();
  struct vma *v;

  // 1. 获取参数
  if(argaddr(0, &addr) < 0 || argaddr(1, &len) < 0)
    return -1;

  if(len == 0)
    return 0;

  // 2. 必须页对齐 (POSIX 标准)
  uint64 va = PGROUNDDOWN(addr);
  uint64 sz = PGROUNDUP(addr + len) - va;

  // 3. 找到对应的 VMA
  v = findvma(p, va);
  if(v == 0 || !v->valid)
    return -1;

  uint64 vstart = v->vstart;
  uint64 vend = v->vstart + v->sz;

  // 4. 检查：解映射必须完全在一个 VMA 内
  if(va < vstart || va + sz > vend)
    return -1;

  // 5. 禁止中间挖洞！只能从 头部 或 尾部 解除映射
  // 不允许中间一段解除
  if(va != vstart && va + sz != vend)
    return -1;

  // 6. 执行真正的解映射（回写文件 + 释放物理页 + 清空页表）
  vmmunmap(p->pagetable, va, sz, v);

  // 7. 更新 VMA 信息
  if(va == vstart){
    // 从 头部 解除：移动起始地址
    v->vstart += sz;
    v->offset += sz;
  } else {
    // 从 尾部 解除：只缩小大小
  }
  v->sz -= sz;

  // 8. 如果 VMA 空了，关闭文件，标记无效
  if(v->sz == 0){
    fileclose(v->f);
    v->valid = 0;
  }

  return 0;
}
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  backtrace();
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//trap_lab
uint64 sys_sigalarm(void) {
  int ticks;
  uint64 handler;

  argint(0, &ticks);
  argaddr(1, &handler); 
  struct proc *p = myproc();

  p->alarm_interval = ticks;
  p->handler_va = handler;
  p->passed_ticks = 0;
  p->alarm_enabled = 1; // 允许触发 alarm

  return 0;
}

uint64 sys_sigreturn(void) {
  struct proc *p = myproc();

  // 恢复保存的 trapframe
  *p->trapframe = p->saved_trapframe;

  // 重新允许 alarm 触发
  p->alarm_enabled = 1;

  // 返回 a0 寄存器（保持系统调用语义)
  return p->trapframe->a0;
}
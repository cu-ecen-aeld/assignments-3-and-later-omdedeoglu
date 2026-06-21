# Assignment 7 - Faulty Driver Kernel Oops Analysis

## Objective

Trigger a kernel oops using the `faulty` kernel module and analyze the resulting stack trace to determine the cause of the crash and how to locate the offending source code.

---

## Test Performed

### Command Executed

```bash
echo "hello_world" > /dev/faulty
```

### Expected Behavior

Write the "hello_world" message to the faulty driver.

### Actual Behavior

It generated a kernel oops message and the system was restarted.

---

## Kernel Oops

```text
# echo hello_world > /dev/faulty
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x0000000096000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000041b3f000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 0000000096000045 [#1] SMP
Modules linked in: scull(O) faulty(O) hello(O)
CPU: 0 PID: 143 Comm: sh Tainted: G           O       6.1.44 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x10/0x20 [faulty]
lr : vfs_write+0xc8/0x390
sp : ffffffc008dfbd20
x29: ffffffc008dfbd80 x28: ffffff8001b2c240 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 000000000000000c x22: 000000000000000c x21: ffffffc008dfbdc0
x20: 0000005585f6bc80 x19: ffffff8001c01e00 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc000785000 x3 : ffffffc008dfbdc0
x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x10/0x20 [faulty]
 ksys_write+0x74/0x110
 __arm64_sys_write+0x1c/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x2c/0xc0
 el0_svc+0x2c/0x90
 el0t_64_sync_handler+0xf4/0x120
 el0t_64_sync+0x18c/0x190
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 0000000000000000 ]---
```

---

## Oops Analysis

### 1. Exception Summary

**Exception**

Exception occured when a pointer that pointing 0 virtual address tried to deference: Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000

**Faulting Virtual Address**

virtual address 0000000000000000

---

### 2. Loaded Kernel Modules

```
Modules linked in: scull(O) faulty(O) hello(O)
(O) means these are out-of-tree modules.
```

---

### 3. Faulting Process

```
CPU: 0
PID: 143
Comm: sh
```

This tells us a shell command of process id 143 is triggered this crash on CPU 0.

---

### 4. Faulting Instruction

```
pc : faulty_write+0x10/0x20 [faulty]
```

This tells us the error occured in faulty module's faulty_write function.

---

### 5. Call Trace

```
Call trace:
```

Describe how execution reached the faulty driver.

| Function               | Description |
| ---------------------- | ----------- |
| faulty_write()         | function that error happens |
| ksys_write()           | system call function |
| __arm64_sys_write()    | system call function |
| invoke_syscall()       | syscall from user side |
| do_el0_svc()           | user side (shell) function |
| el0_svc()              | user side (shell) function |
| el0t_64_sync_handler() | user side (shell) function |
| el0t_64_sync()         | user side (shell) function |

---

### 6. Register Dump

- ARM64 has 31 general purpose registers.
- Registers x0-x7 are used to show function input values
- Register x0 is used to show function return value
- pstate = processor state register
- pc = program counter
- lr(x30) = link register. It shows the return adress of the function
- sp = stack pointer
- x29 is frame pointer.

These register values can be meaningful when using assembly code from
objdump.
For this example, value of x1 is important because the assembly at 0x10 is str     wzr, [x1].
```asm
0000000000000000 <faulty_write>:
   0:   d2800001        mov     x1, #0x0                        // #0
   4:   d2800000        mov     x0, #0x0                        // #0
   8:   d503233f        paciasp
   c:   d50323bf        autiasp
  10:   b900003f        str     wzr, [x1]
  14:   d65f03c0        ret
  18:   d503201f        nop
  1c:   d503201f        nop
```
---

### 7. Locating the Source Code

The kernel oops reports:

```
faulty_write()+0x10/0x20
```

This information tells us error happened at 0x10 offset of the faulty_write function. The total size of the function is 0x20. We can find the assembly code using the objdump tool and we can find the source code line using the addr2line tool if the debug option was opened.

Here is the objdump command: `buildroot/output/host/bin/aarch64-buildroot-linux-gnu-objdump -d buildroot/output/bu
ild/ldd-a2ca055cf95a6896e1cc62dab6b563a5321d9ba4/misc-modules/faulty.ko`

Here is the addr2line command: `buildroot/output/host/bin/aarch64-buildroot-linux-gnu-addr2line -e buildroot/output/
build/ldd-a2ca055cf95a6896e1cc62dab6b563a5321d9ba4/misc-modules/faulty.ko 0x10`

Possible tools:

* `objdump`
* `addr2line`
* `System.map`
* Source code inspection

---

### 8. Root Cause

Kernel crashed because of to try accessing the NULL pointer location.

---

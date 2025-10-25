# Results:
Following is the page table entryes:

```
va 0x0 pte 0x7E0003E pa 0x7E00000 perm 0x3E
va 0x1000 pte 0x7E0003E pa 0x7E00000 perm 0x3E
va 0x2000 pte 0x7E0003E pa 0x7E00000 perm 0x3E
va 0x3000 pte 0x7E0003E pa 0x7E00000 perm 0x3E
va 0x4000 pte 0x7E0003E pa 0x7E00000 perm 0x3E
va 0x5000 pte 0x7E0003E pa 0x7E00000 perm 0x3E
va 0x6000 pte 0x7E0003E pa 0x7E00000 perm 0x3E
va 0x7000 pte 0x7E0003E pa 0x7E00000 perm 0x3E
va 0x8000 pte 0x7E0003E pa 0x7E00000 perm 0x3E
va 0x9000 pte 0x7E0003E pa 0x7E00000 perm 0x3E
va 0xFFF6000 pte 0x0 pa 0x0 perm 0x0
va 0xFFF7000 pte 0x0 pa 0x0 perm 0x0
va 0xFFF8000 pte 0x0 pa 0x0 perm 0x0
va 0xFFF9000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFA000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFB000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFC000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFD000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFE000 pte 0x0 pa 0x0 perm 0x0
va 0xFFFF000 pte 0x0 pa 0x0 perm 0x0

```
Each line of the output has the format:

- **va (Virtual Address):** The virtual address requested by the process.  
- **pte (Page Table Entry):** The raw entry in the page table that maps the virtual address to the physical address and contains permission bits.  
- **pa (Physical Address):** The actual location in physical memory that the virtual address maps to.  
- **perm (Permissions):** Encoded flags that describe the access rights of the page.  

---

## Permission Bits (perm / PTE Flags)

- **Bits 31–12 (High bits):** Physical Page Number (PFN)  
  - Points to the base physical address of the mapped page (aligned to 4 KB).  

- **Bits 11–0 (Low bits): Flags/Permissions**  
  These control access and status of the page:  
  - **P (Present / Valid bit):** 1 = page is mapped, 0 = unmapped.  
  - **W (Writable):** 1 = page can be written, 0 = read-only.  
  - **U (User):** 1 = user-space can access, 0 = kernel-only.  
  - **X (Executable):** determines if code execution is allowed from this page.  
  - **A (Accessed):** set when the page is read/written.  
  - **D (Dirty):** set when the page is written to.  
  - **C (Cacheable):** controls caching behavior on ARM.  
  - **B (Bufferable):** controls write buffering on ARM.  
  - **Other reserved bits:** depend on ARM MMU implementation.

---

### Example from Output

```
va 0x0 pte 0x7E0003E pa 0x7E00000 perm 0x3E
```
- `pte = 0x7E0003E`  
  - Physical address = `0x7E00000` (upper bits).  
  - Flags = `0x3E` (binary `111110`):  
    - Present = 1  
    - Writable = 1  
    - User = 1  
    - Executable = allowed  
    - Cacheable/Bufferable = enabled  



The `perm` value is a bitmask representing flags. For example, in xv6:  

- `PTE_P` (0x001): Page is present in memory  
- `PTE_W` (0x002): Page is writable  
- `PTE_U` (0x004): User-mode access allowed  
- `PTE_PS` (0x080): Page size (used for large pages / superpages)  
- Other bits may encode caching or kernel-only permissions
  
---

## Key Observations from Output

1. The **first 10 virtual addresses (0x0–0x9000)** map to the same physical page `0x7E00000` with `perm 0x3E`.  
   - This indicates the same physical frame is shared for multiple virtual pages (likely due to demand paging or initial mapping).  

2. The **last 10 virtual addresses (0xFFF6000–0xFFFF000)** have `pte 0x0`, meaning they are **not mapped**.  

3. Kernel page table dump (`print_kpt`) shows the kernel mappings, with most of them unmapped (`pte 0x0`).  

---



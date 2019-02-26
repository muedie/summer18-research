// Modified https://github.com/lms0016/LIRS
// as it was more performant than my implementation


#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define MAX_S_LEN (mem_size * 2500)

#define DEFAULT_MEMSIZE 10

#define STAT_START_POINT 0

#define LOWEST_HG_NUM 2

#define TRUE 1
#define FALSE 0

/* used to mark comparison of recency and Smax */
#define S_STACK_IN 1
#define S_STACK_OUT 0

#define EVICT_LIST_SIZE 10

typedef struct pf_struct {
  unsigned long ref_times;
  unsigned long pf_times;

  unsigned long page_num;
  int isResident;
  int isHIR_block;

  struct pf_struct* LIRS_next;
  struct pf_struct* LIRS_prev;

  struct pf_struct* HIR_rsd_next;
  struct pf_struct* HIR_rsd_prev;

  unsigned int recency;
} page_struct;

page_struct* page_tbl;

unsigned long total_pg_refs, warm_pg_refs;
unsigned long no_dup_refs; /* counter excluding duplicate refs */
unsigned long num_pg_flt;

long free_mem_size, mem_size, vm_size;

struct pf_struct* LRU_list_head;
struct pf_struct* LRU_list_tail;

struct pf_struct* HIR_list_head;
struct pf_struct* HIR_list_tail;

struct pf_struct* LIR_LRU_block_ptr; /* LIR block  with Rmax recency */

unsigned long HIR_block_portion_limit, HIR_block_activate_limit;

unsigned long *evict_list, evict_cur_idx, evict_max_idx;

extern page_struct* find_last_LIR_LRU();
extern void add_HIR_list_head(page_struct* new_rsd_HIR_ptr);
extern void add_LRU_list_head(page_struct* new_ref_ptr);
extern FILE* openReadFile();
extern void insert_LRU_list(page_struct* old_ref_ptr, page_struct* new_ref_ptr);
extern page_struct* prune_LIRS_stack();
extern void LIRS_Repl(FILE*);
extern void print_stack(int);
extern void record_evict(unsigned long);

unsigned long cur_lir_S_len;

/* get the range of accessed blocks [1:N] and the number of references */
int get_range(FILE* trc_fp, long* p_vm_size, long* p_trc_len) {
  char ref_blk_str[128];
  long ref_blk;
  long count = 0;
  long min, max;

  // binary file read
  unsigned int bn;
  unsigned int minb, maxb;

  fseek(trc_fp, 0, SEEK_SET);

 /* 
  fread(&bn, sizeof(unsigned int), 1, trc_fp);
  
  minb = maxb = bn;

  while(!feof(trc_fp)) {
    if (bn < 0) return FALSE;
    count++;
    if (bn > maxb) maxb = bn;
    if (bn < minb) minb = bn;

    fread(&bn, sizeof(unsigned int), 1, trc_fp);
  }
  */

  do {
    fscanf(trc_fp, "%s", ref_blk_str);
  } while (strcmp(ref_blk_str, "*") == 0);

  ref_blk = atoi(ref_blk_str);
  max = min = ref_blk;

  while (!feof(trc_fp)) {
    if (ref_blk < 0)
      return FALSE;
    count++;
    if (ref_blk > max)
      max = ref_blk;
    if (ref_blk < min)
      min = ref_blk;

    fscanf(trc_fp, "%s", ref_blk_str);
    ref_blk = atoi(ref_blk_str);
  }

  //  printf(" MIN page refs #: %lu, MAX page refs #: %lu for %lu refs in the
  //  trace\n",
  //           min, max, count);
  fseek(trc_fp, 0, SEEK_SET);
  *p_vm_size = max;
/*
  // binary read
  *p_vm_size = maxb;
*/
  *p_trc_len = count;
  return TRUE;
}

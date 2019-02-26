// Modified https://github.com/lms0016/LIRS
// as it was more performant than my implementation


#include "lirs.h"

static char trc_file_name[128];

int main(int argc, char* argv[]) {
  FILE* trace_fp /*, *cuv_fp, *sln_fp*/;
  unsigned long i;
  int opt;

  if (argc < 2) {
    printf("%s tracefile <data-points def: 10> <HIR rate def: 1> \n", argv[0]);
    exit(1);
  }

  strcpy(trc_file_name, argv[1]);
  trace_fp = openReadFile(trc_file_name);

  if (!get_range(trace_fp, &vm_size, &total_pg_refs)) {
    printf("trace error!\n");
    exit(1);
  }

  // step size
  int step = 10;
  int my_hir_rate = 1;

  // printf("\nmem_size %ld, vm_size %ld", mem_size, vm_size);
  if (argv[2])
    step = atoi(argv[2]);

  if (argv[3])
    my_hir_rate = atoi(argv[3]);

  int inc_per = (int)vm_size / step;

  // small edge case
  if (inc_per == 0)
    inc_per = 1;

  int inc = 0;

  page_tbl = (page_struct*)calloc(vm_size + 1, sizeof(page_struct));

  if (page_tbl == NULL) {
    printf("\nAllocation Failed. Units: %ld, Total: %ld\n", vm_size,
           sizeof(page_struct));
    exit(1);
  }

  // printf("%s\n", trc_file_name);

  for (mem_size = 10; inc <= step + 1; inc++, mem_size += inc_per) {
    // printf("mem_size %ld\n", mem_size);
    /*
      if (mem_size < 10) {
        printf("WARNING: Too small cache size(%ld). \n", mem_size);
        exit(1);
      }
    */
    total_pg_refs = 0;
    warm_pg_refs = 0;
    no_dup_refs = 0;
    num_pg_flt = 0;
    cur_lir_S_len = 0;

    fseek(trace_fp, 0, SEEK_SET);
    free_mem_size = mem_size;

    /* initialize the page table */
    for (i = 0; i <= vm_size; i++) {
      page_tbl[i].ref_times = 0;
      page_tbl[i].pf_times = 0;

      page_tbl[i].page_num = i;
      page_tbl[i].isResident = 0;
      page_tbl[i].isHIR_block = 1;

      page_tbl[i].LIRS_next = NULL;
      page_tbl[i].LIRS_prev = NULL;

      page_tbl[i].HIR_rsd_next = NULL;
      page_tbl[i].HIR_rsd_prev = NULL;

      page_tbl[i].recency = S_STACK_OUT;
    }

    LRU_list_head = NULL;
    LRU_list_tail = NULL;

    HIR_list_head = NULL;
    HIR_list_tail = NULL;

    LIR_LRU_block_ptr = NULL;
    /*
        evict_list = (unsigned long*)calloc(EVICT_LIST_SIZE, sizeof(unsigned
       long)); if (!evict_list) { fprintf(stderr, "Fail to alloc memory!\n");
          exit(1);
        }
        evict_max_idx = EVICT_LIST_SIZE;
        evict_cur_idx = 0;
    */
    /* the memory ratio for hirs is 1% */
    HIR_block_portion_limit = (unsigned long)(my_hir_rate / 100.0 * mem_size);
    if (HIR_block_portion_limit < LOWEST_HG_NUM)
      HIR_block_portion_limit = LOWEST_HG_NUM;

    LIRS_Repl(trace_fp);

    printf("HIR: %ld\t", HIR_block_portion_limit);

    printf("%ld %2.1f\n", mem_size, (float)num_pg_flt / warm_pg_refs * 100);
    // free(evict_list);
  }

  return 0;
}

FILE* openReadFile(char file_name[]) {
  FILE* fp;

  fp = fopen(file_name, "r");

  if (!fp) {
    printf("can not find file %s.\n", file_name);
    return NULL;
  }

  return fp;
}

void LIRS_Repl(FILE* trace_fp) {
  unsigned long ref_block, i, j, step;
  char ref_block_str[128];
  long last_ref_pg = -1;
  long num_LIR_pgs = 0;
  struct pf_struct* temp_ptr;
  int collect_stat = (STAT_START_POINT == 0) ? 1 : 0;
  int count = 0;
  int printout_idx = 1;

  // start of open file for recording DV

  // end of DV

  fseek(trace_fp, 0, SEEK_SET);
  do {
    fscanf(trace_fp, "%s", ref_block_str);
  } while (strcmp(ref_block_str, "*") == 0);
  ref_block = atoi(ref_block_str);

  i = 0;
  while (!feof(trace_fp)) {
    if (strcmp(ref_block_str, "*") == 0) {
      // print_stack(printout_idx);
      printout_idx++;

      fscanf(trace_fp, "%s", ref_block_str);
      if (strcmp(ref_block_str, "*")) {
        ref_block = atoi(ref_block_str);
      }
      continue;
    }

    int hom = TRUE;

    total_pg_refs++;
    if (total_pg_refs % 10000 == 0) {
      // fprintf(stderr, "%ld samples processed\r", total_pg_refs);
    }
    if (total_pg_refs > STAT_START_POINT) {
      collect_stat = 1;
      warm_pg_refs++;
    }

    if (ref_block > vm_size) {
      fprintf(stderr, "Wrong ref page number found: %ld.\n", ref_block);
      return;
    }

    if (ref_block == last_ref_pg) {
      fscanf(trace_fp, "%s", ref_block_str);
      if (strcmp(ref_block_str, "*")) {
        ref_block = atoi(ref_block_str);
      }
      continue;
    } else {
      last_ref_pg = ref_block;
    }

    no_dup_refs++; /* ref counter excluding duplicate refs */

    if (!page_tbl[ref_block].isResident) { /* block miss */
      hom = FALSE;
      if (collect_stat == 1) {
        num_pg_flt++;
      }

      if (free_mem_size == 0) {
        /* remove the "front" of the HIR resident page from cache (queue Q),
           but not from LIRS stack S
         */
        /* actually Q is an LRU stack, "front" is the bottom of the stack,
           "end" is its top
         */
        HIR_list_tail->isResident = FALSE;
        // record_evict(HIR_list_tail->page_num);
        remove_HIR_list(HIR_list_tail);
        free_mem_size++;
      } else if (free_mem_size > HIR_block_portion_limit) {
        page_tbl[ref_block].isHIR_block = FALSE;
        num_LIR_pgs++;
      }

      free_mem_size--;
    }
    /* hit in the cache */
    else if (page_tbl[ref_block].isHIR_block) {
      remove_HIR_list((page_struct*)&page_tbl[ref_block]);
    }

    remove_LIRS_list((page_struct*)&page_tbl[ref_block]);
    /* place newly referenced page at head */
    add_LRU_list_head((page_struct*)&page_tbl[ref_block]);
    page_tbl[ref_block].isResident = TRUE;
    if (page_tbl[ref_block].recency == S_STACK_OUT)
      cur_lir_S_len++;

    if (page_tbl[ref_block].isHIR_block &&
        (page_tbl[ref_block].recency == S_STACK_IN)) {
      page_tbl[ref_block].isHIR_block = FALSE;
      num_LIR_pgs++;

      if (num_LIR_pgs > mem_size - HIR_block_portion_limit) {
        add_HIR_list_head(LIR_LRU_block_ptr);
        HIR_list_head->isHIR_block = TRUE;
        HIR_list_head->recency = S_STACK_OUT;
        num_LIR_pgs--;
        LIR_LRU_block_ptr = find_last_LIR_LRU();  // prune the LIR stack
      } else {
        printf("Warning2!\n");
      }
    } else if (page_tbl[ref_block].isHIR_block) {
      add_HIR_list_head((page_struct*)&page_tbl[ref_block]);
    }

    page_tbl[ref_block].recency = S_STACK_IN;

    prune_LIRS_stack();

    // start of recording DV

    // end of DV

    fscanf(trace_fp, "%s", ref_block_str);
    if (strcmp(ref_block_str, "*")) {
      ref_block = atoi(ref_block_str);
    }
  }
  // print_stack(printout_idx);

  return;
}

/* remove a block from memory */
int remove_LIRS_list(page_struct* page_ptr) {
  if (!page_ptr)
    return FALSE;

  if (!page_ptr->LIRS_prev && !page_ptr->LIRS_next)
    return TRUE;

  if (page_ptr == LIR_LRU_block_ptr) {
    LIR_LRU_block_ptr = page_ptr->LIRS_prev;
    LIR_LRU_block_ptr = find_last_LIR_LRU();  // prune the LIR stack
  }

  if (!page_ptr->LIRS_prev)
    LRU_list_head = page_ptr->LIRS_next;
  else
    page_ptr->LIRS_prev->LIRS_next = page_ptr->LIRS_next;

  if (!page_ptr->LIRS_next)
    LRU_list_tail = page_ptr->LIRS_prev;
  else
    page_ptr->LIRS_next->LIRS_prev = page_ptr->LIRS_prev;

  page_ptr->LIRS_prev = page_ptr->LIRS_next = NULL;
  return TRUE;
}

/* record the evicted page from HIR queue */
void record_evict(unsigned long page_num) {
  if (evict_cur_idx >= (evict_max_idx - 1)) {
    evict_max_idx *= 2;
    evict_list = realloc(evict_list, evict_max_idx * sizeof(unsigned long));
    if (!evict_list) {
      fprintf(stderr, "Fail to realloc memory!\n");
      exit(1);
    }
  }

  evict_list[evict_cur_idx] = page_num;
  evict_cur_idx++;
}

/* remove a block from its teh front of HIR resident list */
int remove_HIR_list(page_struct* HIR_block_ptr) {
  if (!HIR_block_ptr)
    return FALSE;

  if (!HIR_block_ptr->HIR_rsd_prev)
    HIR_list_head = HIR_block_ptr->HIR_rsd_next;
  else
    HIR_block_ptr->HIR_rsd_prev->HIR_rsd_next = HIR_block_ptr->HIR_rsd_next;

  if (!HIR_block_ptr->HIR_rsd_next)
    HIR_list_tail = HIR_block_ptr->HIR_rsd_prev;
  else
    HIR_block_ptr->HIR_rsd_next->HIR_rsd_prev = HIR_block_ptr->HIR_rsd_prev;

  HIR_block_ptr->HIR_rsd_prev = HIR_block_ptr->HIR_rsd_next = NULL;

  return TRUE;
}

page_struct* find_last_LIR_LRU() {
  if (!LIR_LRU_block_ptr) {
    printf("Warning*\n");
    exit(1);
  }

  while (LIR_LRU_block_ptr->isHIR_block == TRUE) {
    LIR_LRU_block_ptr->recency = S_STACK_OUT;
    cur_lir_S_len--;
    LIR_LRU_block_ptr = LIR_LRU_block_ptr->LIRS_prev;
  }

  return LIR_LRU_block_ptr;
}

/* To address an extreme case, in which the size of LIR stack
 * is larger than the limitation(MAX_S_LEN).
 */
page_struct* prune_LIRS_stack() {
  page_struct* tmp_ptr;
  int i = 0;

  if (cur_lir_S_len <= MAX_S_LEN)
    return NULL;

  tmp_ptr = LIR_LRU_block_ptr;
  while (tmp_ptr->isHIR_block == 0)
    tmp_ptr = tmp_ptr->LIRS_prev;

  tmp_ptr->recency = S_STACK_OUT;
  remove_LIRS_list(tmp_ptr);
  insert_LRU_list(tmp_ptr, LIR_LRU_block_ptr);
  cur_lir_S_len--;

  return tmp_ptr;
}

/* put a HIR resident block on the end of HIR resident list */
void add_HIR_list_head(page_struct* new_rsd_HIR_ptr) {
  new_rsd_HIR_ptr->HIR_rsd_next = HIR_list_head;
  if (!HIR_list_head)
    HIR_list_tail = HIR_list_head = new_rsd_HIR_ptr;
  else
    HIR_list_head->HIR_rsd_prev = new_rsd_HIR_ptr;
  HIR_list_head = new_rsd_HIR_ptr;

  return;
}

/* put a newly referenced block on the top of LIRS stack */
void add_LRU_list_head(page_struct* new_ref_ptr) {
  new_ref_ptr->LIRS_next = LRU_list_head;

  if (!LRU_list_head) {
    LRU_list_head = LRU_list_tail = new_ref_ptr;
    LIR_LRU_block_ptr =
        LRU_list_tail; /* since now the point to lir page with Smax isn't nil */
  } else {
    LRU_list_head->LIRS_prev = new_ref_ptr;
    LRU_list_head = new_ref_ptr;
  }

  return;
}

/* insert a block in LIRS list */
void insert_LRU_list(page_struct* old_ref_ptr, page_struct* new_ref_ptr) {
  old_ref_ptr->LIRS_next = new_ref_ptr->LIRS_next;
  old_ref_ptr->LIRS_prev = new_ref_ptr;

  if (new_ref_ptr->LIRS_next)
    new_ref_ptr->LIRS_next->LIRS_prev = old_ref_ptr;
  new_ref_ptr->LIRS_next = old_ref_ptr;

  return;
}

void print_lir_stack(int idx) {
  FILE* file;
  char path[256];
  page_struct* iter;

  sprintf(path, "%s-output-LIR-%d.log", trc_file_name, idx);
  file = fopen(path, "w");
  if (file == NULL) {
    fprintf(stderr, "Fail to generate file %s\n", path);
    exit(1);
  }

  fprintf(file, "** LIRS stack TOP **\n");
  for (iter = LRU_list_head;
       iter && LIR_LRU_block_ptr && iter != LIR_LRU_block_ptr->LIRS_next;
       iter = iter->LIRS_next) {
    fprintf(file, "<%s%s> %ld\n", iter->isResident ? "R" : "NR",
            iter->isResident ? (iter->isHIR_block ? "H" : "L") : "",
            iter->page_num);
  }
  fprintf(file, "** LIRS stack BOTTOM **\n");

  fclose(file);
}

void print_hir_queue(int idx) {
  FILE* file;
  char path[256];
  page_struct* iter;

  sprintf(path, "%s-output-HIR-%d.log", trc_file_name, idx);
  file = fopen(path, "w");
  if (file == NULL) {
    fprintf(stderr, "Fail to generate file %s\n", path);
    exit(1);
  }

  fprintf(file, "** LIRS queue END **\n");
  for (iter = HIR_list_head; iter != NULL; iter = iter->HIR_rsd_next) {
    fprintf(file, "%ld\n", iter->page_num);
  }
  fprintf(file, "** LIRS queue front **\n");

  fclose(file);
}

void print_evict_seq(int idx) {
  FILE* file;
  char path[256];
  unsigned long i;

  sprintf(path, "%s-output-EVICTED-%d.log", trc_file_name, idx);
  file = fopen(path, "w");
  if (file == NULL) {
    fprintf(stderr, "Fail to generate file %s\n", path);
    exit(1);
  }

  fprintf(file, "LIRS EVICTED PAGE sequence:\n");
  for (i = 0; i < evict_cur_idx; i++) {
    fprintf(file, "<%ld> %ld\n", i, evict_list[i]);
  }
  fclose(file);
}

void print_stack(int idx) {
  print_lir_stack(idx);
  print_hir_queue(idx);
  // print_evict_seq(idx);
}

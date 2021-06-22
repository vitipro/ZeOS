int sys_fork(void)
{
  struct list_head *lhcurrent = NULL;
  union task_union *uchild;
  
  /* Any free task_struct? */
  if (list_empty(&freequeue)) return -ENOMEM;

  /* Max Screens reached? */
  if (n_screens + current()->process_n_screens > MAX_SCREENS) return -1;

  lhcurrent=list_first(&freequeue);
  
  list_del(lhcurrent);
  
  uchild=(union task_union*)list_head_to_task_struct(lhcurrent);
  
  /* Copy the parent's task struct to child's */
  copy_data(current(), uchild, sizeof(union task_union));
  
  /* new pages dir */
  allocate_DIR((struct task_struct*)uchild);
  
  /* Allocate pages for DATA+STACK */
  int new_ph_pag, pag, i;
  page_table_entry *process_PT = get_PT(&uchild->task);
  for (pag=0; pag<NUM_PAG_DATA; pag++)
  {
    new_ph_pag=alloc_frame();
    if (new_ph_pag!=-1) /* One page allocated */
    {
      set_ss_pag(process_PT, PAG_LOG_INIT_DATA+pag, new_ph_pag);
    }
    else /* No more free pages left. Deallocate everything */
    {
      /* Deallocate allocated pages. Up to pag. */
      for (i=0; i<pag; i++)
      {
        free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
        del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
      }
      /* Deallocate task_struct */
      list_add_tail(lhcurrent, &freequeue);
      
      /* Return error */
      return -EAGAIN; 
    }
  }

  /* Copy parent's SYSTEM and CODE to child. */
  page_table_entry *parent_PT = get_PT(current());
  for (pag=0; pag<NUM_PAG_KERNEL; pag++)
  {
    set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
  }
  for (pag=0; pag<NUM_PAG_CODE; pag++)
  {
    set_ss_pag(process_PT, PAG_LOG_INIT_CODE+pag, get_frame(parent_PT, PAG_LOG_INIT_CODE+pag));
  }
  /* Copy parent's DATA to child. We will use TOTAL_PAGES-1 as a temp logical page to map to */
  for (pag=NUM_PAG_KERNEL+NUM_PAG_CODE; pag<NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA; pag++)
  {
    /* Map one child page to parent's address space. */
    set_ss_pag(parent_PT, pag+NUM_PAG_DATA, get_frame(process_PT, pag));
    copy_data((void*)(pag<<12), (void*)((pag+NUM_PAG_DATA)<<12), PAGE_SIZE);
    del_ss_pag(parent_PT, pag+NUM_PAG_DATA);
  }


  /* Screen heritage section */
  int j, k;
  
  
  for (int j =0; j < current()->process_n_screens ; ++j) {
        new_ph_pag=alloc_frame();
        if (new_ph_pag!=-1) /* One page allocated */
        {
              set_ss_screenpag(process_PT, PAG_INIT_SCREENS+j, new_ph_pag);
        }
        else /* No more free pages left. Deallocate everything */
        {
              /* Deallocate scree allocated pages. Up to pag. */
              for (k=0; k<j; k++)
              {
                    free_frame(get_frame(process_PT, PAG_INIT_SCREENS+k));
                    del_ss_pag(process_PT, PAG_INIT_SCREENS+k);
              }
              //Deallocate data pages
              for (k=0; k < NUM_PAG_DATA; ++k) {
                    free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+k));
                    del_ss_pag(process_PT, PAG_LOG_INIT_DATA+k);
              }
              
              /* Deallocate task_struct */
              list_add_tail(lhcurrent, &freequeue);
              
              /* Return error */
              return -EAGAIN; 
        }
        
  }
 
 for (int j = 0; j < current()->process_n_screens; ++j) {
            if (parent_PT[PAG_INIT_SCREENS + j].bits.present == 1) {
                  process_PT[PAG_INIT_SCREENS + j].bits.present =1;
                  set_ss_pag(parent_PT, PAG_INIT_SCREENS+ NUM_PAG_SCREENS+ j, get_frame(process_PT, PAG_INIT_SCREENS + j));
                  copy_data((void*)((PAG_INIT_SCREENS + j)<<12), (void*)(( PAG_INIT_SCREENS + NUM_PAG_SCREENS + j)<<12), PAGE_SIZE);
                  del_ss_pag(parent_PT, PAG_INIT_SCREENS + NUM_PAG_SCREENS + j);
            }
  }
  
  if (current()->process_n_screens > 0) {
   n_screens += current()->process_n_screens;
    ++tasks_with_screens_size;
    list_add_tail(&(uchild->task.screen_list), &tasks_with_screens);
  }
  uchild->task.process_n_screens = current()->process_n_screens;

  /* Deny access to the child's memory space */
  set_cr3(get_DIR(current()));

  uchild->task.PID=++global_PID;
  uchild->task.state=ST_READY;

  int register_ebp;		/* frame pointer */
  /* Map Parent's ebp to child's stack */
  register_ebp = (int) get_ebp();
  register_ebp=(register_ebp - (int)current()) + (int)(uchild);

  uchild->task.register_esp=register_ebp + sizeof(DWord);

  DWord temp_ebp=*(DWord*)register_ebp;
  /* Prepare child stack for context switch */
  uchild->task.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->task.register_esp)=(DWord)&ret_from_fork;
  uchild->task.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->task.register_esp)=temp_ebp;

  /* Set stats to 0 */
  init_stats(&(uchild->task.p_stats));

  /* Queue child process into readyqueue */
  uchild->task.state=ST_READY;
  list_add_tail(&(uchild->task.list), &readyqueue);
  return uchild->task.PID;
}

int sys_createScreen () {
    if (n_screens >= MAX_SCREENS) return -1;
    
    if (current()->process_n_screens >= NUM_PAG_SCREENS) return -1;

    int pag=0, found = 0;
    
    while (found == 0 && pag<NUM_PAG_SCREENS) {
      if (get_PT(current())[PAG_INIT_SCREENS + pag].bits.present == 0) found = 1;
      else ++pag;
    }
    if (!found) return -1;
    int new_ph_pag = alloc_frame();
    if (new_ph_pag < 0) return -EAGAIN;
    set_ss_screenpag(get_PT(current()), PAG_INIT_SCREENS+pag, new_ph_pag);
    //get_PT(current())[PAG_INIT_SCREENS + pag].bits.present = 1;

    struct screen_struct *S = (struct screen_struct*)((PAG_INIT_SCREENS+pag) <<12);
    S->screen_ID = pag;
    S->owner_PID = current()->PID;
    S->colordata = 15;	// black background white foreground 
    S->x = 0;
    S->y = 0;
    for (int i = 0; i < NUM_ROWS; ++i) {
      for (int j = 0; j < NUM_COLUMNS; ++j) S->data[i][j] = ' ';
    }

    if (n_screens == 0) {
      cS.task = current();
      cS.fd = pag;
      update_screen();
    }

    ++n_screens;
    ++(current()->process_n_screens);
    if (current()->process_n_screens == 1) {
      ++tasks_with_screens_size;
      list_add_tail(&(current()->screen_list), &tasks_with_screens);
      l = list_first(&tasks_with_screens);
    }
    return pag;
}

int sys_close(int screen) {
    int ret = check_fd(screen, ESCRIPTURA);
    if (ret != 0) return ret;
    if (tasks_with_screens_size == 0) return -1;
    --(current()->process_n_screens);
    --n_screens;
    if (current()->process_n_screens == 0) {
	--tasks_with_screens_size;    
    }
  
  free_frame(get_frame(get_PT(current()), PAG_INIT_SCREENS + screen));
    del_ss_pag(get_PT(current()), PAG_INIT_SCREENS + screen);
//    get_PT(current())[PAG_INIT_SCREENS + screen].bits.present = 0;
     
    next_screen();
      
      if (current()->process_n_screens == 0) {
            struct list_head *l = &(current()->screen_list);
            list_del(l);
      }
    return 0;
}




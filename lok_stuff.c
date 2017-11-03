static long pmu_start_counters(unsigned int counter, unsigned long long config)
{
    unsigned long long pgc;

    printk( "pmu start - 1\n" );

    //Reserve PCMx
    if (!reserve_perfctr_nmi(MSR_ARCH_PERFMON_PERFCTR0 + counter))
    {   
      return (-EBUSY);
    }
    
    //Reserve PerfEvtSelx
    if (!reserve_evntsel_nmi(MSR_ARCH_PERFMON_EVENTSEL0 + counter))
    {   
      release_perfctr_nmi(MSR_ARCH_PERFMON_PERFCTR0 + counter);
      return (-EBUSY);
    }
    
    //Ensure counter is NON-OS and enabled
  config = 0xFFFFFFFF &
           (ARCH_PERFMON_EVENTSEL_ENABLE | //Require ENABLE
             (config & ~(ARCH_PERFMON_EVENTSEL_PIN_CONTROL |
                         ARCH_PERFMON_EVENTSEL_INT |
                         ARCH_PERFMON_EVENTSEL_ANY)
             )
           ); //Disallow PC, INT and ANY for security reasons
    
    printk( "pmu start - 2\n" );
    
    // DEBUG
    //unsigned long long val;
    //rdmsr(MSR_ARCH_PERFMON_PERFCTR0 + counter, val);
    //printk( "msr: %lld\n", val );

  
  //Reset counter
  wrmsr(MSR_ARCH_PERFMON_PERFCTR0 + counter, 0, 0);
  //Configure Counter
  wrmsr(MSR_ARCH_PERFMON_EVENTSEL0 + counter, config, 0);
  //Clear a possible counter overflow
  wrmsrl(MSR_CORE_PERF_GLOBAL_OVF_CTRL, 1 << counter);
    
    printk( "pmu start - 3\n" );
  
  rdmsrl(MSR_CORE_PERF_GLOBAL_CTRL, pgc);
  pgc |= 1 << counter;
  wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, pgc);
    
    long int eax_low, edx_high;
    int reg_addr=0xC1; 
    __asm__("rdmsr" : "=a"(eax_low), "=d"(edx_high) : "c"(reg_addr));
    long long unsigned count = ((long int)eax_low | (long int)edx_high<<32);
    printk( "msr: %lld\n", count );

    printk( "pgc (start): %lld\n", pgc );
    //start_rdpmc_ins = rdpmc_instructions();
    //printk( "rdpmc (start): %ld\n", start_rdpmc_ins );

  return (0);

}

static long pmu_stop_counters(unsigned int counter, unsigned long long config)
{
  unsigned long long pgc;

  config &= ~ARCH_PERFMON_EVENTSEL_ENABLE;
  wrmsrl(MSR_ARCH_PERFMON_EVENTSEL0 + counter, config);

  rdmsrl(MSR_CORE_PERF_GLOBAL_CTRL, pgc);
  pgc &= ~(1 << counter);
  wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, pgc);

    printk( "pgc (stop): %lld\n", pgc );

    long int eax_low, edx_high;
    int reg_addr=0xC1;
    __asm__("rdmsr" : "=a"(eax_low), "=d"(edx_high) : "c"(reg_addr));
    long long unsigned count = ((long int)eax_low | (long int)edx_high<<32);
    printk( "msr: %lld\n", count );

    //stop_rdpmc_ins = rdpmc_instructions();    
    //printk( "rdpmc (stop): %ld\n", stop_rdpmc_ins );
    //printk( "rdpmc (diff): %ld\n", stop_rdpmc_ins - start_rdpmc_ins );

  //FIXME: Deal with the local lp_info

  //Release resources back to kernel
  release_perfctr_nmi(MSR_ARCH_PERFMON_PERFCTR0 + counter);
  release_evntsel_nmi(MSR_ARCH_PERFMON_EVENTSEL0 + counter);

  return (0);
}



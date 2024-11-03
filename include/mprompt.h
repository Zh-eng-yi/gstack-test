/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Microsoft Research, Daan Leijen
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License. A copy of the license can be
  found in the "LICENSE" file at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef MP_MPROMPT_H
#define MP_MPROMPT_H 

//------------------------------------------------------
// Compiler specific attributes
//------------------------------------------------------
#if defined(_MSC_VER) || defined(__MINGW32__)
#if !defined(MP_SHARED_LIB)
#define mp_decl_export      
#elif defined(MP_SHARED_LIB_EXPORT)
#define mp_decl_export      __declspec(dllexport)
#else
#define mp_decl_export      __declspec(dllimport)
#endif
#elif defined(__GNUC__) // includes clang and icc      
#define mp_decl_export      __attribute__((visibility("default")))
#else
#define mp_decl_export      
#endif


//---------------------------------------------------------------------------
// Multi-prompt interface
//---------------------------------------------------------------------------

// Types
typedef struct mp_prompt_s   mp_prompt_t;     // resumable "prompts" (in-place growable stack chain)
typedef struct mp_resume_s   mp_resume_t;     // abstract resumption

// Function types
typedef void* (mp_start_fun_t)(mp_prompt_t*, void* arg); 
typedef void* (mp_yield_fun_t)(mp_resume_t*, void* arg);  

// Continue with `fun(p,arg)` under a fresh prompt `p`.
mp_decl_export void* mp_prompt(mp_start_fun_t* fun, void* arg); 

// Yield back up to a parent prompt `p` and run `fun(r,arg)` from there, where `r` is a `mp_resume_t` resumption.
mp_decl_export void* mp_yield(mp_prompt_t* p, mp_yield_fun_t* fun, void* arg);

// Resume back to the yield point with a result; can be used at most once.
mp_decl_export void* mp_resume(mp_resume_t* resume, void* arg);      // resume 
mp_decl_export void* mp_resume_tail(mp_resume_t* resume, void* arg); // resume as the last action in a `mp_yield_fun_t`
mp_decl_export void  mp_resume_drop(mp_resume_t* resume);            // drop the resume object without resuming


//---------------------------------------------------------------------------
// Multi-shot resumptions; use with care in combination with linear resources.
//---------------------------------------------------------------------------

mp_decl_export mp_resume_t* mp_resume_multi(mp_resume_t* r);  // consume a resumption and return one that can be invoked multiple times
mp_decl_export mp_resume_t* mp_resume_dup(mp_resume_t* r);    // only multi-resumptions can be dup'd



//---------------------------------------------------------------------------
// Initialization
//---------------------------------------------------------------------------
#include <stddef.h>
#include <stdbool.h>

// Configuration settings
typedef struct mp_config_s {
  bool      gpool_enable;         // enable gpools for in-process reuse of stack memory (besides the thread-local cache)
  bool      stack_grow_fast;      // grow stacks by doubling (to up to 1MiB at a time) instead of per-page
  bool      stack_use_overcommit; // use overcommit on systems that support this (Linux only) -- disables gpools and fast stack growing.
  bool      stack_reset_decommits;// instead of resetting memory in a gpool, use a full decommit in instead.
  ptrdiff_t gpool_max_size;       // maximum virtual size per gpool (256 GiB)
  ptrdiff_t stack_max_size;       // maximum virtual size of a gstack (8 MiB)
  ptrdiff_t stack_exn_guaranteed; // guaranteed extra stack space available during exception unwinding (Windows only) (16 KiB)
  ptrdiff_t stack_initial_commit; // initial commit size of a gstack (OS page size, 4 KiB)
  ptrdiff_t stack_gap_size;       // virtual no-access gap between stacks for security (64 KiB)
  ptrdiff_t stack_cache_count;    // count of gstacks to keep in a thread-local cache (4)  
} mp_config_t;

// Initialize with `config`; use NULL for default settings.
// Call at most once from the main thread before using any other functions. 
// Use as: `mp_config_t config = mp_config_default(); config.<setting> = <N>; mp_init(&config);`.
mp_decl_export void        mp_init(const mp_config_t* config);
mp_decl_export mp_config_t mp_config_default(void);  // default configuration for this platform



//---------------------------------------------------------------------------
// Low-level access  
// (only `mp_mresume_should_unwind` is required by `libmphandler`)
//---------------------------------------------------------------------------

// Get a portable backtrace
mp_decl_export int          mp_backtrace(void** backtrace, int len);

// How often is this resumption resumed?
mp_decl_export long         mp_resume_resume_count(mp_resume_t* r);
mp_decl_export int          mp_resume_should_unwind(mp_resume_t* r);  // refcount==1 && resume_count==0

// Separate prompt creation
mp_decl_export mp_prompt_t* mp_prompt_create(void);
mp_decl_export void* mp_prompt_enter(mp_prompt_t* p, mp_start_fun_t* fun, void* arg) ;

// Walk the chain of prompts.
mp_decl_export mp_prompt_t* mp_prompt_top(void);
mp_decl_export mp_prompt_t* mp_prompt_parent(mp_prompt_t* p);



#include <errno.h>
#include "internal/util.h"
#include "internal/longjmp.h"
#include "internal/gstack.h"

/*------------------------------------------------------------------------------
   Internal API for in-place growable gstacks
------------------------------------------------------------------------------*/
struct mp_gstack_s {
  mp_gstack_t*  next;               // used for the cache and delay list
  uint8_t*      full;               // stack reserved memory (including noaccess gaps)
  ssize_t       full_size;          // (for now always fixed to be `os_gstack_size`)
  uint8_t*      stack;              // stack inside the full area (without gaps)
  ssize_t       stack_size;         // actual available total stack size (includes reserved space) (depends on platform, but usually `os_gstack_size - 2*mp_gstack_gap`)
  ssize_t       initial_commit;     // initial committed memory (usually `os_page_size`)  
  ssize_t       committed;          // current committed estimate
  ssize_t       extra_size;         // size of extra allocated bytes.         
  uint8_t       extra[1];           // extra allocated (holds the mp_prompt_t structure)
};

typedef struct mp_gstack_s mp_gstack_t;
typedef struct mp_gsave_s  mp_gsave_t;

bool         mp_gstack_init(const mp_config_t* config); // normally called automatically
void         mp_gstack_clear_cache(void);               // clear thread-local cache of gstacks (called automatically on thread termination)

mp_gstack_t* mp_gstack_alloc(ssize_t extra_size, void** extra); 
void         mp_gstack_free(mp_gstack_t* gstack, bool delay);
void         mp_gstack_enter(mp_gstack_t* g, mp_jmpbuf_t** return_jmp, mp_stack_start_fun_t* fun, void* arg);

mp_gsave_t*  mp_gstack_save(mp_gstack_t* gstack, uint8_t* sp);    // save up to the given stack pointer (that should be in `gstack`)
void         mp_gsave_restore(mp_gsave_t* gsave);
void         mp_gsave_free(mp_gsave_t* gsave);

mp_gstack_t* mp_gstack_current(void);             // implemented in <mprompt.c>

#endif
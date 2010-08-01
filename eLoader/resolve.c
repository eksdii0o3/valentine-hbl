#include "sdk.h"
#include "debug.h"
#include "eloader.h"
//#include "scratchpad.h"
#include "elf.h"
#include "tables.h"
#include "hook.h"
#include "modmgr.h"
#include "syscall.h"
#include "config.h"
#include "resolve.h"
#include "globals.h"

// Autoresolves HBL missing stubs
// Some stubs are compulsory, like sceIo*
void resolve_missing_stubs()
{
    tGlobals * g = get_globals();
	int ret;
    u32 i;
	unsigned int num_nids;
	//u32* cur_stub = *(u32*)ADDR_HBL_STUBS_BLOCK_ADDR;
	u32* cur_stub = (u32*)HBL_STUBS_START;
	u32 nid = 0, syscall = 0;
	char lib_name[MAX_LIBRARY_NAME_LENGTH];

	ret = config_initialize();

	if (ret < 0)
		exit_with_log("**CONFIG INIT FAILED**", &ret, sizeof(ret));

	ret = config_num_nids_total(&num_nids);

	/*
#ifdef DEBUG
	LOGSTR0("--> HBL STUBS BEFORE ESTIMATING:\n");	
	for(i=0; i<num_nids; i++)
	{
		//LOGSTR2("--Stub address: 0x%08lX (offset: 0x%08lX)\n", cur_stub, (u32)cur_stub - *(u32*)ADDR_HBL_STUBS_BLOCK_ADDR);
		LOGSTR2("--Stub address: 0x%08lX (offset: 0x%08lX)\n", (u32)cur_stub, (u32*)cur_stub - (u32*)HBL_STUBS_START);
		LOGSTR1("  0x%08lX ", *cur_stub);
		cur_stub++;
		LOGSTR1("0x%08lX\n", *cur_stub);
		cur_stub++;
	}
	//cur_stub = *(u32*)ADDR_HBL_STUBS_BLOCK_ADDR;
	cur_stub = (u32*)HBL_STUBS_START;
#endif
	*/
	
	for (i=0; i<num_nids; i++)
	{
		if (*cur_stub == 0)
		{
			LOGSTR1("-Resolving unknown import 0x%08lX: ", (u32*)cur_stub - (u32*)HBL_STUBS_START);

			// NID & library for i-th import
			ret = get_lib_nid(i, lib_name, &nid);

			LOGSTR0(lib_name);
			LOGSTR1(" 0x%08lX\n", nid);

			// Is it known by HBL?
			ret = get_nid_index(nid);

			// If it's known, get the call
			if (ret > 0)
			{
				LOGSTR0("-Found in NID table, using real call\n");
				syscall = g->nid_table.table[ret].call;
			}
			
			// If not, estimate
			else
				syscall = estimate_syscall(lib_name, nid, g->syscalls_known ? FROM_LOWEST : FROM_CLOSEST);

			// DEBUG_PRINT("**RESOLVED SYS**", lib_name, strlen(lib_name));
			// DEBUG_PRINT(" ", &syscall, sizeof(syscall));

			resolve_call(cur_stub, syscall);
		}
		cur_stub += 2;
	}

	/*
#ifdef DEBUG
	//cur_stub = *(u32*)ADDR_HBL_STUBS_BLOCK_ADDR;
	cur_stub = (u32*)HBL_STUBS_START;
	
	LOGSTR0("--> HBL STUBS AFTER ESTIMATING:\n");	
	for(i=0; i<num_nids; i++)
	{
		//LOGSTR2("--Stub address: 0x%08lX (offset: 0x%08lX)\n", cur_stub, (u32)cur_stub - *(u32*)ADDR_HBL_STUBS_BLOCK_ADDR);
		LOGSTR2("--Stub address: 0x%08lX (offset: 0x%08lX)\n", (u32)cur_stub, (u32*)cur_stub - (u32*)HBL_STUBS_START);
		LOGSTR1("  0x%08lX ", *cur_stub);
		cur_stub++;
		LOGSTR1("0x%08lX\n", *cur_stub);
		cur_stub++;
	}	
#endif
	*/

	sceKernelDcacheWritebackInvalidateAll();

	config_close();
}

// Subsitutes the right instruction
void resolve_call(u32 *call_to_resolve, u32 call_resolved)
{
	// SYSCALL
	if(!(call_resolved & SYSCALL_MASK_RESOLVE))
	{
		*call_to_resolve = JR_RA_OPCODE;
		*(++call_to_resolve) = call_resolved;
	}
	
	// JUMP
	else
	{
		*call_to_resolve = call_resolved;
		*(++call_to_resolve) = NOP_OPCODE;
	}
}

// Returns utility ID that loads the given library
int is_utility(const char* lib_name)
{
	if (strcmp(lib_name, "sceMp3") == 0)
	    return PSP_MODULE_AV_MP3;
	
	else if (strcmp(lib_name, "sceNetInet") == 0 ||
			 strcmp(lib_name, "sceNetInet_lib") == 0 ||
	    	 strcmp(lib_name, "sceNetApctl") == 0 ||
	    	 strcmp(lib_name, "sceNetApctl_lib") == 0 ||
	    	 strcmp(lib_name, "sceNetApctl_internal_user") == 0 ||
	    	 strcmp(lib_name, "sceNetApctl_lib2") == 0 ||
	    	 strcmp(lib_name, "sceNetResolver") == 0)
		return PSP_MODULE_NET_INET;

	else if (strcmp(lib_name, "sceNet") == 0 ||
	    	 strcmp(lib_name, "sceNet_lib") == 0)
		return PSP_MODULE_NET_COMMON;

	else if (strcmp(lib_name, "sceNetAdhoc") == 0 ||
	    	 strcmp(lib_name, "sceNetAdhoc_lib") == 0 ||
	    	 strcmp(lib_name, "sceNetAdhocctl") == 0 ||
	    	 strcmp(lib_name, "sceNetAdhocctl_lib") == 0 ||
	    	 strcmp(lib_name, "sceNetAdhocMatching") == 0 ||
	    	 strcmp(lib_name, "sceNetAdhocDownload") == 0 ||
	    	 strcmp(lib_name, "sceNetAdhocDiscover") == 0 ||
	    	 strcmp(lib_name, "sceNetAdhoc") == 0 ||
	    	 strcmp(lib_name, "sceNetAdhoc") == 0 ||
	    	 strcmp(lib_name, "sceNetAdhoc") == 0)
		return PSP_MODULE_NET_ADHOC;
	
	else
		return -1;
}

// Resolves imports in ELF's program section already loaded in memory
// Returns number of resolves
unsigned int resolve_imports(tStubEntry* pstub_entry, unsigned int stubs_size)
{
	unsigned int i,j,nid_index;
	u32* cur_nid;
	u32* cur_call;
	u32 real_call;
	unsigned int resolving_count = 0;

#ifdef HOOK_CHDIR_AND_FRIENDS 
    tGlobals * g = get_globals();
    g->chdir_ok = test_sceIoChdir();
#endif

	LOGSTR1("RESOLVING IMPORTS. Stubs size: %d\n", stubs_size);

	/* Browse ELF stub headers */
	for(i=0; i<stubs_size; i+=sizeof(tStubEntry))
	{
		LOGSTR1("Pointer to stub entry: 0x%08lX\n", (u32)pstub_entry);	

		cur_nid = pstub_entry->nid_pointer;
		cur_call = pstub_entry->jump_pointer;

		LOGSTR1("Current library: %s\n", (u32)pstub_entry->library_name);
		
		// Load utility if necessary
		int mod_id = is_utility((char*)pstub_entry->library_name);
		if (mod_id > 0)
			load_utility_module(mod_id, (char*)pstub_entry->library_name);

		/* For each stub header, browse all stubs */
		for(j=0; j<pstub_entry->stub_size; j++)
		{

			LOGSTR1("Current nid: 0x%08lX\n", *cur_nid);
			LOGSTR1("Current call: 0x%08lX\n", (u32)cur_call);

			// Get syscall/jump instruction for current NID
			nid_index = get_call_nidtable(*cur_nid, &real_call);

			LOGSTR1("Index for NID on table: %d\n", nid_index);
            
			u32 hook_call = setup_hook(*cur_nid);

			if (hook_call != 0)
				real_call = hook_call;

			LOGSTR1("Real call before estimation: 0x%08lX\n", real_call);
            
			/* If NID not found in game imports */
			/* Syscall estimation if library available */
			if (real_call == 0)
			{
				real_call = estimate_syscall((char *)pstub_entry->library_name, *cur_nid, g->syscalls_known ? FROM_LOWEST : FROM_CLOSEST);
			}

			LOGSTR1("Real call after estimation: 0x%08lX\n", real_call);

			/* If it's an instruction, resolve it */
			/* 0xC -> syscall 0 */
			/* Jumps are always > 0xC */		
			if(real_call > 0xC)
			{	
				/* Write it in ELF stubs memory */
				resolve_call(cur_call, real_call);
				resolving_count++;
			}

			LOGSTR3("Resolved stub 0x%08lX: 0x%08lX 0x%08lX\n", (u32)cur_call, *cur_call, *(cur_call+1));

			sceKernelDcacheWritebackInvalidateAll();

			cur_nid++;
			cur_call += 2;
		}
		
		pstub_entry++;
	}
	
    LOGSTR0("RESOLVING IMPORTS: Done.");
	return resolving_count;	
}

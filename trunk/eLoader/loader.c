/* Half Byte Loader loader :P */
/* This loads HBL on memory */

#include "sdk.h"
#include "loader.h"
#include "debug.h"
#include "config.h"
#include "tables.h"
#include "utils.h"
#include "eloader.h"
#include "malloc.h"
#include "globals.h"
#include <exploit_config.h>


//This function is copied from elf.c to avoid compiling the entire elf.c dependencies for just one function
// Returns !=0 if stub entry is valid, 0 if it's not
int elf_check_stub_entry(tStubEntry* pentry)
{
	return ( 
    valid_umem_pointer((u32)(pentry->library_name)) &&
	(pentry->nid_pointer) &&
	(pentry->jump_pointer));
}

void (*run_eloader)(unsigned long arglen, unsigned long* argp) = 0;

// Loads HBL to memory
void load_hbl(SceUID hbl_file)
{	
    tGlobals * g = get_globals();
	unsigned long file_size;
	int bytes_read;
	SceUID HBL_block;

	// Get HBL size
	file_size = sceIoLseek(hbl_file, 0, PSP_SEEK_END);

	sceIoLseek(hbl_file, 0, PSP_SEEK_SET);

	//write_debug(" HBL SIZE ", &file_size, sizeof(file_size));
	
	// Allocate memory for HBL
	HBL_block = sceKernelAllocPartitionMemory(2, "Valentine", PSP_SMEM_Addr, file_size, (void *)HBL_LOAD_ADDRESS);
	if(HBL_block < 0)
		exit_with_log(" ERROR ALLOCATING HBL MEMORY ", &HBL_block, sizeof(HBL_block));
	run_eloader = sceKernelGetBlockHeadAddr(HBL_block);
	
	// Write HBL memory block info to scratchpad
	g->hbl_block_uid = HBL_block;
	g->hbl_block_addr = (u32)run_eloader;

	// Load HBL to allocated memory
	if ((bytes_read = sceIoRead(hbl_file, (void*)run_eloader, file_size)) < 0)
		exit_with_log(" ERROR READING HBL ", &bytes_read, sizeof(bytes_read));
	
	sceIoClose(hbl_file);

	LOGSTR1("HBL loaded to allocated memory @ 0x%08lX\n", (u32)run_eloader);

	// Commit changes to RAM
	sceKernelDcacheWritebackInvalidateAll();
}

// Fills Stub array
// Returns number of stubs found
// "pentry" points to first stub header in game
// Both lists must have same size
int search_game_stubs(tStubEntry *pentry, u32** stub_list, u32* hbl_imports_list, unsigned int list_size)
{
	u32 i = 0, j, count = 0;
	u32 *cur_nid, *cur_call;

	LOGSTR1("ENTERING search_game_stubs() 0x%08lX\n", (u32)pentry);

    //Some sanity checks
    if (!valid_umem_pointer((u32)pentry)) 
    {
        LOGSTR1("0x%08lX is not a valid user memory address, will ignore this list, please check your list of stubs \n", (u32)pentry);
        return 0;
    }
    
    if(!elf_check_stub_entry(pentry)) 
    {
        LOGSTR1("First entry for 0x%08lX is incorrect, will ignore this list, please check your list of stubs \n", (u32)pentry);
        return 0;
    }
    
	// While it's a valid stub header
	while(elf_check_stub_entry(pentry))
	{
		if ((pentry->import_flags != 0x11) && (pentry->import_flags != 0))
		{
			// Variable import, skip it
			pentry = (tStubEntry*)((int)pentry + sizeof(u32));
		}
		else
		{
			// Get current NID and stub pointer
			cur_nid = pentry->nid_pointer;
			cur_call = pentry->jump_pointer;
	
			// Browse all stubs defined by this header
			for(i=0; i<pentry->stub_size; i++)
			{
				// Compare each NID in game imports with HBL imports
				for(j=0; j<list_size; j++)
				{
					if(hbl_imports_list[j] == *cur_nid)
					{
						// Don't replace already found nids
						if (stub_list[j] != 0)
							continue;

						stub_list[j] = cur_call;
						LOGSTR3("nid:0x%08lX, address:0x%08lX call:0x%08lX", *cur_nid, (u32)cur_call, *cur_call);
						LOGSTR1(" 0x%08lX\n", *(cur_call+1));
						count++;
					}
				}
				cur_nid++;
				cur_call += 2;
			}	

			pentry++;
		}
	}

	return count;
}

// Loads info from config file
int load_imports(u32* hbl_imports)
{
	unsigned int num_imports;
	u32 nid, i = 0;
	int ret;

	// DEBUG_PRINT(" LOADING HBL IMPORTS FROM CONFIG ", NULL, 0);

	ret = config_num_nids_total(&num_imports);

	if(ret < 0)
		exit_with_log(" ERROR READING NUMBER OF IMPORTS ", &ret, sizeof(ret));

	// DEBUG_PRINT(" NUMBER OF IMPORTS ", &num_imports, sizeof(num_imports));

	if(num_imports > NUM_HBL_IMPORTS)
		exit_with_log(" ERROR FILE CONTAINS MORE IMPORTS THAN BUFFER SIZE ", &num_imports, sizeof(num_imports));

	LOGSTR0("--> HBL imports from imports.config:\n");

	// Get NIDs from config
	ret = config_first_nid(&nid);

	// DEBUG_PRINT(" FIRST NID RETURN VALUE ", &ret, sizeof(ret));
	
	do
	{
		if (ret <= 0)
			exit_with_log(" ERROR READING NEXT NID ", &i, sizeof(i));
		
		hbl_imports[i++] = nid;

		LOGSTR2("%d. 0x%08lX\n", i, nid);

		if (i >= num_imports)
			break;
		
		ret = config_next_nid(&nid);		
	} while (1);

	return i;
}

// Copies stubs used by HBL to scratchpad
void copy_hbl_stubs(void)
{
	// Temp storage
	int i, ret;
	
	// Where are HBL stubs
	u32* stub_addr;
	
	// HBL imports (NIDs)
	u32 hbl_imports[NUM_HBL_IMPORTS];
	
	// Stub addresses
	// The one sets to 0 are not imported by the game, and will be automatically estimated by HBL when loaded
	// ALL FUNCTIONS USED BEFORE SYSCALL ESTIMATION IS READY MUST BE IMPORTED BY THE GAME
	u32* stub_list[NUM_HBL_IMPORTS];
	
	// Game .lib.stub entry
	u32 pgame_lib_stub;

	// Initialize config	
	ret = config_initialize();
	// DEBUG_PRINT(" CONFIG INITIALIZED ", &ret, sizeof(ret));

	if (ret < 0)
		exit_with_log(" ERROR INITIALIZING CONFIG ", &ret, sizeof(ret));

	// Get game's .lib.stub pointer
	ret = config_first_lib_stub(&pgame_lib_stub);

	if (ret < 0)
		exit_with_log(" ERROR READING LIBSTUB ADDRESS ", &ret, sizeof(ret));
	
	//DEBUG_PRINT(" GOT GAME LIB STUB ", &pgame_lib_stub, sizeof(u32));

	//DEBUG_PRINT(" ZEROING STUBS ", NULL, 0);
	memset(&hbl_imports, 0, sizeof(hbl_imports));

	// Loading HBL imports (NIDs)
	ret = load_imports(hbl_imports);
	//DEBUG_PRINT(" HBL IMPORTS LOADED ", &ret, sizeof(ret));	

	if (ret < 0)
		exit_with_log(" ERROR LOADING IMPORTS FROM CONFIG ", &ret, sizeof(ret));

	// Reset stub list
	memset(stub_list, 0, NUM_HBL_IMPORTS * sizeof(u32));

	// Get number of import libraries
	unsigned int num_lib_stubs = 0;
	config_num_lib_stub(&num_lib_stubs);
	LOGSTR1("Loading %d stubs\n", num_lib_stubs);

	// Read in first stub address
	ret = config_first_lib_stub(&pgame_lib_stub);

	// Loop through all stubs and fill in stub list
	i = 0;

	do
	{
		if (ret == 0)
			exit_with_log("**ERROR SEARCHING GAME STUBS**", NULL, 0);

		search_game_stubs((tStubEntry*) pgame_lib_stub, stub_list, hbl_imports, (unsigned int) NUM_HBL_IMPORTS);	

		ret = config_next_lib_stub(&pgame_lib_stub);

		i++;
	}
	while (i < (int)num_lib_stubs);
	
	
	// Config finished
	config_close();

	LOGSTR0(" ****STUBS SEARCHED\n");


	stub_addr = (u32*)HBL_STUBS_START;

	// DEBUG_PRINT(" COPYING STUBS ", NULL, 0);
	for (i=0; i<NUM_HBL_IMPORTS; i++)
	{
		if (stub_list[i] != NULL)
			memcpy(stub_addr, stub_list[i], sizeof(u32)*2);
		else
			memset(stub_addr, 0, sizeof(u32)*2);
		stub_addr += 2;
		sceKernelDelayThread(100);
	}


	sceKernelDcacheWritebackInvalidateAll();
}

// Erase kernel dump
void init_kdump()
{
	SceUID fd;
	
	if ((fd = sceIoOpen(KDUMP_PATH, PSP_O_CREAT | PSP_O_WRONLY | PSP_O_TRUNC, 0777)) >= 0)
	{
		sceIoClose(fd);
	}
}

// Dumps kmem
void get_kmem_dump()
{
	SceUID dump_fd;
	
	dump_fd = sceIoOpen(KDUMP_PATH, PSP_O_CREAT | PSP_O_WRONLY, 0777);

	if (dump_fd >= 0)
	{
		sceIoWrite(dump_fd, (void*) 0x08000000, (unsigned int)0x400000);
		sceIoClose(dump_fd);
	}
}




// Clear the video memory
void clear_vram()
{
	memset(sceGeEdramGetAddr(), 0, sceGeEdramGetSize());
}


// Initializes the savedata dialog loop, opens p5
void p5_open_savedata(int mode)
{
	SceUtilitySavedataParam dialog;

	memset(&dialog, 0, sizeof(SceUtilitySavedataParam));
	dialog.base.size = sizeof(SceUtilitySavedataParam);

    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &dialog.base.language); // Prompt language
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &dialog.base.buttonSwap); // X/O button swap

	dialog.base.graphicsThread = 0x11;
	dialog.base.accessThread = 0x13;
	dialog.base.fontThread = 0x12;
	dialog.base.soundThread = 0x10;

	dialog.mode = mode;
	dialog.overwrite = 1;
	dialog.focus = PSP_UTILITY_SAVEDATA_FOCUS_LATEST; // Set initial focus to the newest file (for loading)

	strcpy(dialog.gameName, "DEMO11111");	// First part of the save name, game identifier name
	strcpy(dialog.saveName, "0000");	// Second part of the save name, save identifier name

	char nameMultiple[][20] =	// End list with ""
	{
	 "0000",
	 "0001",
	 "0002",
	 "0003",
	 "0004",
	 ""
	};

	// List of multiple names
	dialog.saveNameList = nameMultiple;

	strcpy(dialog.fileName, "DATA.BIN");	// name of the data file

	// Allocate buffers used to store various parts of the save data
	dialog.dataBuf = NULL;//malloc_p2(0x20);
	dialog.dataBufSize = 0;//0x20;
	dialog.dataSize = 0;//0x20;

	sceUtilitySavedataInitStart(&dialog);

	// Wait for the dialog to initialize
	while (sceUtilitySavedataGetStatus() < 2)
		sceDisplayWaitVblankStart();
}



// Runs the savedata dialog loop
void p5_close_savedata()
{
	LOGSTR0("entering savedata dialog loop\n");

	int running = 1;
	int last_status = -1;

	while(running) 
	{
		int status = sceUtilitySavedataGetStatus();
		
		if (status != last_status)
		{
			LOGSTR2("status changed from %d to %d\n", last_status, status);
			last_status = status;
		}

		switch(status) 
		{
			case PSP_UTILITY_DIALOG_VISIBLE:
				sceUtilitySavedataUpdate(1);
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilitySavedataShutdownStart();
				break;

			case PSP_UTILITY_DIALOG_NONE:
				running = 0;
				break;

			case PSP_UTILITY_DIALOG_FINISHED:
				break;
		}

		sceDisplayWaitVblankStart();
	}

	LOGSTR0("dialog has shut down\n");
}



// Write the p5 memory partition to a file
void p5_dump_memory(char* filename)
{
	SceUID file;
	file = sceIoOpen(filename, PSP_O_CREAT | PSP_O_WRONLY, 0777);
	sceIoWrite(file, (void*)0x08400000, 0x00400000);
	sceIoClose(file);
}


// Check the stub for validity, special version for p5 memory addresses
// Returns !=0 if stub entry is valid, 0 if it's not
int p5_elf_check_stub_entry(tStubEntry* pentry)
{
	return ( 
    ((u32)(pentry->library_name) > 0x08400000) &&
	((u32)(pentry->library_name) < 0x08800000) &&
	(pentry->nid_pointer) &&
	(pentry->jump_pointer));
}


// Change the stub pointers so that they point into their new memory location
void p5_relocate_stubs(void* destination, void* source)
{
	tStubEntry* pentry = (tStubEntry*)destination;
	int offset = (int)destination - (int)source;

	LOGSTR2("Relocating stub addresses from 0x%08lX to 0x%08lX\n", (u32)source, (u32)destination);

	while (p5_elf_check_stub_entry(pentry))
	{
		if (pentry->import_flags != 0x11)
		{
			// Variable import
			// relocate it to pass the the user memory pointer check
			pentry->library_name = (Elf32_Addr)((int)pentry->library_name + offset);
			pentry = (tStubEntry*)((int)pentry + 4);
		}
		else
		{
			LOGSTR4("current stub: 0x%08lX 0x%08lX 0x%08lX 0x%08lX ", (u32)pentry->library_name, (u32)pentry->import_flags, (u32)pentry->library_version, (u32)pentry->import_stubs);
			LOGSTR3("0x%08lX 0x%08lX 0x%08lX\n", (u32)pentry->stub_size, (u32)pentry->nid_pointer, (u32)pentry->jump_pointer);

			pentry->library_name = (Elf32_Addr)((int)pentry->library_name + offset);
			pentry->nid_pointer = (Elf32_Addr)((int)pentry->nid_pointer + offset);
			pentry->jump_pointer = (Elf32_Addr)((int)pentry->jump_pointer + offset);

			LOGSTR3("relocated to: 0x%08lX 0x%08lX 0x%08lX\n", (u32)pentry->library_name, (u32)pentry->nid_pointer, (u32)pentry->jump_pointer);

			pentry++;
		}
	}
}


// This just copies 128 kiB around the stub address to p2 memory
void p5_copy_stubs(void* destination, void* source)
{
	memcpy((void*)((int)destination - 0x10000), (void*)((int)source - 0x10000), 0x20000);
}



void* p5_find_stub_in_memory(char* library, void* start_address, u32 size) 
{
	char* name_address = NULL;
	void* stub_address = NULL;

	name_address = memfindsz(library, (char*)start_address, size);

	if (name_address)
	{
		// Stub pointer is 40 bytes after the library names char[0]
		stub_address = (void*)*(u32*)(name_address + 40);
	}
	
	return stub_address;
}


// These are the addresses were we copy p5 stuff
// This might overwrite some important things in ram, 
// so overwrite these values in exploit_config.h if needed
#ifndef RELOC_MODULE_ADDR_1
#define RELOC_MODULE_ADDR_1 0x09d10000
#endif
#ifndef RELOC_MODULE_ADDR_2
#define RELOC_MODULE_ADDR_2 0x09d30000
#endif
#ifndef RELOC_MODULE_ADDR_3
#define RELOC_MODULE_ADDR_3 0x09d50000
#endif
#ifndef RELOC_MODULE_ADDR_4
#define RELOC_MODULE_ADDR_4 0x09d70000
#endif

// Copy stubs for the savedata dialog with save mode 
void p5_copy_stubs_savedata_dialog()
{
	p5_open_savedata(PSP_UTILITY_SAVEDATA_SAVE);

	void* scePaf_Module = p5_find_stub_in_memory("scePaf_Module", (void*)0x084C0000, 0x00010000);
	void* sceVshCommonUtil_Module = p5_find_stub_in_memory("sceVshCommonUtil_Module", (void*)0x08760000, 0x00010000);
	void* sceDialogmain_Module = p5_find_stub_in_memory("sceDialogmain_Module", (void*)0x08770000, 0x00010000);

	//p5_dump_memory("ms0:/p5_savedata_save.dump");

	p5_copy_stubs((void*)RELOC_MODULE_ADDR_1, scePaf_Module);
	p5_copy_stubs((void*)RELOC_MODULE_ADDR_2, sceVshCommonUtil_Module);
	p5_copy_stubs((void*)RELOC_MODULE_ADDR_3, sceDialogmain_Module);

	p5_close_savedata();

	p5_relocate_stubs((void*)RELOC_MODULE_ADDR_1, scePaf_Module);
	p5_relocate_stubs((void*)RELOC_MODULE_ADDR_2, sceVshCommonUtil_Module);
	p5_relocate_stubs((void*)RELOC_MODULE_ADDR_3, sceDialogmain_Module);
}

// Copy stubs for the savedata dialog with autoload mode 
void p5_copy_stubs_savedata()
{
	p5_open_savedata(PSP_UTILITY_SAVEDATA_AUTOLOAD);

	void* sceVshSDAuto_Module = p5_find_stub_in_memory("sceVshSDAuto_Module", (void*)0x08410000, 0x00010000);

	//p5_dump_memory("ms0:/p5_savedata_autoload.dump");

	p5_copy_stubs((void*)RELOC_MODULE_ADDR_4, sceVshSDAuto_Module);

	p5_close_savedata();

	p5_relocate_stubs((void*)RELOC_MODULE_ADDR_4, sceVshSDAuto_Module);
}

// Copy the dialog stubs from p5 into p2 memory
void p5_get_stubs()
{
	p5_copy_stubs_savedata();
	p5_copy_stubs_savedata_dialog();
}


// Entry point
void _start() __attribute__ ((section (".text.start")));
void _start()
{
	SceUID hbl_file;

	LOGSTR0("Loader running\n");

    //reset the contents of the debug file;
    init_debug();
    
    //init global variables
    init_globals();

	// If PSPGo on 6.20+, do a kmem dump
	if ((getFirmwareVersion() >= 620) && (getPSPModel() == PSP_GO))
		get_kmem_dump();

	// Get additional syscalls from utility dialogs
	p5_get_stubs();

	if ((hbl_file = sceIoOpen(HBL_PATH, PSP_O_RDONLY, 0777)) < 0)
		exit_with_log(" FAILED TO LOAD HBL ", &hbl_file, sizeof(hbl_file));
	
	else
	{
		LOGSTR0("Loading HBL\n");
		load_hbl(hbl_file);
	
		LOGSTR0("Copying & resolving HBL stubs\n");
		copy_hbl_stubs();
        LOGSTR0("HBL stubs copied, running eLoader\n");
		run_eloader(0, NULL);
	}

	while(1)
		sceKernelDelayThread(100000);
}

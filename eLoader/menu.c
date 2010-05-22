/* Half Byte Loader is GPL
* Crappy Menu(tm) wololo (One day we'll get something better)
* thanks to N00b81 for the graphics library!
* 
*/
#include "sdk.h"
#include "debug.h"
#include "menu.h"
#include "eloader.h"
#include "graphics.h"

char folders[NB_FOLDERS][FOLDERNAME_SIZE] ;

char * cacheFile = HBL_ROOT"menu.cache";

int currentFile;
u32 * isSet;
void * ebootPath;
int nbFiles;

void *frameBuffer = (void *)0x44000000;

/**
* C++ version 0.4 char* style "itoa":
* Written by Lukás Chmela
* Released under GPLv3.
*/
char* itoa(int value, char* result, int base) {
	// check that the base if valid
	if (base < 2 || base > 36) { *result = '\0'; return result; }
	
	char* ptr = result, *ptr1 = result, tmp_char;
	int tmp_value;
	
	do {
		tmp_value = value;
		value /= base;
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
	} while ( value );
	
	// Apply negative sign
	if (tmp_value < 0) *ptr++ = '-';
	*ptr-- = '\0';
	while(ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr--= *ptr1;
		*ptr1++ = tmp_char;
	}
	return result;
}

void saveCache()
{
    SceUID id = sceIoOpen(cacheFile, PSP_O_CREAT | PSP_O_WRONLY | PSP_O_TRUNC, 0777);
    if (id < 0) 
        return;
    sceIoWrite(id, &folders, FOLDERNAME_SIZE * NB_FOLDERS * sizeof(char));
    sceIoClose(id);
}

/*Loads an alternate cached version of the folders
* if sceIoDopen failed
*/
void loadCache()
{
	int i;

    printTextScreen(0, 216 , "->Using a cached version of the folder's contents", 0x000000FF);
    SceUID id = sceIoOpen(cacheFile, PSP_O_RDONLY, 0777);
	
    if (id < 0) 
		return;
	
    sceIoRead(id, &folders, FOLDERNAME_SIZE * NB_FOLDERS * sizeof(char));
    sceIoClose(id);    
    
    nbFiles = 0;
    for (i = 0; i < NB_FOLDERS; ++i)
	{
        if (folders[i][0] == 0) 
        {
            break;
        }
        nbFiles++;
    }    
}

/*
 * Function to filter out "official" games
 * these games are usually in the form XXXX12345
*/
int not_homebrew(const char * name)
{
    int i;
    
    if (strlen(name) == 0) return 1;
    //official games are in the form: XXXX12345
    if (strlen(name) != 9) return 0;
    for (i = 4; i < 9; ++i)
    {
        if (name[i] < '0' || name[i] > '9')
            return 0;
    }
    
    return 1;
}

void init()
{
 	int i;
	SceUID id;
	SceIoDirent entry;
	
	nbFiles = 0;
	for (i = 0; i < NB_FOLDERS; ++i)
		folders[i][0] = 0;
	
  	id = sceIoDopen(ebootPath);
  	if (id < 0) 
	{
    	LOGSTR1("FATAL: Menu can't open directory %s \n", (u32)ebootPath);
        printTextScreen(0, 205 , "Unable to open GAME folder (syscall issue?)", 0x000000FF);
    	loadCache();
    	return;
  	}	
  
 	memset(&entry, 0, sizeof(SceIoDirent));
	
  	while (sceIoDread(id, &entry) > 0 && nbFiles < NB_FOLDERS)
  	{
    	if (strcmp(".", entry.d_name) == 0 || strcmp("..", entry.d_name) == 0 || not_homebrew(entry.d_name)) 
		{
        	memset(&entry, 0, sizeof(SceIoDirent)); 
        	continue;
    	}
         
    	strcpy(folders[nbFiles], entry.d_name);
    	nbFiles++;
    	memset(&entry, 0, sizeof(SceIoDirent)); 
  	}
	
  	sceIoDclose(id);
	
  	if (!nbFiles) 
	{
        printTextScreen(0, 205 , "No files found in GAME folder (syscall issue?)", 0x000000FF);
    	loadCache();
  	}
	
	saveCache();
}
 
void refreshMenu(int offSet)
{
  	int i;
	u32 color;
	cls();
	for(i = offSet; i < (MAXMENUSIZE + offSet); ++i)
	{
    	color = 0x00FFFFFF;
    	if (i == currentFile)
        	color = 0x0000FFFF;
    	printTextScreen(0, 15 + (i - offSet) * 12, folders[i], color);
  	}
}

void setEboot() 
{
  	strcat(ebootPath, folders[currentFile]);
  	strcat(ebootPath, "/EBOOT.PBP");
  	LOGSTR0(ebootPath);
  	isSet[0] = 1;
}

void _start() __attribute__ ((section (".text.start")));
void _start()
{
	SceCtrlData pad; // variable to store the current state of the pad
	
    int menuOffSet = 0;
	char buffer[20];
	
    currentFile = 0;
    isSet = (u32 *) EBOOT_SET_ADDRESS;
    ebootPath = (void *) EBOOT_PATH_ADDRESS;
    LOGSTR0("Start menu\n");
    isSet[0] = 0;

    LOGSTR0("MENU Sets display\n");
    
    //init screen
    sceDisplaySetFrameBuf(frameBuffer, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, 1);
    SetColor(0x00000000);

    //load folder's contents
    init();
    
    refreshMenu(0);
	
    sceKernelDelayThread(100000);
	
    do
	{
        sceCtrlReadBufferPositive (&pad, 1); // check the input.
		
        if (pad.Buttons & PSP_CTRL_CROSS)
		{ // if the cross button is pressed
            LOGSTR0("Menu sets EBOOT path: ");
            setEboot();			
        }
		else if ((pad.Buttons & PSP_CTRL_DOWN) && (currentFile < nbFiles - 1))
		{
            currentFile++;
			if(currentFile >= (MAXMENUSIZE + menuOffSet) && (nbFiles - 1) >= (MAXMENUSIZE + menuOffSet))
				menuOffSet++;
			refreshMenu(menuOffSet);

        }
		else if ((pad.Buttons & PSP_CTRL_UP) && (currentFile > 0) )
		{
            currentFile--;
			if(currentFile ==  menuOffSet && menuOffSet != 0)
				menuOffSet--;
			refreshMenu(menuOffSet);
        }
		else if(pad.Buttons & PSP_CTRL_TRIANGLE) 
		{
            sceKernelExitGame();
        }

#ifndef DEBUG    
        printTextScreen(0, 216, "DO NOT POST LOG FILES OR BUG REPORTS FOR THIS VERSION!!!", 0x000000FF);
#endif

		itoa((currentFile + 1),buffer,10);
		printTextScreen(5, 0 , buffer, 0x00FFFFFF);
		itoa(nbFiles,buffer,10);
		printTextScreen(30, 0 , buffer, 0x00FFFFFF);
		printTextScreen(21, 0 ,"/", 0x00FFFFFF);
		
        printTextScreen(220, 0 , "X to select, /\\ to quit", 0x00FFFFFF);
        printTextScreen(0, 227 , "Half Byte Loader BETA by m0skit0, ab5000, wololo, davee", 0x00FFFFFF);
        printTextScreen(0, 238 , "Thanks to n00b81, Tyranid, devs of the PSPSDK, Hitmen,", 0x00FFFFFF);
        printTextScreen(0, 249 , "Fanjita & Noobz, psp-hacks.com", 0x00FFFFFF);
        printTextScreen(0, 260 , "GPL License: give the sources if you distribute binaries!!!", 0x00FFFFFF);
        sceKernelDelayThread(100000);
    } while (!isSet[0]);

    //This point is reached when the value is set
    sceKernelExitDeleteThread(0);
}



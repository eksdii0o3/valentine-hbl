#define MAX_RUNTIME_STUB_HEADERS 35

int load_utility_module(int module);
int unload_utility_module(int module);
void load_utility_modules(unsigned int moduleIDs[]);
void unload_utility_modules(unsigned int moduleIDs[]);
void load_modules_for_stubs();
int search_stubs(u32 * stub_addresses);
void unload_modules_for_stubs();

#define MODULES_FOR_HBL_STUBS {PSP_MODULE_NET_COMMON, PSP_MODULE_NET_ADHOC, PSP_MODULE_NET_INET,PSP_MODULE_NET_PARSEURI,PSP_MODULE_NET_PARSEHTTP,PSP_MODULE_NET_SSL,PSP_MODULE_USB_PSPCM,PSP_MODULE_USB_MIC,PSP_MODULE_USB_CAM,PSP_MODULE_USB_GPS,PSP_MODULE_AV_SASCORE,PSP_MODULE_AV_ATRAC3PLUS,PSP_MODULE_AV_MPEGBASE,PSP_MODULE_AV_MP3,PSP_MODULE_AV_VAUDIO,PSP_MODULE_AV_AAC,PSP_MODULE_AV_G729,PSP_MODULE_NP_SERVICE,PSP_MODULE_NP_MATCHING2,PSP_MODULE_NP_DRM,PSP_MODULE_IRDA}
#define MODULES {PSP_MODULE_NET_COMMON, PSP_MODULE_NET_ADHOC, PSP_MODULE_NET_INET,PSP_MODULE_NET_PARSEURI,PSP_MODULE_NET_PARSEHTTP,PSP_MODULE_NET_HTTP,PSP_MODULE_NET_SSL,PSP_MODULE_USB_PSPCM,PSP_MODULE_USB_MIC,PSP_MODULE_USB_CAM,PSP_MODULE_USB_GPS,PSP_MODULE_AV_SASCORE,PSP_MODULE_AV_ATRAC3PLUS,PSP_MODULE_AV_MPEGBASE,PSP_MODULE_AV_MP3,PSP_MODULE_AV_VAUDIO,PSP_MODULE_AV_AAC,PSP_MODULE_AV_G729,PSP_MODULE_NP_COMMON,PSP_MODULE_NP_SERVICE,PSP_MODULE_NP_MATCHING2,PSP_MODULE_NP_DRM,PSP_MODULE_IRDA}
//PSP_MODULE_AV_AVCODEC <-- removed to cast syscall of sceAudiocodec and sceVideocodec


#include "node_library.h"

#include "worker.h"

#include "loggers/internal_logger.h"


#define i_type vec_static_libs // NOLINT
#define i_key  node_t          // NOLINT
#include "stc/vec.h"

static struct
{
    const char     *search_path;
    vec_static_libs slibs;
} *state;

// #ifdef OS_WIN
// #include <windows.h> // for Windows LoadLibrary/GetProcAddress
// #else
// #include <dlfcn.h>  // for POSIX dlopen/dlsym
// #endif

// void *getSymbol(void *libHandle, const char *name) {
//     #ifdef OS_WIN
//     return GetProcAddress((HMODULE)libHandle, name);
//     #else
//     return dlsym(libHandle, name);
//     #endif
// }

// static node_t dynLoadNodeLib(hash_t htype) {
//     char libName[256];
//     snprintf(libName, sizeof(libName), "libname-%u.so", htype); // Example library name generation

//     void *handle = NULL;
//     #ifdef OS_WIN
//     handle = LoadLibrary(libName);
//     #else
//     handle = dlopen(libName, RTLD_LAZY);
//     #endif

//     if (!handle) {
//         LOGF("Failed to load library: %s", libName);
//         return (node_t){0};
//     }

//     node_t lib = {0};
//     lib.createHandle = (tunnel_t *(*)(node_instance_context_t *))getSymbol(handle, "createHandle");
//     lib.destroyHandle = (tunnel_t *(*)(tunnel_t *))getSymbol(handle, "destroyHandle");
//     lib.apiHandle = (api_result_t (*)(tunnel_t *, const char *))getSymbol(handle, "apiHandle");
//     lib.getMetadataHandle = (tunnel_metadata_t (*)(void))getSymbol(handle, "getMetadataHandle");
//     lib.hash_name = htype;

//     return lib;
// }

static node_t dynLoadNodeLib(hash_t htype)
{
    discard htype;
    LOGF("dynLoadNodeLib not implemented");
    return (node_t) {0};
}

node_t nodelibraryLoadByTypeHash(hash_t htype)
{

    if (state != NULL)
    {
        c_foreach(k, vec_static_libs, state->slibs)
        {
            if ((k.ref)->hash_type == htype)
            {
                return *(k.ref);
            }
        }
    }
    return dynLoadNodeLib(htype);
}

node_t nodelibraryLoadByTypeName(const char *name)
{
    hash_t htype = calcHashBytes(name, strlen(name));
    return nodelibraryLoadByTypeHash(htype);
}

void nodelibraryRegister(node_t lib)
{
    if (state == NULL)
    {
        state = memoryAllocate(sizeof(*state));
        memorySet(state, 0, sizeof(*state));
        state->slibs = vec_static_libs_init();
    }

    vec_static_libs_push(&(state->slibs), lib);
}

bool nodeHasFlagChainHead(node_t *node)
{
    return (node->flags & kNodeFlagChainHead) == kNodeFlagChainHead;
}

void nodelibraryCleanup(void)
{
    if (state != NULL)
    {
        c_foreach(k, vec_static_libs, state->slibs)
        {
            memoryFree((k.ref)->type);
        }
        vec_static_libs_drop(&(state->slibs));
    }
    memoryFree(state);
    state = NULL;
}

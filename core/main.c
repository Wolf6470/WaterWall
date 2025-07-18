#include "core_settings.h"
#include "wwapi.h"

#include "imported_tunnels.h"
#include "loggers/core_logger.h"
#include "os_helpers.h"

// #ifdef COMPILER_MSVC
// #define _CRTDBG_MAP_ALLOC
// #pragma warning (disable: 4005)
// #include <crtdbg.h>
// #endif

static void exitHandle(void *userdata, int signum)
{
    discard signum;
    discard userdata;
    destroyCoreSettings();
}

int main(void)
{

    // #ifdef COMPILER_MSVC
    //     _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // #endif

    // check address sanitizer works properly
    // int test[3] = {0};
    // printf("hello world %d", test[3]);

    initWLibc();

    static const char *core_file_name    = "core.json";
    char              *core_file_content = readFile(core_file_name);

    if (core_file_content == NULL)
    {
        printError("Waterwall version %s\nCould not read core settings file \"%s\" \n", TOSTRING(WATERWALL_VERSION),
                   core_file_name);
        terminateProgram(1);
    }
    parseCoreSettings(core_file_content);
    memoryFree(core_file_content);

    //  [Runtime setup]
    createDirIfNotExists(getCoreSettings()->log_path);

    ww_construction_data_t runtime_data = {
        .workers_count = getCoreSettings()->workers_count,
        .ram_profile   = getCoreSettings()->ram_profile,
        .mtu_size      = getCoreSettings()->mtu_size,
        .internal_logger_data =
            (logger_construction_data_t) {.log_file_path = getCoreSettings()->internal_log_file_fullpath,
                                          .log_level     = getCoreSettings()->internal_log_level,
                                          .log_console   = getCoreSettings()->internal_log_console},

        .core_logger_data = (logger_construction_data_t) {.log_file_path = getCoreSettings()->core_log_file_fullpath,
                                                          .log_level     = getCoreSettings()->core_log_level,
                                                          .log_console   = getCoreSettings()->core_log_console},

        .network_logger_data =
            (logger_construction_data_t) {.log_file_path = getCoreSettings()->network_log_file_fullpath,
                                          .log_level     = getCoreSettings()->network_log_level,
                                          .log_console   = getCoreSettings()->network_log_console},

        .dns_logger_data = (logger_construction_data_t) {.log_file_path = getCoreSettings()->dns_log_file_fullpath,
                                                         .log_level     = getCoreSettings()->dns_log_level,
                                                         .log_console   = getCoreSettings()->dns_log_console},
    };

    // core logger is available after ww setup
    createGlobalState(runtime_data);

    LOGI("Starting Waterwall version %s", TOSTRING(WATERWALL_VERSION));
    LOGI("Parsing core file complete");
    registerAtExitCallBack(exitHandle, NULL);
    increaseFileLimit();
    loadImportedTunnelsIntoCore();

    //  [Parse ConfigFiles]
    {
        c_foreach(k, vec_config_path_t, getCoreSettings()->config_paths)
        {
            LOGD("Core: begin parsing config file \"%s\"", *k.ref);
            config_file_t *cfile = configfileParse(*k.ref);

            /*
                in case of error in config file, the details are already printed out
            */
            if (! cfile)
            {
                terminateProgram(1);
            }

            LOGI("Core: parsing config file \"%s\" complete", *k.ref);
            nodemanagerRunConfigFile(cfile);
        }
    }

    LOGD("Core: starting workers ...");
    socketmanagerStart();
    runMainThread();
}

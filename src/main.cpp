// Copyright (c) 2019 Intel Corporation.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM,OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

/**
 * @file
 * @brief VideoIngestion main program
 */

#include <unistd.h>
#include <condition_variable>
#include "eii/vi/video_ingestion.h"
#include <mutex>
#include <atomic>
#include <csignal>
#include <safe_lib.h>
#include <stdbool.h>
#include <eii/utils/json_validator.h>
#include <fstream>
#include <iostream>

#define MAX_CONFIG_KEY_LENGTH 250

using namespace eii::vi;
using namespace eii::ch;
using namespace eii::utils;
using namespace eii::config_manager;

static CommandHandler* g_ch = NULL;
static VideoIngestion* g_vi = NULL;
static char* g_vi_config = NULL;
static std::condition_variable g_err_cv;
static ConfigMgr* g_cfg_mgr = NULL;
static std::atomic<bool> g_cfg_change;

void usage(const char* name) {
    printf("Usage: %s \n", name);
}

void signal_callback_handler(int signum){
    if (signum == SIGTERM){
        LOG_INFO("Received SIGTERM signal, terminating Video Ingestion");
    }else if(signum == SIGABRT){
        LOG_INFO("Received SIGABRT signal, terminating Video Ingestion");
    }else if(signum == SIGINT){
        LOG_INFO("Received Ctrl-C, terminating Video Ingestion");
    }

    if(g_vi) {
        delete g_vi;
    }
    exit(0);
}

void clean_up() {
    if (g_ch) {
        delete g_ch;
    }
    if (g_vi) {
        delete g_vi;
    }
    if (g_cfg_mgr) {
        delete g_cfg_mgr;
    }
}

void vi_initialize(char* vi_config, std::string app_name){
    if (g_ch) {
        delete g_ch;
        g_ch = NULL;
    }

    if (g_vi) {
        delete g_vi;
        g_vi = NULL;
    }

    int server_num = g_cfg_mgr->getNumServers();
    if (server_num != -1) {
        try {
            g_ch = new CommandHandler(g_cfg_mgr);
        } catch(const char *err) {
            LOG_ERROR("Exception occurred : %s", err);
            if (g_ch) {
                delete g_ch;
                g_ch = NULL;
            }
        }
    } else {
        LOG_WARN_0("Server Config not present and hence Command Handler is not instantiated");
    }

    try {
        g_vi = new VideoIngestion(app_name, g_err_cv, vi_config, g_cfg_mgr, g_ch);
        g_vi->start();
    } catch(const std::exception& ex) {
        LOG_ERROR("Exception '%s' occurred", ex.what());
        clean_up();
    } catch(...) {
        LOG_ERROR("Exception occurred in Video Ingestion");
        clean_up();
    }
}

void on_change_config_callback(const char* key, config_t* value, void* user_data) {
    LOG_INFO("Callback triggered for key %s", key);
    char* vi_config = configt_to_char(value);
    if(strcmp(g_vi_config, vi_config)){
	// Deleting of g_vi, g_vi_config was hanging with gstreamer ingestor.
	// Making a forceful exit until this is fixed.
	// This shouldnt cause memory leaks as the application process is
	// exiting and re-spawned by docker clearing all process allocated
	// memory.
        _Exit(-1);
        /*if(g_vi) {
            delete g_vi;
            g_vi = NULL;
        }
        delete g_vi_config;*/
        // TODO: Uncomment the below logic once the dynamic cfg fix works as
        // expected
        //g_vi_config = vi_config;
        //g_cfg_change.store(true);
        //g_err_cv.notify_one();
    }
}

int main(int argc, char** argv) {
    signal(SIGINT, signal_callback_handler);
    signal(SIGABRT, signal_callback_handler);
    signal(SIGTERM, signal_callback_handler);

    try {
        if(argc >= 2) {
            usage(argv[0]);
            return -1;
        }

        // Get the configuration from the configuration manager
        g_cfg_mgr = new ConfigMgr();
        AppCfg* cfg = g_cfg_mgr->getAppConfig();
        if(cfg == NULL) {
            throw "Failed to initilize AppCfg object";
        }
        config_t* app_config = cfg->getConfig();
        if (app_config == NULL) {
            throw "Unable to fetch app config";
        }
        std::string app_name = g_cfg_mgr->getAppName();

        g_vi_config = configt_to_char(app_config);
        if (g_vi_config == NULL) {
            throw "Unable to fetch app config string";
        }
        LOG_DEBUG("App config: %s", g_vi_config);

        // Validating config against schema
        if (!validate_json_file_buffer(
                    "./VideoIngestion/schema.json", g_vi_config)) {
            LOG_ERROR_0("Schema validation failed");
            return -1;
        }

        char* str_log_level = NULL;
        log_lvl_t log_level = LOG_LVL_ERROR; // default log level is `ERROR`

        str_log_level = getenv("C_LOG_LEVEL");
        if(str_log_level == NULL) {
            throw "\"C_LOG_LEVEL\" env not set";
        } else {
            if(strncmp(str_log_level, "DEBUG", 5) == 0) {
                log_level = LOG_LVL_DEBUG;
            } else if(strncmp(str_log_level, "INFO", 5) == 0) {
                log_level = LOG_LVL_INFO;
            } else if(strncmp(str_log_level, "WARN", 5) == 0) {
                log_level = LOG_LVL_WARN;
            } else if(strncmp(str_log_level, "ERROR", 5) == 0) {
                log_level = LOG_LVL_ERROR;
        }
        set_log_level(log_level);
        }

        set_log_level(log_level);

        LOG_DEBUG_0("Registering watch on app config");
        bool ret = cfg->watchConfig(on_change_config_callback, NULL);
        if (!ret) {
            throw "Failed to register callback";
        }

        vi_initialize(g_vi_config, app_name);

        std::mutex mtx;

        while(true) {
            std::unique_lock<std::mutex> lk(mtx);
            g_err_cv.wait(lk);
            if(g_cfg_change.load()) {
                vi_initialize(g_vi_config, app_name);
                g_cfg_change.store(false);
            } else {
                break;
            }
        }

        clean_up();
    } catch(const char *err) {
        LOG_ERROR("Exception occurred: %s", err);
        clean_up();
    } catch(const std::exception& ex) {
        LOG_ERROR("Exception '%s' occurred", ex.what());
        clean_up();
    } catch(...) {
        LOG_ERROR("Exception occurred in Video Ingestion");
        clean_up();
    }
    return -1;
}

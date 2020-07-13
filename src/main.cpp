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
#include "eis/vi/video_ingestion.h"
#include <mutex>
#include <atomic>
#include <csignal>
#include <safe_lib.h>
#include <stdbool.h>
#include <eis/utils/json_validator.h>
#include <fstream>
#include <iostream>

#define MAX_CONFIG_KEY_LENGTH 250

using namespace eis::vi;
using namespace eis::ch;
using namespace eis::utils;

static CommandHandler* g_ch = NULL;
static VideoIngestion* g_vi = NULL;
static char* g_vi_config = NULL;
static std::condition_variable g_err_cv;
static config_mgr_t* g_config_mgr = NULL;
static env_config_t* g_env_config_client = NULL;
static std::atomic<bool> g_cfg_change;
static char* server_config = NULL;

void get_config_mgr(char* str_app_name) {
    char pub_cert_file[MAX_CONFIG_KEY_LENGTH];
    char pri_key_file[MAX_CONFIG_KEY_LENGTH];
    char trust_file[MAX_CONFIG_KEY_LENGTH];
    char storage_type[MAX_CONFIG_KEY_LENGTH];
    std::string dev_mode_str = "";
    int ret = 0;
    server_config = getenv("Server");

    char* str_dev_mode = NULL;
    str_dev_mode = getenv("DEV_MODE");
    if(str_dev_mode == NULL) {
        throw "\"DEV_MODE\" env not set";
    } else {
        dev_mode_str = str_dev_mode;
    }

    bool dev_mode = false;
    if (dev_mode_str == "true") {
        dev_mode = true;
    }

    if(!dev_mode) {
        ret = snprintf(pub_cert_file, MAX_CONFIG_KEY_LENGTH,
                 "/run/secrets/etcd_%s_cert", str_app_name);
        if (ret < 0) {
            throw "failed to create pub_cert_file";
        }
        ret = snprintf(pri_key_file, MAX_CONFIG_KEY_LENGTH,
                 "/run/secrets/etcd_%s_key", str_app_name);
        if (ret < 0) {
            throw "failed to create pri_key_file";
        }
        ret = strncpy_s(trust_file, MAX_CONFIG_KEY_LENGTH + 1,
                  "/run/secrets/ca_etcd", MAX_CONFIG_KEY_LENGTH);
        if (ret != 0) {
            throw "failed to create trust file";
        }

        char* confimgr_cert = getenv("CONFIGMGR_CERT");
        char* confimgr_key = getenv("CONFIGMGR_KEY");
        char* confimgr_cacert = getenv("CONFIGMGR_CACERT");
        if(confimgr_cert && confimgr_key && confimgr_cacert) {
            ret = strncpy_s(pub_cert_file, MAX_CONFIG_KEY_LENGTH + 1,
                            confimgr_cert, MAX_CONFIG_KEY_LENGTH);
            if (ret != 0) {
                throw "failed to add cert to trust file";
            }
            ret = strncpy_s(pri_key_file, MAX_CONFIG_KEY_LENGTH + 1,
                            confimgr_key, MAX_CONFIG_KEY_LENGTH);
            if (ret !=0) {
                throw "failed to add key to trust file";
            }
            ret = strncpy_s(trust_file, MAX_CONFIG_KEY_LENGTH + 1,
                            confimgr_cacert, MAX_CONFIG_KEY_LENGTH);
            if (ret != 0 ){
                 throw "failed to add cacert to trust file";
            }
       }
    }

    ret = strncpy_s(storage_type, (MAX_CONFIG_KEY_LENGTH + 1),
                    "etcd", MAX_CONFIG_KEY_LENGTH);
    if (ret != 0){
        throw "failed to add storage type";
    }

    g_config_mgr = config_mgr_new(storage_type,
                                 pub_cert_file,
                                 pri_key_file,
                                 trust_file);
}

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

void vi_initialize(char* vi_config, char* str_app_name){
    if (g_ch) {
        delete g_ch;
        g_ch = NULL;
    }

    if(g_vi){
        delete g_vi;
        g_vi = NULL;
    }

    if(!g_config_mgr){
        get_config_mgr(str_app_name);
        if(!g_config_mgr) {
            const char* err = "config-mgr object creation failed.";
            throw(err);
        }
    }
    g_env_config_client = env_config_new();

    std::string app_name = "";
    app_name = str_app_name;

    if (server_config != NULL) {
        try {
            g_ch = new CommandHandler(app_name, g_env_config_client, g_config_mgr);
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

    g_vi = new VideoIngestion(app_name, g_err_cv, g_env_config_client, vi_config, g_config_mgr, g_ch);
    g_vi->start();
}

void on_change_config_callback(char* key, char* vi_config){
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

void clean_up(){
    if(g_ch) {
        delete g_ch;
    }

    if(g_vi) {
        delete g_vi;
    }
    config_mgr_config_destroy(g_config_mgr);
    env_config_destroy(g_env_config_client);
}

bool validate_config(char config_key[]) {
    // Writing to external file
    std::ofstream out;
    out.open("./config.json", std::ios::binary);
    out << config_key;
    out.close();

    WJReader readjson;
    WJReader readschema;
    WJElement json;
    WJElement schema;

    readjson = WJROpenFILEDocument(fopen("./VideoIngestion/config.json", "r"), NULL, 0);
    json = WJEOpenDocument(readjson, NULL, NULL, NULL);
    if(readjson == NULL || json == NULL) {
        LOG_ERROR_0("config json could not be read");
        return false;
    }

    readschema = WJROpenFILEDocument(fopen("./VideoIngestion/schema.json", "r"), NULL, 0);
    schema = WJEOpenDocument(readschema, NULL, NULL, NULL);
    if(readschema == NULL || schema == NULL) {
        LOG_ERROR_0("schema json could not be read");
        return false;
    }

    bool result = validate_json(schema, json);

    WJECloseDocument(json);
    WJECloseDocument(schema);
    WJRCloseDocument(readjson);
    WJRCloseDocument(readschema);

    if(!result) {
        // Clean up and return if failure
        clean_up();
        return false;
    }

    return true;
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

        // read app_name env variable
        char* str_app_name = NULL;
        str_app_name = getenv("AppName");
        if(str_app_name == NULL) {
            throw "\"AppName\" env not set";
        }

        get_config_mgr(str_app_name);

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

        // Get the configuration from the configuration manager
        char config_key[MAX_CONFIG_KEY_LENGTH];
        snprintf(config_key, MAX_CONFIG_KEY_LENGTH, "/%s/config", str_app_name);

        // Validating config against schema
        if(!validate_config(config_key)) {
            return -1;
        }

        g_vi_config = g_config_mgr->get_config(config_key);
        LOG_DEBUG("App config: %s", g_vi_config);

        LOG_DEBUG("Registering watch on config key: %s", config_key);
        g_config_mgr->register_watch_key(config_key, on_change_config_callback);

        vi_initialize(g_vi_config, str_app_name);

        std::mutex mtx;

        while(true) {
            std::unique_lock<std::mutex> lk(mtx);
            g_err_cv.wait(lk);
            if(g_cfg_change.load()) {
                vi_initialize(g_vi_config, str_app_name);
                g_cfg_change.store(false);
            } else {
                break;
            }
        }

        clean_up();
    } catch(const std::exception& ex) {
        LOG_ERROR("Exception '%s' occurred", ex.what());
        clean_up();
    } catch(const char *err) {
        LOG_ERROR("Exception occurred: %s", err);
        clean_up();
    }
    return -1;
}

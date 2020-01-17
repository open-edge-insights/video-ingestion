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

#define MAX_CONFIG_KEY_LENGTH 40

using namespace eis::vi;
using namespace eis::utils;

static VideoIngestion* g_vi = NULL;
static char* g_vi_config = NULL;
static std::condition_variable g_err_cv;
static config_mgr_t* g_config_mgr = NULL;
static env_config_t* g_env_config_client = NULL;
static std::atomic<bool> g_cfg_change;

void get_config_mgr(){
    std::string pub_cert_file = "";
    std::string pri_key_file = "";
    std::string trust_file = "";
    std::string app_name = "";
    std::string dev_mode_str = "";
    
    char* str_app_name = NULL;
    str_app_name = getenv("AppName");
    if(str_app_name == NULL) {
        throw "\"AppName\" env not set";
    } else {
        app_name = str_app_name;
    }

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
        pub_cert_file = "/run/secrets/etcd_" + app_name + "_cert";
        pri_key_file = "/run/secrets/etcd_" + app_name + "_key";
        trust_file = "/run/secrets/ca_etcd";
    }

    g_config_mgr = config_mgr_new("etcd",
                                 (char*)pub_cert_file.c_str(),
                                 (char*) pri_key_file.c_str(),
                                 (char*) trust_file.c_str());

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

void vi_initialize(char* vi_config){
    if(g_vi){
        delete g_vi;
        g_vi = NULL;
    }
    if(!g_config_mgr){
        get_config_mgr();
    }
    g_env_config_client = env_config_new();
    g_vi = new VideoIngestion(g_err_cv, g_env_config_client, vi_config, g_config_mgr);
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
    if(g_vi) {
        delete g_vi;
    }
    config_mgr_config_destroy(g_config_mgr);
    env_config_destroy(g_env_config_client);
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
        get_config_mgr();

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

        std::string app_name = "";
        char* str_app_name = NULL;
        str_app_name = getenv("AppName");
        if(str_app_name == NULL) {
            throw "\"AppName\" env not set";
        } else {
            app_name = str_app_name;
        }

        // Get the configuration from the configuration manager
        char config_key[MAX_CONFIG_KEY_LENGTH];
        snprintf(config_key, MAX_CONFIG_KEY_LENGTH, "/%s/config", app_name.c_str());
        g_vi_config = g_config_mgr->get_config(config_key);
        LOG_DEBUG("App config: %s", g_vi_config);

        LOG_DEBUG("Registering watch on config key: %s", config_key);
        g_config_mgr->register_watch_key(config_key, on_change_config_callback);

        vi_initialize(g_vi_config);

        std::mutex mtx;

        while(true) {
            std::unique_lock<std::mutex> lk(mtx);
            g_err_cv.wait(lk);
            if(g_cfg_change.load()) {
                vi_initialize(g_vi_config);
                g_cfg_change.store(false);
            } else {
                break;
            }
        }

        clean_up();
    } catch(const std::exception& ex) {
        LOG_ERROR("Exception '%s' occurred", ex.what());
        clean_up();
    }
    return -1;
}

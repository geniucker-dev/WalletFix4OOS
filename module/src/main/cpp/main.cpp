#include <string>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <filesystem>
#include <unistd.h>
#include <fcntl.h>
#include <android/log.h>
#include <sys/system_properties.h>

#include "zygisk.hpp"


using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

#define LOG_TAG "WalletFix4OOS"

#ifdef NDEBUG
#define LOGD(...) ((void)0)
#define LOGV(...) ((void)0)
#else
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#endif
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, __VA_ARGS__)

#define CONFIG_FILE "/data/adb/walletfix/spoof_vars"
#define TARGET_FILE "/data/adb/walletfix/target.txt"
#define DEFAULT_CONFIG "MODEL=PJD110"
#define DEFAULT_TARGETS "com.finshell.wallet\ncom.unionpay.tsmservice"

ssize_t xread(int fd, void *buffer, size_t count) {
    LOGD("xread, fd: %d, count: %zu", fd, count);
    ssize_t total = 0;
    char *buf = (char *)buffer;
    while (count > 0) {
        ssize_t ret = read(fd, buf, count);
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

ssize_t xwrite(int fd, const void *buffer, size_t count) {
    LOGD("xwrite, fd: %d, count: %zu", fd, count);
    ssize_t total = 0;
    char *buf = (char *)buffer;
    while (count > 0) {
        ssize_t ret = write(fd, buf, count);
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

std::vector<std::string> split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }

    tokens.push_back(str.substr(start));
    return tokens;
}

std::string trim(const std::string& str) {
    // remove spaces, tabs, newlines
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    if (start == std::string::npos || end == std::string::npos || start > end) {
        return "";
    }
    return str.substr(start, end - start + 1);
}

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        LOGD("preAppSpecialize");
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        if (!args || !args->app_data_dir) {
            return;
        }

        auto app_data_dir = env->GetStringUTFChars(args->app_data_dir, nullptr);
        auto nice_name = env->GetStringUTFChars(args->nice_name, nullptr);

        std::string_view dir(app_data_dir);
        std::string_view process(nice_name);

        LOGD("process: %s", process.data());

        int fd = api->connectCompanion();
        LOGD("connectCompanion: %d", fd);
        if (fd < 0) {
            env->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);
            env->ReleaseStringUTFChars(args->nice_name, nice_name);
            return;
        }

        // send process name to companion
        int nameLen = (int)process.size();
        xwrite(fd, &nameLen, sizeof(nameLen));
        if (nameLen > 0) {
            xwrite(fd, process.data(), process.size() * sizeof(uint8_t));
        }

        bool matched;
        xread(fd, &matched, sizeof(matched));


        if (!matched) {
            env->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);
            env->ReleaseStringUTFChars(args->nice_name, nice_name);
            close(fd);
            return;
        }

        int configSize;
        std::string configStr;
        xread(fd, &configSize, sizeof(configSize));
        if (configSize > 0) {
            configStr.resize(configSize);
            xread(fd, configStr.data(), configSize * sizeof(uint8_t));
            // Parse config
            LOGD("Config: %s", configStr.c_str());
            LOGD("Parsing config");
            auto lines = split(configStr, "\n");
            LOGD("Parsed %zu lines", lines.size());
            for (auto &line: lines) {
                // skip empty lines
                LOGD("Line: %s", line.c_str());
                if (trim(line).empty()) {
                    LOGD("Empty line");
                    continue;
                }
                auto parts = split(line, "=");
                if (parts.size() != 2) {
                    LOGD("Invalid line");
                    continue;
                }
                auto key = trim(parts[0]);
                auto value = trim(parts[1]);
                spoofVars[key] = value;
                LOGD("Parsed: %s=%s", key.c_str(), value.c_str());
            }
            LOGD("Parsed %zu spoof vars", spoofVars.size());
            
        }
        close(fd);
        LOGD("Close companion, fd: %d", fd);
        LOGI("Config file size: %d", configSize);

        LOGI("Spoofing build vars for %s", process.data());
        UpdateBuildFields();
        LOGI("Spoofed build vars for %s", process.data());

        spoofVars.clear();
        env->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);
        env->ReleaseStringUTFChars(args->nice_name, nice_name);

    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        LOGD("preServerSpecialize");
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
    std::unordered_map<std::string, std::string> spoofVars;

    void UpdateBuildFields() {
        LOGD("UpdateBuildFields");
        jclass buildClass = env->FindClass("android/os/Build");
        LOGD("buildClass: %p", buildClass);
        jclass versionClass = env->FindClass("android/os/Build$VERSION");
        LOGD("versionClass: %p", versionClass);

        if (buildClass == nullptr || versionClass == nullptr) {
            LOGE("Failed to find Build or Build$VERSION class");
            if (buildClass) {
                env->DeleteLocalRef(buildClass);
            }
            if (versionClass) {
                env->DeleteLocalRef(versionClass);
            }
            return;
        }

        for (auto &[key, val]: spoofVars) {
            if (key.empty() || val.empty()) {
                continue;
            }

            const char *fieldName = key.c_str();
            bool isVersionField = false;

            jfieldID fieldID = env->GetStaticFieldID(buildClass, fieldName, "Ljava/lang/String;");

            if (env->ExceptionCheck()) {
                env->ExceptionClear();

                fieldID = env->GetStaticFieldID(versionClass, fieldName, "Ljava/lang/String;");

                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    continue;
                }

                isVersionField = true;
            }

            if (fieldID != nullptr) {
                const char *value = val.c_str();
                jstring jValue = env->NewStringUTF(value);

                env->SetStaticObjectField(isVersionField ? versionClass : buildClass, fieldID, jValue);

                env->DeleteLocalRef(jValue);

                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    LOGE("%s failed to set field '%s'", isVersionField ? "VERSION" : "Build", fieldName);
                    continue;
                }

                LOGI("%s set '%s' to '%s'", isVersionField ? "VERSION" : "Build", fieldName, value);
            }
        }

        env->DeleteLocalRef(buildClass);
        env->DeleteLocalRef(versionClass);
    }
};

static std::vector<uint8_t> readFile(const char *path) {
    FILE *file = fopen(path, "rb");

    if (!file) return {};

    int size = static_cast<int>(std::filesystem::file_size(path));

    std::vector<uint8_t> vector(size);

    fread(vector.data(), 1, size, file);

    fclose(file);

    return vector;
}

static void companion_handler(int fd) {
    LOGD("companion_handler, fd: %d", fd);

    // Read process name from fd
    int processNameSize;
    xread(fd, &processNameSize, sizeof(processNameSize));
    bool matched = false;
    if (processNameSize > 0) {
        std::string processName;
        processName.resize(processNameSize);
        xread(fd, processName.data(), processNameSize * sizeof(uint8_t));
        processName = split(processName, ":")[0]; // strip
        LOGD("Process name: %s", processName.c_str());

        // read target
        auto targetStrBuf = readFile(TARGET_FILE);
        std::string targetStr = std::string(targetStrBuf.begin(), targetStrBuf.end());
        if (targetStr.empty()) {
            LOGD("Using default target file");
            targetStr = DEFAULT_TARGETS;
        }
        auto targets = split(targetStr, "\n");
        for (auto &target: targets) {
            target = trim(target);
            LOGD("Target: %s", target.c_str());
            if (processName == target) {
                matched = true;
                break;
            }
        }
    }

    // send bool matched
    xwrite(fd, &matched, sizeof(matched));
    if (!matched) {
        LOGD("Process not matched, exiting companion_handler");
        return;
    }



    std::vector<uint8_t> config_data;

    config_data = readFile(CONFIG_FILE);
    LOGD("config_data size: %zu", config_data.size());
    if (config_data.empty()) {
        LOGD("Using default config file");
        config_data.resize(strlen(DEFAULT_CONFIG));
        memcpy(config_data.data(), DEFAULT_CONFIG, strlen(DEFAULT_CONFIG));
    }

    int configSize = static_cast<int>(config_data.size());

    xwrite(fd, &configSize, sizeof(configSize));

    if (configSize > 0) {
        xwrite(fd, config_data.data(), configSize * sizeof(uint8_t));
    }
    LOGD("companion_handler done, fd: %d", fd);
}

// Register our module class and the companion handler function
REGISTER_ZYGISK_MODULE(MyModule)
REGISTER_ZYGISK_COMPANION(companion_handler)

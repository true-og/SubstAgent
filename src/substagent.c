#include <errno.h>
#include <jvmti.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include "build_info.h"

static jvmtiEnv* jvmti = NULL;

typedef jint (JNICALL *open0_t)(JNIEnv*, jobject, jstring);
typedef jint (JNICALL *readBytes_t)(JNIEnv*, jobject, jbyteArray, jint, jint);
static volatile readBytes_t real_readBytes = NULL;
static volatile open0_t real_open0 = NULL;

struct file_data{
    signed char* data;
    size_t data_len;
    size_t offset;
};

struct file_entry{
    jint key;
    struct file_data value;
} *files_map = NULL;

// Check return value for .data == NULL
struct file_data make_file_data(const signed char* data, size_t len, size_t offset) {
    struct file_data fd;

    fd.data = malloc(len);
    if (fd.data == NULL) {
        fd.data_len = 0;
        return fd;
    }

    memcpy(fd.data, data, len);
    fd.data_len = len;
    fd.offset = offset;
    return fd;
}

static const jlong TAG = 1;

static void tag(jobject obj) {
    if (obj && jvmti) {
        (*jvmti)->SetTag(jvmti, obj, TAG);
    }
}

static int has_tag(jobject obj) {
    jlong tag = 0;
    if (obj && jvmti) {
        (*jvmti)->GetTag(jvmti, obj, &tag);
    }
    return tag == 1;
}

static int is_config_path(const char* path) {
    char* extString = strrchr(path, '.');
    if (extString) {
        return strcmp(extString, ".yml") == 0 || strcmp(extString, ".yaml") == 0 || strcmp(extString, ".json") == 0 || strcmp(extString, ".txt") == 0;
    }
    return 0;
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

static void substitute(const char* value, size_t value_len, int dollar_sign_matched_index, size_t i, ssize_t len_diff, jsize ba_length, signed char* arr, jsize* used_bytes, signed char* exceeded, size_t* exceeded_len) {
    signed char before[dollar_sign_matched_index];
    size_t after_len = MAX(ba_length - (ssize_t)i - len_diff, 0);
    signed char after[after_len];
    memcpy(before, arr, dollar_sign_matched_index);
    memcpy(after, arr+i, after_len);

    signed char result[ba_length];
    memcpy(result, before, dollar_sign_matched_index);
    memcpy(result + dollar_sign_matched_index, value, value_len);
    memcpy(result + dollar_sign_matched_index + value_len, after, after_len);


    if (len_diff < 0) {
        // len_diff is negative so +- which equates to -
        *used_bytes += len_diff;
    } else if (len_diff > 0) {
        ssize_t clamped_free_bytes = MAX(ba_length - *used_bytes, 0);
        size_t already_copied_bytes = 0;
        if (clamped_free_bytes > 0) {
            already_copied_bytes = MIN(len_diff, clamped_free_bytes);
            *used_bytes += already_copied_bytes;
        }
        size_t new_exceeded_len = *exceeded_len + len_diff - already_copied_bytes;
        exceeded = realloc(exceeded, new_exceeded_len);
        memcpy(exceeded, exceeded, *exceeded_len);
        memcpy(exceeded+*exceeded_len, arr + (ba_length - len_diff) + already_copied_bytes, len_diff - already_copied_bytes);
        *exceeded_len = new_exceeded_len;
    }

    memcpy(arr, result, ba_length);
}

static jint JNICALL readBytes_hook(JNIEnv* env, jobject thiz, jbyteArray buf, jint off, jint len) {
    jclass class = (*env)->GetObjectClass(env, thiz);
    jmethodID hash_code_method = (*env)->GetMethodID(env, class, "hashCode", "()I");

    jint hash = (*env)->CallIntMethod(env, thiz, hash_code_method);
    struct file_entry *fe = hmgetp(files_map, hash);

    jint r = real_readBytes(env, thiz, buf, off, len);
    if (!has_tag(thiz)) {
        return r;
    }

    jmethodID length_method_id = (*env)->GetMethodID(env, class, "length", "()J");
    jlong length = (*env)->CallLongMethod(env, thiz, length_method_id);

    jmethodID position_method_id = (*env)->GetMethodID(env, class, "position", "()J");
    jlong position = (*env)->CallLongMethod(env, thiz, position_method_id);

    int last_data = length - position == 0;

    jsize ba_length = (*env)->GetArrayLength(env, buf);
    jbyte *ba = (*env)->GetByteArrayElements(env, buf, JNI_FALSE);

    if (r == -1) {
        if (fe && fe->key != hash) {
            (*env)->ReleaseByteArrayElements(env, buf, ba, 0);
            return -1;
        } else {
            struct file_data fd = fe->value;
            size_t writeDataLen = MIN(fd.data_len, ba_length);
            memcpy(ba, fd.data, writeDataLen);
            if (ba_length < fd.data_len) {
                signed char data[fd.data_len - ba_length];
                memcpy(data, fd.data+ba_length, fd.data_len - ba_length);
                struct file_data new_fd = make_file_data(data, fd.data_len-ba_length, 0);
                if (new_fd.data == NULL) {
                    fprintf(stderr, "\e[0;31m[SubstAgent] Malloc failed\e[0m\n");
                    exit(EXIT_FAILURE);
                }
                hmput(files_map, hash, fd);
            }
            (*env)->ReleaseByteArrayElements(env, buf, ba, 0);
            return writeDataLen;
        }
    }

    jsize used_bytes = r;
    if (ba) {
        signed char* arr = malloc(ba_length);
        signed char* exceeded = malloc(0);
        size_t exceeded_len = 0;

        if (fe->key == hash) {
            struct file_data fd = fe->value;
            ssize_t orig_fitting_data_len = ba_length - fd.data_len;
            if (orig_fitting_data_len > 0) {
                memcpy(arr+fd.data_len, ba, orig_fitting_data_len);
            }
            size_t dataLen = MIN(fd.data_len, ba_length);
            ssize_t offset = 0;
            if (off != 0) {
                offset = fd.offset - off;
            }
            for (ssize_t i = offset; i < dataLen; i++) {
                if (i < 0) continue;
                arr[i] = fd.data[i];
            }
            
            ssize_t free_bytes = ba_length - used_bytes;
            size_t already_copied_bytes = 0;
            if (free_bytes > 0) {
                already_copied_bytes = MIN(fd.data_len, free_bytes);
                used_bytes += already_copied_bytes;
            }

            size_t left_over_exceeded = MAX((ssize_t)fd.data_len - (ssize_t)ba_length, 0);
            exceeded = realloc(exceeded, left_over_exceeded + fd.data_len - left_over_exceeded);
            memcpy(exceeded, fd.data+fd.data_len-left_over_exceeded, left_over_exceeded);
            memcpy(exceeded+left_over_exceeded, ba+(ba_length-fd.data_len)+left_over_exceeded, fd.data_len-left_over_exceeded);
            exceeded_len = left_over_exceeded + fd.data_len - left_over_exceeded;
        } else {
            memcpy(arr, ba, ba_length);
        }

        int dollar_sign_matched_index = -1;
        char env_var[4096];
        env_var[0] = '\0';
        for (size_t i = 0; i < len; i++) {
            unsigned char c = arr[i];
            if (dollar_sign_matched_index != -1) {
                if ((c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == 95) {
                    size_t len = strlen(env_var);
                    env_var[len] = c;
                    env_var[len + 1] = '\0';
                } else {
                    if (strlen(env_var) == 0) continue;
                    char* value = getenv(env_var);
                    if (!value) {
                        dollar_sign_matched_index = -1;
                        env_var[0] = '\0';
                        continue;
                    }

                    size_t env_len = i - dollar_sign_matched_index;
                    size_t value_len = strlen(value);
                    ssize_t len_diff = value_len - env_len;

                    substitute(value, value_len, dollar_sign_matched_index, i, len_diff, ba_length, arr, &used_bytes, exceeded, &exceeded_len);

                    dollar_sign_matched_index = -1;
                    env_var[0] = '\0';
                    continue;
                }
            } else {
                if (c == '$') {
                    dollar_sign_matched_index = i;
                }
            }
        }
        if (last_data && dollar_sign_matched_index != -1) {
            if (strlen(env_var) != 0) {
                char* value = getenv(env_var);
                if (value) {
                    size_t env_len = len - dollar_sign_matched_index;
                    size_t value_len = strlen(value);
                    ssize_t len_diff = value_len - env_len;

                    substitute(value, value_len, dollar_sign_matched_index, len, len_diff, ba_length, arr, &used_bytes, exceeded, &exceeded_len);
                }
            }
        }
        if (exceeded_len > 0) {
            struct file_data fd = make_file_data(exceeded, exceeded_len, ba_length);
            hmput(files_map, hash, fd);
        }
        memcpy(ba, arr, ba_length);
    }
    (*env)->ReleaseByteArrayElements(env, buf, ba, 0);
    return used_bytes;
}

static void open0_hook(JNIEnv* env, jobject thiz, jstring jpath) {
    if (!jpath) {
        return;
    }
    const char* path = (*env)->GetStringUTFChars(env, jpath, NULL);
    if (path) {
        if (is_config_path(path)) {
            tag(thiz);
        }
        (*env)->ReleaseStringUTFChars(env, jpath, path);
    }
    real_open0(env, thiz, jpath);
}

static void dealloc(jvmtiEnv* jvmti, char* p) {
    if (p != NULL) {
        unsigned char* up = (unsigned char*)p;
        (*jvmti)->Deallocate(jvmti, up);
    }
}

static void JNICALL onNativeMethodBind(jvmtiEnv* jvmti_env, JNIEnv* env, jthread thread, jmethodID method, void* address, void** new_address_ptr) {
    char* mname = NULL;
    char* msig  = NULL;
    char* csig  = NULL;
    char* gsig  = NULL;

    jclass decl;
    (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, method, &decl);
    (*jvmti_env)->GetClassSignature(jvmti_env, decl, &csig, &gsig);
    (*jvmti_env)->GetMethodName(jvmti_env, method, &mname, &msig, NULL);

    if (csig && strcmp(csig, "Ljava/io/FileInputStream;") == 0 && mname && msig) {
        if (strcmp(mname, "open0") == 0 && strcmp(msig, "(Ljava/lang/String;)V") == 0) {
            real_open0 = (open0_t)address;
            *new_address_ptr = (void*)&open0_hook;
        } else if (strcmp(mname, "readBytes") == 0 && strcmp(msig, "([BII)I") == 0) {
            real_readBytes = (readBytes_t)address;
            *new_address_ptr = (void*)&readBytes_hook;
        }
    }

    (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)mname);
    (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)msig);
    (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)csig);
    (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)gsig);
}

static jint init_jvmti(JavaVM* vm) {
    jint rc = (*vm)->GetEnv(vm, (void**)&jvmti, JVMTI_VERSION_1_2);
    if (rc != JNI_OK || jvmti == NULL) return JNI_ERR;

    jvmtiCapabilities caps;
    memset(&caps, 0, sizeof(caps));
    caps.can_generate_native_method_bind_events = 1;
    caps.can_tag_objects = 1;
    (*jvmti)->AddCapabilities(jvmti, &caps);

    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.NativeMethodBind = &onNativeMethodBind;
    (*jvmti)->SetEventCallbacks(jvmti, &callbacks, (jint)sizeof(callbacks));

    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_NATIVE_METHOD_BIND, (jthread)NULL);

    FILE *env_file = fopen(".env", "r");
    if (env_file) {
        char* line = NULL;
        size_t len = 0;
        ssize_t n;
        while ((n = getline(&line, &len, env_file)) != -1) {
            if (n > 0 && line[n - 1] == '\n') {
                line[n - 1] = '\0';
            }
            char *name = line;
            char *value = NULL;
            char *equal = strchr(line, '=');
            if (equal == NULL) {
                fprintf(stderr, "\e[0;31m[SubstAgent] Invalid line in .env: \"%s\"\e[0m\n", line);
                continue;
            }
            *equal = '\0';
            value = equal + 1;
            if (setenv(name, value, 0) != 0) {
                perror("setenv failed");
            }
        }
    } else {
        if (errno != ENOENT) {
            perror("[SubstAgent] fopen failed");
            exit(EXIT_FAILURE);
        }
    }

    fprintf(stderr, "[SubstAgent] Loaded version %s\n", GIT_HASH);

    return JNI_OK;
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
    return init_jvmti(vm);
}

JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
    return init_jvmti(vm);
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM* vm) {}

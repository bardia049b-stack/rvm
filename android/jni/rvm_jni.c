#include <android/log.h>
#include <jni.h>

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "rvm.h"
#include "vm/vm.h"

#define LOG_TAG   "rvm"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define JNI_EXPORT __attribute__((visibility("default")))

#define RING_CAP  8192
#define RING_MASK (RING_CAP - 1)

typedef struct {
    u8 buf[RING_CAP];
    _Alignas(64) atomic_size_t head;
    _Alignas(64) atomic_size_t tail;
} input_ring;

static void ring_init(input_ring *r) {
    atomic_store_explicit(&r->head, 0, memory_order_relaxed);
    atomic_store_explicit(&r->tail, 0, memory_order_relaxed);
}

static size_t ring_push(input_ring *r, const u8 *src, size_t n) {
    size_t tail = atomic_load_explicit(&r->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    size_t free_ = RING_CAP - (tail - head);
    if (n > free_)
        n = free_;
    for (size_t i = 0; i < n; i++)
        r->buf[(tail + i) & RING_MASK] = src[i];
    atomic_store_explicit(&r->tail, tail + n, memory_order_release);
    return n;
}

static int ring_poll(void *ud, u8 *dst, size_t max) {
    input_ring *r = (input_ring *)ud;
    size_t head = atomic_load_explicit(&r->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    size_t avail = tail - head;
    if (avail == 0)
        return 0;
    if (avail > max)
        avail = max;
    for (size_t i = 0; i < avail; i++)
        dst[i] = r->buf[(head + i) & RING_MASK];
    atomic_store_explicit(&r->head, head + avail, memory_order_release);
    return (int)avail;
}

typedef struct {
    vm vm;
    input_ring ring;

    JavaVM *jvm;
    jclass cls;
    jmethodID onOutput;
    jmethodID onLog;

    jbyteArray scratch;
    size_t scratch_len;

} session;

static void jni_log(void *ud, rvm_loglevel lvl, const char *line) {
    session *s = ud;
    JNIEnv *env = NULL;
    if (s->onLog == NULL)
        return;
    if ((*s->jvm)->GetEnv(s->jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK || env == NULL)
        return;
    jstring js = (*env)->NewStringUTF(env, line);
    if (js == NULL) {
        (*env)->ExceptionClear(env);
        return;
    }
    (*env)->CallStaticVoidMethod(env, s->cls, s->onLog, (jint)lvl, js);
    (*env)->DeleteLocalRef(env, js);
}

static void jni_write(void *ud, const u8 *buf, size_t n) {
    session *s = (session *)ud;
    if (n == 0 || s->cls == NULL)
        return;

    JNIEnv *env = NULL;
    if ((*s->jvm)->GetEnv(s->jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("jni_write: VM thread is not attached");
        return;
    }

    if (s->scratch == NULL || s->scratch_len < n) {
        if (s->scratch != NULL)
            (*env)->DeleteGlobalRef(env, s->scratch);
        size_t cap = n < 4096 ? 4096 : n;
        jbyteArray local = (*env)->NewByteArray(env, (jsize)cap);
        if (local == NULL)
            return;
        s->scratch = (jbyteArray)(*env)->NewGlobalRef(env, local);
        (*env)->DeleteLocalRef(env, local);
        if (s->scratch == NULL)
            return;
        s->scratch_len = cap;
    }

    (*env)->SetByteArrayRegion(env, s->scratch, 0, (jsize)n, (const jbyte *)buf);
    (*env)->CallStaticVoidMethod(env, s->cls, s->onOutput, s->scratch, (jint)n);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
}

static char *dup_string(JNIEnv *env, jstring js) {
    if (js == NULL)
        return NULL;
    const char *c = (*env)->GetStringUTFChars(env, js, NULL);
    if (c == NULL)
        return NULL;
    char *out = strdup(c);
    (*env)->ReleaseStringUTFChars(env, js, c);
    return out;
}

static void notify_exit(session *s, int code) {
    JNIEnv *env = NULL;
    if (s->cls == NULL)
        return;
    if ((*s->jvm)->GetEnv(s->jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK)
        return;
    jmethodID m = (*env)->GetStaticMethodID(env, s->cls, "onVmExit", "(I)V");
    if (m != NULL)
        (*env)->CallStaticVoidMethod(env, s->cls, m, (jint)code);
    if ((*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);
}

static JavaVM *g_jvm;

JNI_EXPORT jint JNI_OnLoad(JavaVM *jvm, void *reserved) {
    (void)reserved;
    g_jvm = jvm;
    LOGI("librvm_jni loaded");
    return JNI_VERSION_1_6;
}

JNI_EXPORT jlong JNICALL Java_dev_rvm_app_RvmNative_vmCreate(JNIEnv *env, jclass cls,
                                                             jstring kernel, jstring disk,
                                                             jstring initrd, jint ramMib,
                                                             jstring bootargs, jboolean trace) {
    session *s = calloc(1, sizeof *s);
    if (s == NULL)
        return 0;

    ring_init(&s->ring);
    s->jvm = g_jvm;

    s->cls = (jclass)(*env)->NewGlobalRef(env, cls);
    if (s->cls == NULL) {
        free(s);
        return 0;
    }
    s->onLog = (*env)->GetStaticMethodID(env, s->cls, "onLog", "(ILjava/lang/String;)V");
    if (s->onLog == NULL)
        (*env)->ExceptionClear(env);

    s->onOutput = (*env)->GetStaticMethodID(env, s->cls, "onConsoleOutput", "([BI)V");
    if (s->onOutput == NULL) {
        LOGE("onConsoleOutput([BI)V not found -- is proguard keeping RvmNative?");
        (*env)->ExceptionClear(env);
        (*env)->DeleteGlobalRef(env, s->cls);
        free(s);
        return 0;
    }

    vm_opts o;
    vm_opts_default(&o);

    char *k = dup_string(env, kernel);
    char *d = dup_string(env, disk);
    char *i = dup_string(env, initrd);
    char *b = dup_string(env, bootargs);

    o.kernel_path = k;
    o.disk_path = d;
    o.initrd_path = i;
    o.bootargs = b;
    o.dtb_path = NULL;
    o.ram_size = (u64)(ramMib > 0 ? ramMib : 512) << 20;
    o.create_disk = false;
    o.trace = false;
    o.log_level = trace ? RVM_LOG_DEBUG : RVM_LOG_WARN;
    if (trace && s->onLog != NULL)
        rvm_log_set_sink(jni_log, s);
    else
        rvm_log_set_sink(NULL, NULL);
    o.write = jni_write;
    o.write_ud = s;
    o.poll = ring_poll;
    o.poll_ud = &s->ring;

    rvm_err e = vm_new(&s->vm, &o);
    if (e != RVM_OK) {
        LOGE("vm_new failed: %d", (int)e);
        free(k);
        free(d);
        free(i);
        free(b);
        (*env)->DeleteGlobalRef(env, s->cls);
        free(s);
        return 0;
    }

    s->vm.opts.kernel_path = k;
    s->vm.opts.disk_path = d;
    s->vm.opts.initrd_path = i;
    s->vm.opts.bootargs = b;

    e = vm_load(&s->vm);
    if (e != RVM_OK) {
        LOGE("vm_load failed: %d", (int)e);
        jni_write(s, (const u8 *)"\r\n[error] failed to load the kernel\r\n", 38);
    }

    LOGI("session %p: ram=%llu MiB kernel=%s disk=%s", (void *)s,
         (unsigned long long)(o.ram_size >> 20), k ? k : "(none)", d ? d : "(none)");
    return (jlong)(intptr_t)s;
}

JNI_EXPORT jint JNICALL Java_dev_rvm_app_RvmNative_vmRun(JNIEnv *env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;
    session *s = (session *)(intptr_t)handle;
    if (s == NULL)
        return -1;
    rvm_err e = vm_run(&s->vm);
    int code = (e == RVM_OK) ? (int)s->vm.exit_code : -(int)e;
    notify_exit(s, code);
    return code;
}

JNI_EXPORT void JNICALL Java_dev_rvm_app_RvmNative_vmStop(JNIEnv *env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;
    session *s = (session *)(intptr_t)handle;
    if (s == NULL)
        return;
    vm_stop(&s->vm, 0);
}

JNI_EXPORT void JNICALL Java_dev_rvm_app_RvmNative_vmFree(JNIEnv *env, jclass cls, jlong handle) {
    (void)cls;
    session *s = (session *)(intptr_t)handle;
    if (s == NULL)
        return;
    vm_free(&s->vm);
    free((void *)s->vm.opts.kernel_path);
    free((void *)s->vm.opts.disk_path);
    free((void *)s->vm.opts.initrd_path);
    free((void *)s->vm.opts.bootargs);
    if (s->scratch != NULL)
        (*env)->DeleteGlobalRef(env, s->scratch);
    if (s->cls != NULL)
        (*env)->DeleteGlobalRef(env, s->cls);
    rvm_log_set_sink(NULL, NULL);
    free(s);
}

JNI_EXPORT void JNICALL Java_dev_rvm_app_RvmNative_vmInput(JNIEnv *env, jclass cls, jlong handle,
                                                           jbyteArray data, jint len) {
    (void)cls;
    session *s = (session *)(intptr_t)handle;
    if (s == NULL || data == NULL || len <= 0)
        return;
    jbyte tmp[256];
    jbyte *p = tmp;
    if ((size_t)len > sizeof tmp) {
        p = (jbyte *)malloc((size_t)len);
        if (p == NULL)
            return;
    }
    (*env)->GetByteArrayRegion(env, data, 0, len, p);
    if (!(*env)->ExceptionCheck(env))
        ring_push(&s->ring, (const u8 *)p, (size_t)len);
    else
        (*env)->ExceptionClear(env);
    if (p != tmp)
        free(p);
}

JNI_EXPORT void JNICALL Java_dev_rvm_app_RvmNative_vmInterrupt(JNIEnv *env, jclass cls,
                                                               jlong handle) {
    (void)env;
    (void)cls;
    session *s = (session *)(intptr_t)handle;
    if (s == NULL)
        return;
    const u8 etx = 3;
    ring_push(&s->ring, &etx, 1);
}

JNI_EXPORT jlong JNICALL Java_dev_rvm_app_RvmNative_vmInsns(JNIEnv *env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;
    session *s = (session *)(intptr_t)handle;
    return (s == NULL) ? 0 : (jlong)s->vm.insns;
}

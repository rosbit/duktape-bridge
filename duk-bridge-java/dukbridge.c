#include "duk_bridge.h"
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Class:     DukBridge
 * Method:    jsCreateEnv
 * Signature: (Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_DukBridge_jsCreateEnv(JNIEnv *env, jobject obj, jstring modPath)
{
	const char* mp = (*env)->GetStringUTFChars(env, modPath, 0);
	jlong e = (jlong)js_create_env(mp);
	(*env)->ReleaseStringUTFChars(env, modPath, 0);
	return e;
}

/*
 * Class:     DukBridge
 * Method:    jsDestroyEnv
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_DukBridge_jsDestroyEnv(JNIEnv *env, jobject obj, jlong jsEnv)
{
	void *e = (void*)jsEnv;
	js_destroy_env(e);
}

static JNIEnv *g_env = NULL;
static jobject g_fileReader = NULL;

static int fileReaderBridge(const char *fileName, char **content, size_t *len) {
	jclass clazz = (*g_env)->GetObjectClass(g_env, g_fileReader);
	jmethodID readFile = (*g_env)->GetMethodID(g_env, clazz, "readFile","(Ljava/lang/String;)[B");
	if (readFile == NULL) {
		return -1;
	}

	jstring fn = (*g_env)->NewStringUTF(g_env, fileName);
	jbyteArray c = (jbyteArray)(*g_env)->CallObjectMethod(g_env, g_fileReader, readFile, fn);
	if (c == NULL) {
		return -2;
	}

	jboolean copy = JNI_FALSE;
	jbyte *arr = (*g_env)->GetByteArrayElements(g_env, c, &copy);
	jsize alen = (*g_env)->GetArrayLength(g_env, c);
	*content = (char*)malloc(alen);
	memcpy(*content, arr, alen);
	*len = (size_t)alen;

	(*g_env)->ReleaseByteArrayElements(g_env, c, arr, JNI_ABORT);
	return 0;
}

/*
 * Class:     DukBridge
 * Method:    jsSetFileReader
 * Signature: (JLFileReader;)V
 */
JNIEXPORT void JNICALL Java_DukBridge_jsSetFileReader(JNIEnv *env, jobject obj, jlong jsEnv, jobject fr)
{
	void *e = (void*)jsEnv;
	if (fr == NULL) {
		return;
	}
	if (g_fileReader != NULL) {
		(*env)->DeleteGlobalRef(env, g_fileReader);
	}
	g_fileReader = (*env)->NewGlobalRef(env, fr);

	if (g_env == NULL) {
		g_env = env;
		js_set_readfile(fileReaderBridge);
	}
}

typedef struct {
	JNIEnv *env;
	jobject *res;
} cb_t;

static void setObject(cb_t *cb, const char* clsName, const char *initSignature, res_type_t res_type, jvalue v) {
	JNIEnv *env = cb->env;
	jclass cls = (*env)->FindClass(env, clsName);
	jmethodID constructor = (*env)->GetMethodID(env, cls, "<init>", initSignature);
	if (res_type == rt_double) {
		*(cb->res) = (*env)->NewObject(env, cls, constructor, v.d);
	} else {
		*(cb->res) = (*env)->NewObject(env, cls, constructor, v);
	}
}

static void resultReceived(void *udd, res_type_t res_type, void *res, size_t res_len) {
	cb_t *cb = (cb_t*)udd;
	jvalue v;

	switch (res_type) {
	case rt_none:
		return;
	case rt_bool:
		v.z = (jboolean)((long)res);
		return setObject(cb, "java/lang/Boolean", "(Z)V", res_type, v);
	case rt_int:
		v.i = (jint)(long)res;
		return setObject(cb, "java/lang/Integer", "(I)V", res_type, v);
	case rt_double:
		v.d = (jdouble)voidp2double(res);
		return setObject(cb, "java/lang/Double", "(D)V", res_type, v);
	case rt_string:
	case rt_object:
	case rt_buffer:
	case rt_array:
		break;
	default:
		return;
	}

	// all of the non-primitive data is converted to byte[]
	JNIEnv *env = cb->env;
	jbyteArray a = (*env)->NewByteArray(env, res_len);
	if (a != NULL) {
		(*env)->SetByteArrayRegion(env, a, 0, res_len, res);
		*(cb->res) = a;
		a = NULL;
	}

	// printf("%.*s\n", (int)res_len, (char*)res);
}

/*
 * Class:     DukBridge
 * Method:    jsEval
 * Signature: (J[B)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_DukBridge_jsEval(JNIEnv *env, jobject ojb, jlong jsEnv, jbyteArray jsCode)
{
	jboolean copy = JNI_FALSE;
	jbyte *jc = (*env)->GetByteArrayElements(env, jsCode, &copy);
	jsize len = (*env)->GetArrayLength(env, jsCode);

	jobject res = NULL;
	cb_t cb = {env, &res};

	void *e = (void*)jsEnv;
	int ret = js_eval(e, jc, len, resultReceived, &cb);

	(*env)->ReleaseByteArrayElements(env, jsCode, jc, JNI_ABORT);
	if (ret == 0) {
		return res;
	}
	return NULL;
}

/*
 * Class:     DukBridge
 * Method:    jsEvalFile
 * Signature: (JLjava/lang/String;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_DukBridge_jsEvalFile(JNIEnv *env, jobject obj, jlong jsEnv, jstring scriptFile)
{
	const char* sf = (*env)->GetStringUTFChars(env, scriptFile, 0);

	jobject res = NULL;
	cb_t cb = {env, &res};
	void *e = (void*)jsEnv;
	int ret = js_eval_file(e, sf, resultReceived, &cb);
	(*env)->ReleaseStringUTFChars(env, scriptFile, 0);
	if (ret == 0) {
		return res;
	}
	return NULL;
}

/*
 * Class:     DukBridge
 * Method:    jsRegisterFileFunc
 * Signature: (JLjava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_DukBridge_jsRegisterFileFunc(JNIEnv *env, jobject obj, jlong jsEnv, jstring scriptFile, jstring funcName)
{
	void *e = (void*)jsEnv;
	const char* sf = (*env)->GetStringUTFChars(env, scriptFile, 0);
	const char* fn = (*env)->GetStringUTFChars(env, funcName, 0);

	int res = js_register_file_func(e, sf, fn);

	(*env)->ReleaseStringUTFChars(env, scriptFile, 0);
	(*env)->ReleaseStringUTFChars(env, funcName, 0);
	return (jint)res;
}

/*
 * Class:     DukBridge
 * Method:    jsRegisterCodeFunc
 * Signature: (JLjava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_DukBridge_jsRegisterCodeFunc(JNIEnv *env, jobject obj, jlong jsEnv, jstring jsCode, jstring funcName)
{
	void *e = (void*)jsEnv;
	const char* jc = (*env)->GetStringUTFChars(env, jsCode, 0);
	size_t len = (*env)->GetStringUTFLength(env, jsCode);
	const char* fn = (*env)->GetStringUTFChars(env, funcName, 0);

	int res = js_register_code_func(e, jc, len, fn);

	(*env)->ReleaseStringUTFChars(env, jsCode, 0);
	(*env)->ReleaseStringUTFChars(env, funcName, 0);
	return (jint)res;
}

/*
 * Class:     DukBridge
 * Method:    jsUnregisterFunc
 * Signature: (JLjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_DukBridge_jsUnregisterFunc(JNIEnv *env, jobject ojb, jlong jsEnv, jstring funcName)
{
	void *e = (void*)jsEnv;
	const char* fn = (*env)->GetStringUTFChars(env, funcName, 0);
	js_unregister_func(e, fn);
	(*env)->ReleaseStringUTFChars(env, funcName, 0);
}

static void setValue(JNIEnv *env, char fmt, jobject val, void **v)
{
	jclass cls = (*env)->GetObjectClass(env, val);
	jmethodID getVal;
	switch (fmt) {
	case af_bool:
		getVal = (*env)->GetMethodID(env, cls, "booleanValue", "()Z");
		jboolean b = (*env)->CallBooleanMethod(env, val, getVal);
		if (b) {
			*v = (void*)((long)1);
		} else {
			*v = (void*)((long)0);
		}
		return;
	case af_int:
	case af_double:
		getVal = (*env)->GetMethodID(env, cls, "doubleValue", "()D");
		jdouble d = (*env)->CallDoubleMethod(env, val, getVal);
		*v = double2voidp(d);
		return;
	}
}

#define SET_BYTES_ELEM(i, j, n) do { \
	jobject val = (*env)->GetObjectArrayElement(env, args, i); \
	switch (fmt[i]) { \
		case af_none: \
			argv[j++] = NULL; \
			continue; \
		case af_bool: \
		case af_int: \
		case af_double: \
			setValue(env, fmt[i], val, &(argv[j])); \
			j++; continue; \
		default: break; \
	} \
	argv[j++] = (void*)(long)(*env)->GetArrayLength(env, val); \
	argv[j++] = (void*)(*env)->GetByteArrayElements(env, val, NULL); \
} while (++i < n);

#define RELEASE_BYTES(i, n) i=0; do { \
	(*env)->ReleaseByteArrayElements(env, ba[i], b[i], JNI_ABORT); \
} while (i++ < n);


typedef int (*fn_call_func)(void*,const char*, fn_call_func_res,void*,char*, void**);

static jobject do_call_func(JNIEnv *env, jobject obj, jlong jsEnv, jstring strarg, jstring fmtarg, jobjectArray args, fn_call_func call_func) {
	void *e = (void*)jsEnv;
	jobject res = NULL;
	cb_t cb = {env, &res};

	const char* fmt = (*env)->GetStringUTFChars(env, fmtarg, 0);
	const char* f = (*env)->GetStringUTFChars(env, strarg, 0);
	jsize len = (*env)->GetArrayLength(env, args);
	// printf("len of args: %d\n", len);
	int ret;
	
	if (len <= 0) {
		ret = call_func(e, f, resultReceived, &cb, NULL, (void**)NULL);
		goto EXIT;
	}

	void **argv = (void**)malloc(sizeof(void*) * len * 2);

	int i=0;
	int j=0;
	SET_BYTES_ELEM(i, j, len)
	ret = call_func(e, f, resultReceived, &cb, (char*)fmt, argv);
	// RELEASE_BYTES(i, len);  // b[i] belongs to args(jobjectArray), so calling ReleaseByteArrayElements() is not necessary.
	free((void*)argv);
EXIT:
	(*env)->ReleaseStringUTFChars(env, strarg, 0);
	(*env)->ReleaseStringUTFChars(env, fmtarg, 0);
	return res;
}

/*
 * Class:     DukBridge
 * Method:    jsCallFunc
 * Signature: (JLjava/lang/String;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_DukBridge_jsCallFunc(JNIEnv *env, jobject obj, jlong jsEnv, jstring funcName, jstring fmt, jobjectArray args)
{
	return do_call_func(env, obj, jsEnv, funcName, fmt, args, js_call_registered_func);
}

/*
 * Class:     DukBridge
 * Method:    jsCallFileFunc
 * Signature: (JLjava/lang/String;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_DukBridge_jsCallFileFunc(JNIEnv *env, jobject obj, jlong jsEnv, jstring scriptFile, jstring fmt, jobjectArray args)
{
	return do_call_func(env, obj, jsEnv, scriptFile, fmt, args, js_call_file_func);
}

/*
 * Class:     DukBridge
 * Method:    jsRegisterJavaFunc
 * Signature: (JLjava/lang/String;LNativeFunc;)I
 */
JNIEXPORT jint JNICALL Java_DukBridge_jsRegisterJavaFunc(JNIEnv *env, jobject obj, jlong jsEnv, jstring funcName, jobject nativeFunc)
{
	return -1;
}

/*
 * Class:     DukBridge
 * Method:    jsUnregisterJavaFunc
 * Signature: (JLjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_DukBridge_jsUnregisterJavaFunc(JNIEnv *env, jobject obj, jlong jsEnv, jstring funcName)
{
}

/*
 * Class:     DukBridge
 * Method:    jsAddModuleLoader
 * Signature: (JLNativeModuleLoader;)I
 */
JNIEXPORT jint JNICALL Java_DukBridge_jsAddModuleLoader(JNIEnv *env, jobject obj, jlong jsEnv, jobject nativeModuleLoader)
{
	return -1;
}

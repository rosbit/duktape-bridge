/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class DukBridge */

#ifndef _Included_DukBridge
#define _Included_DukBridge
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     DukBridge
 * Method:    jsCreateEnv
 * Signature: (Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_DukBridge_jsCreateEnv
  (JNIEnv *, jobject, jstring);

/*
 * Class:     DukBridge
 * Method:    jsDestroyEnv
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_DukBridge_jsDestroyEnv
  (JNIEnv *, jobject, jlong);

/*
 * Class:     DukBridge
 * Method:    jsSetFileReader
 * Signature: (JLFileReader;)V
 */
JNIEXPORT void JNICALL Java_DukBridge_jsSetFileReader
  (JNIEnv *, jobject, jlong, jobject);

/*
 * Class:     DukBridge
 * Method:    jsEval
 * Signature: (J[B)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_DukBridge_jsEval
  (JNIEnv *, jobject, jlong, jbyteArray);

/*
 * Class:     DukBridge
 * Method:    jsEvalFile
 * Signature: (JLjava/lang/String;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_DukBridge_jsEvalFile
  (JNIEnv *, jobject, jlong, jstring);

/*
 * Class:     DukBridge
 * Method:    jsRegisterFileFunc
 * Signature: (JLjava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_DukBridge_jsRegisterFileFunc
  (JNIEnv *, jobject, jlong, jstring, jstring);

/*
 * Class:     DukBridge
 * Method:    jsRegisterCodeFunc
 * Signature: (JLjava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_DukBridge_jsRegisterCodeFunc
  (JNIEnv *, jobject, jlong, jstring, jstring);

/*
 * Class:     DukBridge
 * Method:    jsUnregisterFunc
 * Signature: (JLjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_DukBridge_jsUnregisterFunc
  (JNIEnv *, jobject, jlong, jstring);

/*
 * Class:     DukBridge
 * Method:    jsCallFunc
 * Signature: (JLjava/lang/String;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_DukBridge_jsCallFunc
  (JNIEnv *, jobject, jlong, jstring, jstring, jobjectArray);

/*
 * Class:     DukBridge
 * Method:    jsCallFileFunc
 * Signature: (JLjava/lang/String;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_DukBridge_jsCallFileFunc
  (JNIEnv *, jobject, jlong, jstring, jstring, jobjectArray);

/*
 * Class:     DukBridge
 * Method:    jsRegisterJavaFunc
 * Signature: (JLjava/lang/String;LNativeFunc;)I
 */
JNIEXPORT jint JNICALL Java_DukBridge_jsRegisterJavaFunc
  (JNIEnv *, jobject, jlong, jstring, jobject);

/*
 * Class:     DukBridge
 * Method:    jsUnregisterJavaFunc
 * Signature: (JLjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_DukBridge_jsUnregisterJavaFunc
  (JNIEnv *, jobject, jlong, jstring);

/*
 * Class:     DukBridge
 * Method:    jsAddModuleLoader
 * Signature: (JLNativeModuleLoader;)I
 */
JNIEXPORT jint JNICALL Java_DukBridge_jsAddModuleLoader
  (JNIEnv *, jobject, jlong, jobject);

#ifdef __cplusplus
}
#endif
#endif
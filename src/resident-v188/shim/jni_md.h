/* jni_md.h for 32-bit ARM Android (the JDK's jni.h is used for the JNI
 * function table, which is the same on ART). */
#ifndef KOBO_JNI_MD_H
#define KOBO_JNI_MD_H
#define JNIEXPORT __attribute__((visibility("default")))
#define JNIIMPORT
#define JNICALL
typedef int jint;
typedef long long jlong;
typedef signed char jbyte;
#endif

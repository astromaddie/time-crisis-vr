#include "quest_coop.h"
#include <jni.h>
#include <stdlib.h>
#include <string.h>

static char *string_value(JNIEnv *env,jstring value){
    if(!value)return NULL;const char *utf=(*env)->GetStringUTFChars(env,value,NULL);if(!utf)return NULL;char *copy=strdup(utf);(*env)->ReleaseStringUTFChars(env,value,utf);return copy;
}
static jobjectArray strings_to_java(JNIEnv *env,char **strings,size_t count){
    jclass string_class=(*env)->FindClass(env,"java/lang/String");if(!string_class)return NULL;
    jobjectArray array=(*env)->NewObjectArray(env,(jsize)count,string_class,NULL);if(!array)return NULL;
    for(size_t i=0;i<count;i++){
        jstring item=(*env)->NewStringUTF(env,strings[i]?strings[i]:"");if(!item)continue;
        (*env)->SetObjectArrayElement(env,array,(jsize)i,item);(*env)->DeleteLocalRef(env,item);
    }
    return array;
}
static void free_strings(char **strings,size_t count){if(!strings)return;for(size_t i=0;i<count;i++)free(strings[i]);free(strings);}

JNIEXPORT jboolean JNICALL Java_org_timecrisis_quest_CoopLobbyBridge_nativeStartHost(JNIEnv *env,jclass cls,jstring romDir,jint tcpPort,jint discoveryPort){
    (void)cls;char *dir=string_value(env,romDir);bool ok=qcoop_start_host(dir,tcpPort,discoveryPort);free(dir);return ok?JNI_TRUE:JNI_FALSE;
}
JNIEXPORT jboolean JNICALL Java_org_timecrisis_quest_CoopLobbyBridge_nativeStartJoin(JNIEnv *env,jclass cls,jstring romDir,jstring ipv4,jint tcpPort,jint discoveryPort){
    (void)cls;char *dir=string_value(env,romDir),*host=string_value(env,ipv4);bool ok=qcoop_start_join(dir,host,tcpPort,discoveryPort);free(dir);free(host);return ok?JNI_TRUE:JNI_FALSE;
}
JNIEXPORT jstring JNICALL Java_org_timecrisis_quest_CoopLobbyBridge_nativeStatusJson(JNIEnv *env,jclass cls){
    (void)cls;char *json=qcoop_status_json();if(!json)return (*env)->NewStringUTF(env,"{\"state\":\"error\",\"message\":\"Status unavailable.\"}");jstring out=(*env)->NewStringUTF(env,json);free(json);return out;
}
JNIEXPORT jboolean JNICALL Java_org_timecrisis_quest_CoopLobbyBridge_nativeHostStart(JNIEnv *env,jclass cls){(void)env;(void)cls;return qcoop_host_start()?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jobjectArray JNICALL Java_org_timecrisis_quest_CoopLobbyBridge_nativeDiscover(JNIEnv *env,jclass cls,jstring romDir,jint discoveryPort,jint timeoutMs){
    (void)cls;(void)romDir;size_t count=0;char **entries=qcoop_discover(NULL,discoveryPort,timeoutMs,&count);jobjectArray out=strings_to_java(env,entries,count);free_strings(entries,count);return out;
}
JNIEXPORT jobjectArray JNICALL Java_org_timecrisis_quest_CoopLobbyBridge_nativeLocalIpv4Addresses(JNIEnv *env,jclass cls){
    (void)cls;size_t count=0;char **addresses=qcoop_local_ipv4_addresses(&count);jobjectArray out=strings_to_java(env,addresses,count);free_strings(addresses,count);return out;
}
JNIEXPORT void JNICALL Java_org_timecrisis_quest_CoopLobbyBridge_nativeCancel(JNIEnv *env,jclass cls){(void)env;(void)cls;qcoop_cancel(false);}
JNIEXPORT void JNICALL Java_org_timecrisis_quest_CoopLobbyBridge_nativeSelectSolo(JNIEnv *env,jclass cls){(void)env;(void)cls;qcoop_cancel(true);}

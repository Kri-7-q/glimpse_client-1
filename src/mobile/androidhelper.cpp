#include "androidhelper.h"

#include <QList>
#include <QHash>
#include <QDebug>

namespace {
    static JavaVM* g_vm = NULL;

    static QList<const char*> registeredClasses;
    static QHash<QByteArray, jclass> classMap;
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*)
{
    g_vm = vm;

    JNIEnv* env;
    vm->GetEnv((void**)&env, JNI_VERSION_1_6);

    foreach(const char* className, registeredClasses) {
        jclass clazz = env->FindClass(className);
        if ( clazz ) {
            classMap.insert(className, (jclass)env->NewGlobalRef(clazz));
        } else {
            qDebug() << Q_FUNC_INFO << "Failed to find class" << className;
        }
    }

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM* vm, void*)
{
    JNIEnv* env;
    vm->GetEnv((void**)&env, JNI_VERSION_1_6);

    foreach(jclass clazz, classMap)
        env->DeleteGlobalRef(clazz);
}

JavaVM* javaVM()
{
    return g_vm;
}

bool Java::attach()
{
    return (valid = javaVM()->AttachCurrentThread(&env, 0)) >= 0;
}

void Java::detach()
{
    javaVM()->DetachCurrentThread();
}

bool Java::isValid() const
{
    return valid;
}

JNIEnv *Java::operator ->() const
{
    return env;
}

void Java::registerClass(const char *className)
{
    if (!registeredClasses.contains(className))
        registeredClasses.append(className);
}

jclass Java::findClass(const QByteArray &className) const
{
    return classMap.value(className);
}

jobject Java::createInstance(jclass clazz) const
{
    if ( clazz == NULL )
        return NULL;

    jmethodID constructor = env->GetMethodID(clazz, "<init>", "()V");
    if ( !constructor )
        return NULL;

    return env->NewGlobalRef( env->NewObject(clazz, constructor) );
}

jobject Java::createInstance(const QByteArray &className) const
{
    return createInstance(findClass(className));
}

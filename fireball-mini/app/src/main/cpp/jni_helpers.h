#pragma once

#include <jni.h>
#include <string>
#include <vector>

namespace fireball::jni {

inline std::string JStringToStdString(JNIEnv* env, jstring jstr) {
  if (!jstr) return "";
  const char* chars = env->GetStringUTFChars(jstr, nullptr);
  if (!chars) return "";
  std::string result(chars);
  env->ReleaseStringUTFChars(jstr, chars);
  return result;
}

inline jstring StdStringToJString(JNIEnv* env, const std::string& str) {
  return env->NewStringUTF(str.c_str());
}

}  // namespace fireball::jni

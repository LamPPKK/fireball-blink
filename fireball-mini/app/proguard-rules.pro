# Proguard rules for Fireball Mini
-keepclassmembers class com.fireball.mini.core.FireballNativeBridge {
    public static native <methods>;
}
-keep class com.fireball.mini.core.models.** { *; }

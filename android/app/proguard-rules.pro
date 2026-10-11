-keepclasseswithmembernames class * {
    native <methods>;
}

-keep class dev.rvm.app.RvmNative {
    *;
}
-keepclassmembers class dev.rvm.app.RvmNative {
    void onConsoleOutput(byte[], int);
    void onFrame(byte[], int, int, int);
    void onVmExit(int);
    void onLog(int, java.lang.String);
}

-keepclasseswithmembers class dev.rvm.app.RvmView { <init>(...); }
-keepclasseswithmembers class dev.rvm.app.GfxView { <init>(...); }
-keepclasseswithmembers class dev.rvm.app.KeyBar  { <init>(...); }

-dontwarn java.lang.invoke.StringConcatFactory

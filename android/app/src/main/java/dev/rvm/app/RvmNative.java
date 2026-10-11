package dev.rvm.app;

public final class RvmNative {
    static {
        System.loadLibrary("rvm_jni");
    }

    private RvmNative() {}

    public interface LogSink { void onLog(int level, String line); }

    public static volatile LogSink logSink;

    static void onLog(int level, String line) {
        LogSink s = logSink;
        if (s != null) s.onLog(level, line);
    }

    public static native long vmCreate(String kernel, String disk, String initrd,
                                       int ramMib, String bootargs, boolean debug);

    public static native int vmRun(long handle);

    public static native void vmStop(long handle);

    public static native void vmFree(long handle);

    public static native void vmInput(long handle, byte[] data, int len);

    public static native void vmInterrupt(long handle);

    public static native long vmInsns(long handle);

    static volatile ConsoleSink consoleSink;
    static volatile FrameSink frameSink;
    static volatile ExitSink exitSink;

    public interface ConsoleSink { void onOutput(byte[] buf, int len); }
    public interface FrameSink   { void onFrame(byte[] rgba, int w, int h, int stride); }
    public interface ExitSink    { void onExit(int code); }

    @SuppressWarnings("unused")
    private static void onConsoleOutput(byte[] buf, int len) {
        ConsoleSink s = consoleSink;
        if (s != null) s.onOutput(buf, len);
    }

    @SuppressWarnings("unused")
    private static void onFrame(byte[] rgba, int w, int h, int stride) {
        FrameSink s = frameSink;
        if (s != null) s.onFrame(rgba, w, h, stride);
    }

    @SuppressWarnings("unused")
    private static void onVmExit(int code) {
        ExitSink s = exitSink;
        if (s != null) s.onExit(code);
    }
}

package dev.rvm.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.os.StatFs;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.PopupMenu;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedInputStream;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.GZIPInputStream;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public final class MainActivity extends Activity {
    private static final int PICK_KERNEL = 1;
    private static final int PICK_DISK = 2;
    private static final int PICK_INITRD = 3;

    private RvmView console;
    private GfxView gfx;
    private KeyBar keys;
    private TextView status;

    private final Handler ui = new Handler(Looper.getMainLooper());
    private volatile long vm;
    private volatile Thread vmThread;
    private volatile boolean sawOutput;
    private volatile boolean copying;
    private int ramMib = 1536;
    private int harts = 1;
    private boolean shellFirst;
    private PowerManager.WakeLock wake;
    private long bootAt;
    private String specRaw = "ram=1536 cpu=1";
    private boolean debug;
    private File kernel, disk, initrd;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        setContentView(R.layout.activity_main);

        console = findViewById(R.id.console);
        gfx = findViewById(R.id.gfx);
        keys = findViewById(R.id.keys);
        status = findViewById(R.id.status);

        kernel = new File(getFilesDir(), "vmlinux");
        disk = new File(getFilesDir(), "disk.img");
        initrd = new File(getFilesDir(), "initrd.img");
        ensureBuiltinInitrd();

        debug = getSharedPreferences("rvm", MODE_PRIVATE).getBoolean("debug", false);
        applySpec(loadSpec());
        applyLogSink();
        applyInsets();

        findViewById(R.id.title).setOnLongClickListener(new View.OnLongClickListener() {
            @Override public boolean onLongClick(View v) {
                debug = !debug;
                getSharedPreferences("rvm", MODE_PRIVATE).edit()
                    .putBoolean("debug", debug).apply();
                applyLogSink();
                toast(debug ? "debug log on: boot again and it will be captured"
                            : "debug log off");
                return true;
            }
        });

        wireKeys();
        wireMenu();
        trackIme();

        console.setKeyListener(new RvmView.KeyListener() {
            @Override public void onKey(byte ch) { send(new byte[] { ch }); }
        });

        gfx.setPointerListener(new GfxView.PointerListener() {
            @Override public void onMove(float nx, float ny) {  }
            @Override public void onButton(int button, boolean down) {  }
        });

        RvmNative.consoleSink = new RvmNative.ConsoleSink() {
            @Override public void onOutput(final byte[] buf, final int len) {
                sawOutput = true;
                final byte[] copy = new byte[len];
                System.arraycopy(buf, 0, copy, 0, len);
                ui.post(new Runnable() {
                    @Override public void run() { console.write(copy, len); }
                });
            }
        };
        RvmNative.frameSink = new RvmNative.FrameSink() {
            @Override public void onFrame(final byte[] rgba, final int w, final int h, final int s) {
                ui.post(new Runnable() {
                    @Override public void run() { gfx.setFrame(rgba, w, h, s); }
                });
            }
        };
        RvmNative.exitSink = new RvmNative.ExitSink() {
            @Override public void onExit(final int code) {
                ui.post(new Runnable() {
                    @Override public void run() {
                        status.setText("guest exited (" + code + ")");
                    }
                });
            }
        };

        console.clear();
        if (!kernel.exists()) {
            banner("No kernel yet.\n\nOpen the menu (top right) and pick Kernel: a riscv64 kernel, "
                 + "either the ELF vmlinux or the Image Debian ships as /boot/vmlinux-*. "
                 + "Then Disk, then Boot.  The initramfs is already in the app.\n\n"
                 + "Long-press the RVM title for a debug log.\n");
        } else {
            banner("Ready.\n  kernel: " + human(kernel.length())
                 + "\n  initrd: " + (initrd.exists() ? human(initrd.length()) : "(none)")
                 + "\n  disk:   " + (disk.exists() ? human(disk.length()) : "(none)")
                 + "\n\nMenu (top right) -> Boot.  Tap the screen for the keyboard, "
                 + "long-press to copy and paste.");
        }
    }

    private void applyInsets() {
        final View header = findViewById(R.id.header);
        final View keyscroll = findViewById(R.id.keyscroll);
        header.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override public WindowInsets onApplyWindowInsets(View v, WindowInsets in) {
                v.setPadding(v.getPaddingLeft(), in.getSystemWindowInsetTop(),
                             v.getPaddingRight(), v.getPaddingBottom());
                return in;
            }
        });
        keyscroll.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override public WindowInsets onApplyWindowInsets(View v, WindowInsets in) {
                v.setPadding(v.getPaddingLeft(), v.getPaddingTop(),
                             v.getPaddingRight(), in.getSystemWindowInsetBottom());
                return in;
            }
        });
    }

    private void applyLogSink() {
        RvmNative.logSink = debug ? new RvmNative.LogSink() {
            @Override public void onLog(final int level, final String line) {
                final char t = level <= 0 ? 'T' : level == 1 ? 'D' : level == 2 ? 'I'
                                 : level == 3 ? 'W' : 'E';
                final byte[] b = ("[rvm " + t + "] " + line + "\n").getBytes();
                ui.post(new Runnable() {
                    @Override public void run() { console.write(b, b.length); }
                });
            }
        } : null;
    }

    private void wireKeys() {
        keys.setSink(new KeyBar.Sink() {
            @Override public void send(byte[] seq) {
                boolean alt = keys.isAltLatched();
                boolean ctrl = keys.isCtrlLatched();
                keys.releaseLatches();
                if (alt) MainActivity.this.send(new byte[] { 27 });
                if (ctrl && seq.length == 1 && seq[0] >= 'a' && seq[0] <= 'z') {
                    MainActivity.this.send(new byte[] { (byte) (seq[0] - 'a' + 1) });
                    return;
                }
                MainActivity.this.send(seq);
            }

            @Override public void scrollPages(int dir) {
                if (dir == 0) console.scrollToBottom();
                else console.scrollPages(dir);
            }

            @Override public void hideIme() {
                setImeVisible(false);
            }
        });
    }

    private void trackIme() {
        final View root = findViewById(R.id.root);
        root.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override public void onLayoutChange(View v, int l, int t, int r, int b,
                                                 int ol, int ot, int or, int ob) {
                if (b == ot && r == or) return;
                android.graphics.Rect frame = new android.graphics.Rect();
                root.getWindowVisibleDisplayFrame(frame);
                int hidden = b - frame.bottom;
                boolean open = hidden > b / 4;
                if (open != imeOpen) {
                    imeOpen = open;
                    setKeysVisible(open);
                }
            }
        });
    }

    private boolean imeOpen;

    private void setKeysVisible(boolean show) {
        findViewById(R.id.keys).setVisibility(show ? View.VISIBLE : View.GONE);
        findViewById(R.id.keyscroll).setVisibility(show ? View.VISIBLE : View.GONE);
        findViewById(R.id.keyrule).setVisibility(show ? View.VISIBLE : View.GONE);
    }

    private void setImeVisible(boolean show) {
        InputMethodManager imm = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
        if (imm == null) return;
        if (show) { console.requestFocus(); imm.showSoftInput(console, 0); }
        else imm.hideSoftInputFromWindow(console.getWindowToken(), 0);
    }

    private void wireMenu() {
        findViewById(R.id.menu).setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { openMenu(); }
        });
    }

    private void openMenu() {
        PopupMenu menu = new PopupMenu(this, findViewById(R.id.menu));
        int act = (vmThread != null) ? R.string.stop : R.string.boot;
        int dbg = debug ? R.string.debug_on : R.string.debug_off;
        int sh = shellFirst ? R.string.shell_on : R.string.shell_off;
        int order = 0;
        menu.getMenu().add(0, act, order++, act);
        menu.getMenu().add(0, R.string.kernel, order++, R.string.kernel);
        menu.getMenu().add(0, R.string.disk, order++, R.string.disk);
        menu.getMenu().add(0, R.string.initrd, order++, R.string.initrd);
        menu.getMenu().add(0, R.string.machine, order++, R.string.machine);
        menu.getMenu().add(0, R.string.clear, order++, R.string.clear);
        menu.getMenu().add(0, sh, order++, sh);
        menu.getMenu().add(0, dbg, order++, dbg);
        menu.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
            @Override public boolean onMenuItemClick(android.view.MenuItem item) {
                int id = item.getItemId();
                if (id == R.string.boot || id == R.string.stop) {
                    if (vmThread != null) stopVm(); else startVm();
                } else if (id == R.string.kernel) pick(PICK_KERNEL);
                else if (id == R.string.disk) pick(PICK_DISK);
                else if (id == R.string.initrd) pick(PICK_INITRD);
                else if (id == R.string.machine) askMachine();
                else if (id == R.string.shell_on || id == R.string.shell_off) {
                    String next = shellFirst
                        ? specRaw.replaceAll("(?i)\\s*shell\\s*=\\s*1", "")
                        : specRaw.trim() + " shell=1";
                    applySpec(next);
                    saveSpec(next.trim());
                    toast((shellFirst ? "boot to shell: on - the init scripts are skipped"
                                      : "boot to shell: off - normal boot")
                          + (vmThread != null ? " - Boot again to apply" : ""));
                }
                else if (id == R.string.clear) console.clear();
                else if (id == R.string.debug_on || id == R.string.debug_off) {
                    debug = !debug;
                    getSharedPreferences("rvm", MODE_PRIVATE).edit()
                        .putBoolean("debug", debug).apply();
                    applyLogSink();
                    toast(debug ? "debug log on: boot again and it will be captured"
                                : "debug log off");
                }
                return true;
            }
        });
        menu.show();
    }

    private void send(byte[] seq) {
        long h = vm;
        if (h == 0 || seq == null || seq.length == 0) return;
        RvmNative.vmInput(h, seq, seq.length);
    }

    private static final int INITRD_REV = 2;
    private static final int INITRD_PICKED = -1;

    private void ensureBuiltinInitrd() {
        final int rev = getSharedPreferences("rvm", MODE_PRIVATE).getInt("initrd_rev", 0);
        if (rev == INITRD_PICKED || (rev == INITRD_REV && initrd.exists()))
            return;
        unpackBuiltinInitrd();
    }

    private boolean initrdHasShell(File f) {
        java.io.InputStream in = null;
        try {
            boolean gz = false;
            java.io.InputStream probe = new java.io.FileInputStream(f);
            try { gz = probe.read() == 0x1f && probe.read() == 0x8b; }
            finally { close(probe); }
            in = new java.io.BufferedInputStream(new java.io.FileInputStream(f), 1 << 16);
            if (gz) in = new java.util.zip.GZIPInputStream(in);
            java.io.ByteArrayOutputStream all = new java.io.ByteArrayOutputStream(1 << 20);
            byte[] buf = new byte[1 << 16];
            int got;
            while ((got = in.read(buf)) > 0) {
                all.write(buf, 0, got);
                if (all.size() > (64 << 20)) return true;
            }
            return all.toString("ISO-8859-1").indexOf("rvm.shell=root") >= 0;
        } catch (java.io.IOException e) {
            return true;
        } finally {
            close(in);
        }
    }

    private boolean unpackBuiltinInitrd() {
        final File tmp = new File(initrd.getParentFile(), initrd.getName() + ".new");
        boolean ok = false;
        java.io.InputStream in = null;
        java.io.OutputStream out = null;
        try {
            in = getAssets().open("initrd.img");
            out = new java.io.FileOutputStream(tmp);
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            out.close();
            out = null;
            ok = tmp.renameTo(initrd);
        } catch (java.io.IOException e) {
            ok = false;
        } finally {
            close(in); close(out);
            if (!ok) tmp.delete();
        }
        if (ok) {
            getSharedPreferences("rvm", MODE_PRIVATE).edit()
                .putInt("initrd_rev", INITRD_REV).apply();
        }
        return ok;
    }

    private void startVm() {
        if (vmThread != null) { toast("already running"); return; }
        if (copying) {
            toast("a file is still being copied - the bar at the top shows how many bytes");
            return;
        }
        if (!kernel.exists()) { toast("pick a kernel first"); return; }
        if (disk.exists() && disk.length() < (1L << 20)) {
            toast("disk.img is " + human(disk.length()) + ", far too small - copy it again");
            return;
        }

        final String k = kernel.getAbsolutePath();
        final String d = disk.exists() ? disk.getAbsolutePath() : null;
        final String i = initrd.exists() ? initrd.getAbsolutePath() : null;
        final String bootargs = "console=ttyS0 earlycon=ns16550a,mmio32,0x10000000 "
            + "root=/dev/vda rootwait rw nosoftlockup rvm.time=virtual"
            + (shellFirst ? " rvm.shell=root" : "");

        boolean hasShell = initrd.exists() && initrdHasShell(initrd);
        String idnote = "";
        if (shellFirst && initrd.exists() && !hasShell) {
            unpackBuiltinInitrd();
            hasShell = initrdHasShell(initrd);
            idnote = hasShell
                ? "\n  initrd: your copy predates Boot to shell (no rvm.shell=root in it),\n"
                  + "          so the initramfs inside this APK was used for this boot\n"
                : "\n  initrd: WARNING - neither your copy nor the APK's asset implements\n"
                  + "          rvm.shell=root, so Boot to shell will be ignored this boot\n";
        }

        console.clear();
        banner("Ready.\n  kernel: " + human(kernel.length())
               + "\n  disk:   " + (disk.exists() ? human(disk.length()) : "none")
               + "\n  initrd: " + (initrd.exists()
                       ? human(initrd.length())
                         + (hasShell ? ", Boot to shell supported" : ", NO Boot to shell support")
                       : "none")
               + "\n  ram:    " + ramMib + " MiB\n  cpu:    " + harts
               + (shellFirst ? "\n  shell:  root - the disk's init scripts are skipped\n" : "\n")
               + idnote
               + "  boot args: " + bootargs + "\n");
        sawOutput = false;
        status.setText(debug ? "starting, debug log on…" : "starting…");
        bootAt = System.currentTimeMillis();
        startKeepAwake();
        ui.removeCallbacks(beat);
        ui.postDelayed(beat, 10000);
        vmThread = new Thread(new Runnable() {
            @Override public void run() {
                long h = RvmNative.vmCreate(k, d, i, ramMib, bootargs, debug);
                if (h == 0) {
                    ui.post(new Runnable() {
                        @Override public void run() {
                            status.setText("failed to create the VM");
                            vmThread = null;
                        }
                    });
                    return;
                }
                vm = h;
                ui.post(new Runnable() {
                    @Override public void run() { status.setText("running"); }
                });
                final int code = RvmNative.vmRun(h);
                vm = 0;
                RvmNative.vmFree(h);
                ui.post(new Runnable() {
                    @Override public void run() {
                        status.setText("stopped (" + code + ")");
                        vmThread = null;
                    }
                });
            }
        }, "rvm-vm");
        vmThread.setPriority(Thread.NORM_PRIORITY);
        vmThread.start();
    }

    private void stopVm() {
        long h = vm;
        if (h != 0) RvmNative.vmStop(h);
        else toast("not running");
        releaseKeepAwake();
        ui.removeCallbacks(beat);
    }

    private void pick(int what) {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        startActivityForResult(i, what);
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (res != RESULT_OK || data == null || data.getData() == null) return;
        startCopy(data.getData(), (req == PICK_KERNEL) ? kernel
                : (req == PICK_DISK) ? disk : initrd);
    }

    private boolean copyIn(Uri uri, File dest) {
        File tmp = new File(dest.getParentFile(), dest.getName() + ".part");
        boolean ok = false;
        InputStream raw = null, in = null;
        FileOutputStream out = null;
        try {
            raw = getContentResolver().openInputStream(uri);
            if (raw == null) return false;
            BufferedInputStream bin = new BufferedInputStream(raw, 1 << 16);
            bin.mark(4);
            int a = bin.read(), b = bin.read();
            bin.reset();
            in = (a == 0x1f && b == 0x8b) ? (InputStream) new GZIPInputStream(bin, 1 << 16) : bin;
            out = new FileOutputStream(tmp);
            byte[] buf = new byte[1 << 16];
            long soFar = 0;
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
                if ((soFar >>> 23) != ((soFar + n) >>> 23)) announce(dest, soFar + n);
                soFar += n;
            }
            out.getFD().sync();
            out.close();
            out = null;
            in.close();
            in = null;
            if (dest.exists() && !dest.delete()) return false;
            ok = tmp.renameTo(dest);
            return ok;
        } catch (Exception e) {
            return false;
        } finally {
            close(raw); close(in); close(out);
            if (!ok) tmp.delete();
        }
    }

    static final class MachineSpec {
        int ramMib = 1536;
        int cpus = 1;
        boolean shell;
    }

    static MachineSpec parseSpec(String raw) {
        MachineSpec sp = new MachineSpec();
        if (raw == null) return sp;
        Matcher m = Pattern.compile("(?i)(?:ram\\s*=\\s*)?(\\d{3,4})(?:\\D+(?:cpu\\s*=\\s*)?(\\d+))?")
            .matcher(raw.trim());
        if (m.find()) {
            int ram = Integer.parseInt(m.group(1));
            sp.ramMib = Math.max(256, Math.min(4096, ram));
            if (m.group(2) != null)
                sp.cpus = Math.max(1, Integer.parseInt(m.group(2)));
        }
        sp.shell = Pattern.compile("(?i)shell\\s*=\\s*(1|root)\\b").matcher(raw).find();
        sp.cpus = 1;
        return sp;
    }

    private String loadSpec() {
        return getSharedPreferences("rvm", MODE_PRIVATE)
            .getString("machine_spec", "ram=1536 cpu=1");
    }

    private void applySpec(String raw) {
        MachineSpec sp = parseSpec(raw);
        ramMib = sp.ramMib;
        harts = sp.cpus;
        shellFirst = sp.shell;
        specRaw = (raw == null || raw.trim().isEmpty()) ? "ram=" + ramMib + " cpu=1" : raw.trim();
    }

    private void saveSpec(String raw) {
        getSharedPreferences("rvm", MODE_PRIVATE).edit().putString("machine_spec", raw).apply();
    }

    private void askMachine() {
        final EditText field = new EditText(this);
        field.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        field.setSingleLine(true);
        field.setTypeface(Typeface.MONOSPACE);
        field.setText(specRaw);
        field.setSelection(specRaw.length());
        new AlertDialog.Builder(this)
            .setTitle("Machine")
            .setMessage("ram=1536 cpu=1 - add shell=1 for a root shell with no init scripts")
            .setView(field)
            .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                @Override public void onClick(DialogInterface d, int w) {
                    String raw = field.getText().toString().trim();
                    applySpec(raw);
                    saveSpec(raw);
                    status.setText("ram " + ramMib + " MiB, cpu " + harts + " - boot to apply");
                }
            })
            .setNegativeButton(android.R.string.cancel, null)
            .show();
    }

    private void startCopy(final Uri uri, final File dest) {
        if (copying) { toast("still copying - wait for the size to show in the bar"); return; }
        long need = dest.getName().equals("disk.img") ? 3L << 30 : 64L << 20;
        long free = freeBytes();
        if (free > 0 && free < need) {
            status.setText("not enough room: " + human(free) + " free, about " + human(need) + " needed");
            toast("free up space, then pick the file again");
            return;
        }
        copying = true;
        status.setText("copying " + dest.getName() + "…");
        new Thread(new Runnable() {
            @Override public void run() {
                final boolean ok = copyIn(uri, dest);
                final long size = dest.length();
                copying = false;
                ui.post(new Runnable() {
                    @Override public void run() {
                        status.setText(ok ? dest.getName() + ": " + human(size)
                                          : "copy failed (not enough space?)");
                        if (ok && dest == initrd)
                            getSharedPreferences("rvm", MODE_PRIVATE).edit()
                                .putInt("initrd_rev", INITRD_PICKED).apply();
                    }
                });
            }
        }, "rvm-copy").start();
    }

    private long freeBytes() {
        try {
            return new StatFs(getFilesDir().getAbsolutePath()).getAvailableBytes();
        } catch (Exception e) {
            return -1;
        }
    }

    private void announce(final File dest, final long soFar) {
        final long bytes = soFar;
        final String name = dest.getName();
        ui.post(new Runnable() {
            @Override public void run() { status.setText("copying " + name + ": " + human(bytes)); }
        });
    }

    private final Runnable beat = new Runnable() {
        @Override public void run() {
            if (vmThread == null) { releaseKeepAwake(); return; }
            long s = (System.currentTimeMillis() - bootAt) / 1000;
            String el = (s / 60) + "m" + (s % 60 < 10 ? "0" : "") + (s % 60) + "s";
            status.setText(sawOutput
                ? "running, " + el
                : "booting, " + el + (s > 90
                    ? " - still silent; menu -> Boot to shell skips the init scripts" : ""));
            ui.postDelayed(this, 10000);
        }
    };

    private void startKeepAwake() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        try {
            if (wake == null) {
                PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
                wake = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "rvm:vm");
                wake.setReferenceCounted(false);
            }
            if (!wake.isHeld())
                wake.acquire(8 * 60 * 60 * 1000L);
        } catch (Exception e) {
            status.setText("no wake lock - keep the screen on or the guest will stall");
        }
    }

    private void releaseKeepAwake() {
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        try {
            if (wake != null && wake.isHeld()) wake.release();
        } catch (Exception ignored) { }
    }

    private void banner(String s) { console.write(s.getBytes(), s.getBytes().length); }

    private void toast(String s) {
        Toast t = Toast.makeText(this, s, Toast.LENGTH_SHORT);
        t.setGravity(Gravity.CENTER, 0, 0);
        t.show();
    }

    private static String human(long n) {
        if (n >= 1L << 30) return String.format("%.1f GiB", n / (double) (1L << 30));
        if (n >= 1L << 20) return String.format("%.1f MiB", n / (double) (1L << 20));
        if (n >= 1L << 10) return String.format("%.0f KiB", n / (double) (1L << 10));
        return n + " B";
    }

    private static void close(java.io.Closeable c) {
        if (c != null) try { c.close(); } catch (Exception ignored) { }
    }

    @Override
    protected void onDestroy() {
        stopVm();
        RvmNative.logSink = null;
        RvmNative.consoleSink = null;
        RvmNative.frameSink = null;
        RvmNative.exitSink = null;
        super.onDestroy();
    }
}

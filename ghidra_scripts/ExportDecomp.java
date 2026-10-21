// Decompile every named function (not FUN_*) and write one C file per namespace/class.
// Headless: -postScript ExportDecomp.java <outdir> [timeoutSecondsPerFunction]
//@category BFME
//@author bfme-native

import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class ExportDecomp extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args.length > 0 ? args[0] : askDirectory("output", "Export").getPath());
        int timeout = args.length > 1 ? Integer.parseInt(args[1]) : 60;
        out.mkdirs();
        DecompInterface di = new DecompInterface();
        DecompileOptions opt = new DecompileOptions();
        di.setOptions(opt);
        di.toggleCCode(true);
        di.openProgram(currentProgram);

        Map<String, StringBuilder> files = new TreeMap<>();
        int done = 0, failed = 0;
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        StringBuilder index = new StringBuilder("address\tnamespace\tname\tsize\n");
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            if (f.getName().startsWith("FUN_") || f.isThunk() || f.isExternal()) continue;
            String ns = f.getParentNamespace().isGlobal() ? "_global" : f.getParentNamespace().getName(true);
            String fileKey = ns.replaceAll("[^A-Za-z0-9_]", "_");
            DecompileResults r = di.decompileFunction(f, timeout, monitor);
            StringBuilder sb = files.computeIfAbsent(fileKey, k -> new StringBuilder("// " + k + " (decompiled from RotWK game.dat)\n\n"));
            sb.append("// ").append(f.getEntryPoint()).append("  ").append(f.getName(true)).append('\n');
            if (r != null && r.decompileCompleted()) {
                sb.append(r.getDecompiledFunction().getC()).append('\n');
                done++;
            } else {
                sb.append("/* decompile failed: ").append(r == null ? "null" : r.getErrorMessage()).append(" */\n\n");
                failed++;
            }
            index.append(f.getEntryPoint()).append('\t').append(ns).append('\t').append(f.getName())
                 .append('\t').append(f.getBody().getNumAddresses()).append('\n');
            if ((done + failed) % 200 == 0) println("decompiled " + (done + failed));
        }
        for (Map.Entry<String, StringBuilder> e : files.entrySet()) {
            try (Writer w = new FileWriter(new File(out, e.getKey() + ".c"))) { w.write(e.getValue().toString()); }
        }
        try (Writer w = new FileWriter(new File(out, "index.tsv"))) { w.write(index.toString()); }
        di.dispose();
        println(String.format("ExportDecomp: %d functions decompiled, %d failed, %d files -> %s",
                done, failed, files.size(), out));
    }
}

// Decompile specific functions to stdout-file.
// Args: <out.c> <spec>...   spec = hex address | name (Class::method) | str:<text> (every function
// that references a string containing <text>) | callers:<hex> (functions calling that address)
//@category BFME
//@author bfme-native

import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DecompileFns extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args[0]);
        LinkedHashSet<Function> fns = new LinkedHashSet<>();
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();
        Memory mem = currentProgram.getMemory();
        for (int i = 1; i < args.length; i++) {
            String s = args[i];
            if (s.startsWith("str:")) {
                byte[] needle = s.substring(4).getBytes("ISO-8859-1");
                for (MemoryBlock b : mem.getBlocks()) {
                    if (b.isExecute() || !b.isInitialized()) continue;
                    Address a = b.getStart();
                    while (a != null && a.compareTo(b.getEnd()) < 0) {
                        Address hit = mem.findBytes(a, b.getEnd(), needle, null, true, monitor);
                        if (hit == null) break;
                        // walk back to string start
                        Address st = hit;
                        while (st.compareTo(b.getStart()) > 0 && mem.getByte(st.subtract(1)) != 0) st = st.subtract(1);
                        for (Reference r : rm.getReferencesTo(st)) {
                            Function f = fm.getFunctionContaining(r.getFromAddress());
                            if (f != null) fns.add(f);
                        }
                        a = hit.add(1);
                    }
                }
            } else if (s.startsWith("callers:")) {
                Address t = toAddr(Long.parseLong(s.substring(8), 16));
                for (Reference r : rm.getReferencesTo(t)) {
                    Function f = fm.getFunctionContaining(r.getFromAddress());
                    if (f != null) fns.add(f);
                }
            } else if (s.matches("[0-9a-fA-F]{6,8}")) {
                Function f = fm.getFunctionContaining(toAddr(Long.parseLong(s, 16)));
                if (f != null) fns.add(f);
            } else {
                for (Symbol sym : currentProgram.getSymbolTable().getAllSymbols(true)) {
                    if (sym.getName(true).equals(s) || sym.getName().equals(s)) {
                        Function f = fm.getFunctionAt(sym.getAddress());
                        if (f != null) fns.add(f);
                    }
                }
            }
        }
        DecompInterface di = new DecompInterface();
        di.toggleCCode(true);
        di.openProgram(currentProgram);
        try (Writer w = new FileWriter(out)) {
            for (Function f : fns) {
                DecompileResults r = di.decompileFunction(f, 120, monitor);
                w.write("// ==== " + f.getEntryPoint() + "  " + f.getName(true) + "  (" +
                        f.getBody().getNumAddresses() + " bytes)\n");
                w.write(r != null && r.decompileCompleted() ? r.getDecompiledFunction().getC()
                        : "/* failed */\n");
                w.write("\n");
            }
        }
        di.dispose();
        println("DecompileFns: " + fns.size() + " functions -> " + out);
    }
}

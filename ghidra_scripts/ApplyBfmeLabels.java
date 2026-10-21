// Apply labels produced by the bfme-native scanners (data/labels.csv) to the current program.
// Creates functions where missing, names them (Class::method becomes a namespaced symbol),
// and leaves anything a human already renamed untouched.
//@category BFME
//@author bfme-native

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.*;
import ghidra.app.util.NamespaceUtils;
import java.io.*;
import java.util.*;

public class ApplyBfmeLabels extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File csv = args.length > 0 ? new File(args[0]) : askFile("bfme-native labels.csv", "Apply");
        SymbolTable st = currentProgram.getSymbolTable();
        int fnNew = 0, named = 0, kept = 0, bad = 0;
        try (BufferedReader r = new BufferedReader(new FileReader(csv))) {
            String line = r.readLine(); // header
            while ((line = r.readLine()) != null) {
                String[] f = line.split(",", -1);
                if (f.length < 3) continue;
                Address a = toAddr(Long.parseLong(f[0], 16));
                String full = f[1], kind = f[2];
                if (currentProgram.getMemory().getBlock(a) == null) { bad++; continue; }
                if (kind.equals("function")) {
                    Function fn = getFunctionAt(a);
                    if (fn == null) {
                        disassemble(a);
                        fn = createFunction(a, null);
                        if (fn != null) fnNew++;
                    }
                }
                Symbol cur = st.getPrimarySymbol(a);
                if (cur != null && cur.getSource() == SourceType.USER_DEFINED) { kept++; continue; }
                String ns = null, name = full;
                int i = full.lastIndexOf("::");
                if (i > 0) { ns = full.substring(0, i); name = full.substring(i + 2); }
                Namespace parent = currentProgram.getGlobalNamespace();
                if (ns != null)
                    parent = NamespaceUtils.createNamespaceHierarchy(ns, null, currentProgram, SourceType.ANALYSIS);
                Function fn = getFunctionAt(a);
                try {
                    if (fn != null) {
                        fn.setParentNamespace(parent);
                        fn.setName(name, SourceType.ANALYSIS);
                    } else {
                        Symbol s = st.createLabel(a, name, parent, SourceType.ANALYSIS);
                        s.setPrimary();
                    }
                    named++;
                } catch (Exception e) {
                    printerr("skip " + f[0] + " " + full + ": " + e.getMessage());
                    bad++;
                }
            }
        }
        println(String.format("bfme-native: %d named, %d functions created, %d kept (user-named), %d skipped",
                named, fnNew, kept, bad));
    }
}

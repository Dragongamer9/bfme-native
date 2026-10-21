// Dump per-function features for cross-binary matching and string-based naming.
// Output: JSON lines, one object per function:
//   {"ep":"00401000","name":"FUN_00401000","size":123,"ninsn":40,
//    "mhash":"…",   // hash of mnemonic sequence (operands dropped) - survives relocation
//    "strs":["…"],  // C strings referenced from the body
//    "calls":["00402000",…], "consts":[…]}  // callees, distinctive immediates
// Headless: -postScript ExportFeatures.java <out.jsonl>
//@category BFME
//@author bfme-native

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.security.MessageDigest;
import java.util.*;

public class ExportFeatures extends GhidraScript {
    private String esc(String s) {
        StringBuilder b = new StringBuilder();
        for (char c : s.toCharArray()) {
            if (c == '"' || c == '\\') b.append('\\').append(c);
            else if (c < 0x20 || c > 0x7e) b.append(String.format("\\u%04x", (int) c));
            else b.append(c);
        }
        return b.toString();
    }

    private String cstring(Address a) {
        try {
            Memory m = currentProgram.getMemory();
            MemoryBlock blk = m.getBlock(a);
            if (blk == null || blk.isExecute()) return null;
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < 300; i++) {
                byte c = m.getByte(a.add(i));
                if (c == 0) break;
                if (c < 0x20 && c != '\t' && c != '\n') return null;
                sb.append((char) (c & 0xff));
            }
            return sb.length() >= 4 ? sb.toString() : null;
        } catch (Exception e) {
            return null;
        }
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args.length > 0 ? args[0] : "features.jsonl");
        Listing listing = currentProgram.getListing();
        ReferenceManager rm = currentProgram.getReferenceManager();
        MessageDigest md = MessageDigest.getInstance("SHA-1");
        int n = 0;
        try (Writer w = new BufferedWriter(new FileWriter(out))) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (monitor.isCancelled()) break;
                if (f.isThunk() || f.isExternal()) continue;
                md.reset();
                int ninsn = 0;
                LinkedHashSet<String> strs = new LinkedHashSet<>();
                LinkedHashSet<String> calls = new LinkedHashSet<>();
                TreeSet<Long> consts = new TreeSet<>();
                for (Instruction ins : listing.getInstructions(f.getBody(), true)) {
                    ninsn++;
                    md.update(ins.getMnemonicString().getBytes());
                    for (int op = 0; op < ins.getNumOperands(); op++) {
                        md.update((byte) ins.getOperandType(op));
                        Scalar s = ins.getScalar(op);
                        if (s != null) {
                            long v = s.getUnsignedValue();
                            if (v > 0xff && v < 0x400000) consts.add(v);
                        }
                    }
                    for (Reference r : ins.getReferencesFrom()) {
                        Address to = r.getToAddress();
                        if (r.getReferenceType().isCall()) {
                            Function c = getFunctionAt(to);
                            if (c != null && c.isThunk()) c = c.getThunkedFunction(true);
                            if (c != null) calls.add(c.getEntryPoint().toString());
                        } else if (to.isMemoryAddress()) {
                            String s = cstring(to);
                            if (s != null) strs.add(s);
                        }
                    }
                }
                byte[] h = md.digest();
                StringBuilder hx = new StringBuilder();
                for (int i = 0; i < 10; i++) hx.append(String.format("%02x", h[i]));
                w.write("{\"ep\":\"" + f.getEntryPoint() + "\",\"name\":\"" + esc(f.getName(true)) +
                        "\",\"size\":" + f.getBody().getNumAddresses() + ",\"ninsn\":" + ninsn +
                        ",\"mhash\":\"" + hx + "\",\"strs\":[");
                int i = 0;
                for (String s : strs) { if (i++ > 0) w.write(","); w.write("\"" + esc(s) + "\""); }
                w.write("],\"calls\":[");
                i = 0;
                for (String c : calls) { if (i++ > 0) w.write(","); w.write("\"" + c + "\""); }
                w.write("],\"consts\":[");
                i = 0;
                for (Long c : consts) { if (i++ > 0) w.write(","); w.write(Long.toString(c)); if (i > 24) break; }
                w.write("]}\n");
                n++;
            }
        }
        println("ExportFeatures: " + n + " functions -> " + out);
    }
}

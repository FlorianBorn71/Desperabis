// Read-only, bounded discovery. Never creates instructions, functions, or references.
// @category Desperabis

import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.security.MessageDigest;
import java.util.*;
import java.util.regex.*;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.app.util.PseudoInstruction;
import ghidra.program.model.address.SegmentedAddressSpace;
import ghidra.program.model.lang.Register;
import ghidra.program.model.lang.UnknownInstructionException;
import ghidra.program.model.mem.MemoryBlock;

public class DiscoverObjectSpawns extends GhidraScript {
    private static final String HASH =
        "219525080e93e7a9d2b072b0dd75ed35bb7e41024e01e837c7889566723a9318";
    private static final int MAX_NODES = 12000;
    private static final Pattern STACK = Pattern.compile("\\[bp(?:([+-])(0x[0-9a-f]+|[0-9]+))?\\]");
    private static final Pattern MEMORY = Pattern.compile("(?:(es|ds):)?\\[(di|si|bx)(?:\\+(0x[0-9a-f]+|[0-9]+))?\\]");
    private PseudoDisassembler decoder;
    private SegmentedAddressSpace space;
    private final List<Region> regions = new ArrayList<>();
    private final List<Call> calls = new ArrayList<>();
    private final Map<Integer, Integer> argumentBytes = new HashMap<>();

    private static class Region {
        int segment, firstOffset;
        byte[] bytes;
        Map<Integer, Insn> decoded = new HashMap<>();
        Set<Integer> bad = new HashSet<>();
        Map<Integer, Set<Integer>> reachability = new HashMap<>();
    }

    private static class Call {
        Region region;
        int offset, targetSegment, targetOffset;
        boolean relocated, near;
        boolean allocator() { return targetSegment == 0x1e48 &&
            (targetOffset == 0x2049 || targetOffset == 0x2085); }
    }

    private static class Insn {
        int offset, length, opcode;
        byte[] bytes;
        String mnemonic, text;
        String[] operands;
        List<String> writes = new ArrayList<>();
    }

    private static class State {
        Map<String, String> values = new TreeMap<>();
        List<String> pushes = new ArrayList<>(); // Known suffix of the operand stack, top first.
        State copy() {
            State s = new State(); s.values.putAll(values); s.pushes.addAll(pushes); return s;
        }
        String get(String key) { return values.get(key); }
        void put(String key, String value) {
            if (value == null) { values.remove(key); } else { values.put(key, value); }
        }
        boolean intersect(State other) {
            int oldSize = values.size();
            values.entrySet().removeIf(e -> !e.getValue().equals(other.values.get(e.getKey())));
            List<String> common = new ArrayList<>();
            for (int i = 0; i < Math.min(pushes.size(), other.pushes.size()); ++i) {
                common.add(pushes.get(i).equals(other.pushes.get(i)) ? pushes.get(i) : "unknown");
            }
            boolean changed = oldSize != values.size() || !common.equals(pushes);
            pushes = common;
            return changed;
        }
    }

    private static int word(byte[] data, int at) {
        return (data[at] & 255) | ((data[at + 1] & 255) << 8);
    }

    private static String address(int segment, int offset) {
        return String.format("%04X:%04X", segment, offset);
    }

    private static String operand(String value) {
        return value.toLowerCase(Locale.ROOT).replace("word ptr", "").replace("byte ptr", "")
            .replace(" ", "").replace("+-", "-");
    }

    private static Integer number(String value) {
        if (value == null || !value.matches("-?(?:0x[0-9a-f]+|[0-9]+)")) { return null; }
        boolean negative = value.startsWith("-");
        String digits = negative ? value.substring(1) : value;
        int result = digits.startsWith("0x") ? Integer.parseInt(digits.substring(2), 16) :
            Integer.parseInt(digits);
        return (negative ? -result : result) & 65535;
    }

    private static String localKey(String value) {
        Matcher m = STACK.matcher(value);
        if (!m.matches()) { return null; }
        int displacement = m.group(2) == null ? 0 : number(m.group(2));
        if ("-".equals(m.group(1))) { displacement = -displacement; }
        return "stack:" + (short) displacement;
    }

    private static String value(State state, String operand) {
        Integer constant = number(operand);
        if (constant != null) { return "constant:" + constant; }
        String local = localKey(operand);
        return state.get(local != null ? local : operand);
    }

    private static Integer constant(String value) {
        return value != null && value.startsWith("constant:") ?
            Integer.parseInt(value.substring(9)) : null;
    }

    private Insn instruction(Region region, int offset) throws Exception {
        if (offset < region.firstOffset || offset >= region.bytes.length || region.bad.contains(offset)) { return null; }
        if (region.decoded.containsKey(offset)) { return region.decoded.get(offset); }
        PseudoInstruction decoded;
        try {
            decoded = decoder.disassemble(space.getAddress(region.segment + 0x1000, offset));
        } catch (UnknownInstructionException ex) {
            region.bad.add(offset);
            return null;
        }
        if (decoded == null || offset + decoded.getLength() > region.bytes.length) {
            region.bad.add(offset);
            return null;
        }
        Insn insn = new Insn();
        insn.offset = offset;
        insn.length = decoded.getLength();
        insn.bytes = Arrays.copyOfRange(region.bytes, offset, offset + insn.length);
        insn.opcode = insn.bytes[0] & 255;
        insn.mnemonic = decoded.getMnemonicString().toUpperCase(Locale.ROOT);
        insn.text = decoded.toString();
        insn.operands = new String[decoded.getNumOperands()];
        for (int i = 0; i < insn.operands.length; ++i) {
            insn.operands[i] = operand(decoded.getDefaultOperandRepresentation(i));
        }
        for (Object result : decoded.getResultObjects()) {
            if (result instanceof Register) {
                insn.writes.add(((Register) result).getName().toLowerCase(Locale.ROOT));
            }
        }
        region.decoded.put(offset, insn);
        return insn;
    }

    private static Integer branch(Insn insn) {
        int op = insn.opcode;
        if ((op == 0xe9 || op == 0xe8) && insn.length == 3) {
            return (insn.offset + 3 + (short) word(insn.bytes, 1)) & 65535;
        }
        if ((op == 0xeb || (op >= 0x70 && op <= 0x7f) || (op >= 0xe0 && op <= 0xe3)) &&
            insn.length == 2) {
            return (insn.offset + 2 + insn.bytes[1]) & 65535;
        }
        if (op == 0x0f && insn.length == 4 && (insn.bytes[1] & 255) >= 0x80 &&
            (insn.bytes[1] & 255) <= 0x8f) {
            return (insn.offset + 4 + (short) word(insn.bytes, 2)) & 65535;
        }
        return null;
    }

    private static String barrier(Insn insn) {
        if (insn.mnemonic.startsWith("RET") || insn.mnemonic.equals("IRET")) { return "return"; }
        if (insn.mnemonic.equals("ENTER")) { return "another ENTER prologue"; }
        if (insn.opcode == 0xcd && insn.length == 2 && (insn.bytes[1] & 255) >= 0x34 &&
            (insn.bytes[1] & 255) <= 0x3f) { return "floating-point emulator/inline-data boundary"; }
        if (insn.mnemonic.startsWith("JMP") && branch(insn) == null) {
            return "indirect or far jump";
        }
        if (insn.mnemonic.equals("HLT")) { return "HLT"; }
        return null;
    }

    private static List<Integer> successors(Insn insn) {
        Integer target = branch(insn);
        if (insn.mnemonic.equals("JMP")) { return Collections.singletonList(target); }
        if (target != null && (insn.mnemonic.startsWith("J") || insn.mnemonic.startsWith("LOOP"))) {
            return Arrays.asList(target, insn.offset + insn.length);
        }
        return Collections.singletonList(insn.offset + insn.length);
    }

    private Set<Integer> reachable(Region region, int entry) throws Exception {
        if (region.reachability.containsKey(entry)) { return region.reachability.get(entry); }
        Set<Integer> seen = new HashSet<>();
        ArrayDeque<Integer> queue = new ArrayDeque<>();
        Insn prologue = instruction(region, entry);
        queue.add(entry + prologue.length);
        seen.add(entry);
        while (!queue.isEmpty() && seen.size() < MAX_NODES) {
            monitor.checkCancelled();
            int offset = queue.remove();
            if (!seen.add(offset)) { continue; }
            Insn insn = instruction(region, offset);
            if (insn == null || barrier(insn) != null) { continue; }
            for (Integer next : successors(insn)) {
                if (next != null && next > entry && next < region.bytes.length) { queue.add(next); }
            }
        }
        region.reachability.put(entry, seen);
        return seen;
    }

    private int enclosing(Call call) throws Exception {
        int attempts = 0;
        for (int entry = call.offset - 1; entry >= 0 && attempts < 16; --entry) {
            if ((call.region.bytes[entry] & 255) != 0xc8 || entry + 3 >= call.region.bytes.length ||
                call.region.bytes[entry + 3] != 0) { continue; }
            ++attempts;
            if (reachable(call.region, entry).contains(call.offset)) { return entry; }
        }
        return -1;
    }

    private static void killRegister(State state, String register) {
        state.put(register, null);
        if (register.matches("[abcd][lh]")) { state.put(register.substring(0, 1) + "x", null); }
        if (register.matches("[abcd]x")) {
            state.put(register.substring(0, 1) + "l", null);
            state.put(register.substring(0, 1) + "h", null);
        }
    }

    private static State transfer(Insn insn, State input) {
        State result = input.copy();
        for (String register : insn.writes) { killRegister(result, register); }
        String[] op = insn.operands;
        if (insn.mnemonic.startsWith("CALL")) {
            for (String register : Arrays.asList("ax", "bx", "cx", "dx", "si", "di", "es")) {
                killRegister(result, register);
            }
            result.pushes.clear();
            return result;
        }
        if (op.length < 1) { return result; }
        String destination = op[0];
        String key = localKey(destination);
        if (key != null && !insn.mnemonic.equals("CMP") && !insn.mnemonic.equals("TEST") &&
            !insn.mnemonic.equals("PUSH")) {
            result.put(key, null);
            int displacement = Integer.parseInt(key.substring(6));
            result.put("stack:" + (displacement - 1), null);
            result.put("stack:" + (displacement + 1), null);
        }
        if (insn.mnemonic.equals("MOV") && op.length == 2) {
            boolean byteWrite = insn.text.toLowerCase(Locale.ROOT).contains("byte ptr");
            if (key != null) {
                result.put(key, byteWrite ? null : value(input, op[1]));
            } else if (destination.matches("ax|bx|cx|dx|si|di|ds|es")) {
                result.put(destination, value(input, op[1]));
            }
        } else if (insn.mnemonic.equals("IMUL") && op.length == 3 && number(op[2]) != null &&
            number(op[2]) == 54 && "slot".equals(value(input, op[1]))) {
            result.put(destination, "scaled-slot");
        } else if (insn.mnemonic.equals("ADD") && op.length == 2 &&
            "scaled-slot".equals(value(input, destination)) && Integer.valueOf(0x823c).equals(number(op[1]))) {
            result.put(destination, "object-pointer");
        } else if (insn.mnemonic.equals("LES") && op.length == 2) {
            String pointerKey = localKey(op[1]);
            if (pointerKey != null) {
                int displacement = Integer.parseInt(pointerKey.substring(6));
                result.put(destination, input.get(pointerKey));
                result.put("es", input.get("stack:" + (displacement + 2)));
            }
        } else if (insn.mnemonic.equals("LEA") && op.length == 2 && localKey(op[1]) != null) {
            result.put(destination, "frame-address");
        } else if (insn.mnemonic.equals("XOR") && op.length == 2 && op[0].equals(op[1])) {
            if (destination.matches("ax|bx|cx|dx|si|di")) { result.put(destination, "constant:0"); }
        } else if (insn.mnemonic.equals("PUSH")) {
            String pushed = value(input, destination);
            if ("slot".equals(pushed)) { pushed = "slot@" + insn.offset; }
            if (destination.equals("cs")) { pushed = "code-segment"; }
            if (destination.equals("bp")) { pushed = "frame-address"; }
            result.pushes.add(0, pushed == null ? "unknown" : pushed);
            if (result.pushes.size() > 64) { result.pushes.remove(64); }
        } else if (insn.mnemonic.equals("POP")) {
            if (!result.pushes.isEmpty()) { result.pushes.remove(0); }
        }
        if (insn.writes.contains("sp") && !insn.mnemonic.equals("PUSH") && !insn.mnemonic.equals("POP")) {
            result.pushes.clear();
        }
        return result;
    }

    private Region regionAt(int linear) {
        for (Region region : regions) {
            int offset = linear - region.segment * 16;
            if (offset >= region.firstOffset && offset < region.bytes.length) { return region; }
        }
        return null;
    }

    private Integer calleeLinear(Region region, Insn insn) {
        if (insn.opcode == 0x9a && insn.length == 5) {
            return (word(insn.bytes, 3) - 0x1000) * 16 + word(insn.bytes, 1);
        }
        if (insn.opcode == 0xe8 && insn.length == 3) { return region.segment * 16 + branch(insn); }
        return null;
    }

    private int cleanupBytes(int linear) throws Exception {
        if (argumentBytes.containsKey(linear)) { return argumentBytes.get(linear); }
        Region region = regionAt(linear);
        if (region == null) { argumentBytes.put(linear, -1); return -1; }
        int start = linear - region.segment * 16;
        ArrayDeque<Integer> queue = new ArrayDeque<>();
        Set<Integer> seen = new HashSet<>();
        queue.add(start);
        Integer cleanup = null;
        boolean unresolved = false;
        while (!queue.isEmpty() && seen.size() < 2048) {
            monitor.checkCancelled();
            int offset = queue.remove();
            if (!seen.add(offset)) { continue; }
            Insn insn = instruction(region, offset);
            if (insn == null) { unresolved = true; continue; }
            if (insn.opcode == 0xca && insn.length == 3) {
                int bytes = word(insn.bytes, 1);
                if (cleanup != null && cleanup != bytes) { unresolved = true; }
                cleanup = bytes;
                continue;
            }
            if (insn.opcode == 0xcb) {
                if (cleanup != null && cleanup != 0) { unresolved = true; }
                cleanup = 0;
                continue;
            }
            if (offset != start && barrier(insn) != null) { unresolved = true; continue; }
            if (offset == start && barrier(insn) != null && !insn.mnemonic.equals("ENTER")) {
                unresolved = true; continue;
            }
            for (Integer next : successors(insn)) {
                if (next == null || next < region.firstOffset || next >= region.bytes.length) {
                    unresolved = true;
                } else { queue.add(next); }
            }
        }
        int bytes = unresolved || !queue.isEmpty() || cleanup == null || (cleanup & 1) != 0 ? -1 : cleanup;
        argumentBytes.put(linear, bytes);
        return bytes;
    }

    private List<String> argumentStack(Insn insn, State input) {
        List<String> stack = new ArrayList<>(input.pushes);
        if (insn.opcode == 0xe8) {
            if (stack.isEmpty() || !stack.get(0).equals("code-segment")) { return Collections.emptyList(); }
            stack.remove(0);
        }
        return stack;
    }

    private State transferCall(Region region, Insn insn, State input) throws Exception {
        State output = transfer(insn, input);
        Integer linear = calleeLinear(region, insn);
        int bytes = linear == null ? -1 : cleanupBytes(linear);
        List<String> stack = argumentStack(insn, input);
        List<String> consumed = bytes < 0 ? input.pushes :
            stack.subList(0, Math.min(stack.size(), bytes / 2));
        if (consumed.contains("frame-address")) {
            output.values.keySet().removeIf(k -> k.startsWith("stack:"));
        }
        if (bytes < 0) { return output; }
        if (bytes / 2 <= stack.size()) { output.pushes.addAll(stack.subList(bytes / 2, stack.size())); }
        return output;
    }

    private static Integer objectField(Insn insn, State state) {
        if (!insn.mnemonic.equals("MOV") || insn.operands.length != 2) { return null; }
        Matcher m = MEMORY.matcher(insn.operands[0]);
        if (!m.matches()) { return null; }
        int displacement = m.group(3) == null ? 0 : number(m.group(3));
        String base = state.get(m.group(2));
        String segment = state.get(m.group(1) == null ? "ds" : m.group(1));
        if (!"data-segment".equals(segment)) { return null; }
        if ("scaled-slot".equals(base)) { displacement -= 0x823c; }
        else if (!"object-pointer".equals(base)) { return null; }
        return displacement >= 0 && displacement < 54 ? displacement : null;
    }

    private static Insn testInstruction(String mnemonic, String text, String... operands) {
        Insn insn = new Insn();
        insn.mnemonic = mnemonic;
        insn.text = text;
        insn.operands = operands;
        return insn;
    }

    private static void require(boolean condition, String message) {
        if (!condition) { throw new IllegalStateException("Analyzer self-test: " + message); }
    }

    private static void selfTest() {
        State state = new State();
        state.put("ds", "data-segment");
        state.put("ax", "slot");
        state = transfer(testInstruction("MOV", "MOV word ptr [BP + -0x2],AX", "[bp-0x2]", "ax"), state);
        state = transfer(testInstruction("IMUL", "IMUL DI,word ptr [BP + -0x2],0x36",
            "di", "[bp-0x2]", "0x36"), state);
        Insn indexed = testInstruction("MOV", "MOV word ptr [DI + 0x8244],0x15", "[di+0x8244]", "0x15");
        require(Integer.valueOf(8).equals(objectField(indexed, state)), "indexed world-array provenance");
        state = transfer(testInstruction("ADD", "ADD DI,0x823c", "di", "0x823c"), state);
        state = transfer(testInstruction("MOV", "MOV word ptr [BP + -0xc],DI", "[bp-0xc]", "di"), state);
        state = transfer(testInstruction("MOV", "MOV word ptr [BP + -0xa],DS", "[bp-0xa]", "ds"), state);
        state = transfer(testInstruction("CALLF", "CALLF unknown"), state);
        require(state.get("di") == null && "slot".equals(state.get("stack:-2")),
            "opaque call invalidates registers but retains unaliased locals under documented convention");
        state = transfer(testInstruction("LES", "LES DI,[BP + -0xc]", "di", "[bp-0xc]"), state);
        Insn behavior = testInstruction("MOV", "MOV word ptr ES:[DI + 0x8],0x15", "es:[di+0x8]", "0x15");
        require(Integer.valueOf(8).equals(objectField(behavior, state)), "far-pointer reload provenance");
        State unrelated = new State();
        require(objectField(behavior, unrelated) == null, "offset eight alone is not an object");
        State wrongSegment = state.copy();
        wrongSegment.put("es", null);
        require(objectField(behavior, wrongSegment) == null, "unknown segment cannot establish object identity");
        State joined = state.copy();
        joined.intersect(unrelated);
        require(objectField(behavior, joined) == null, "branch merge must drop one-sided facts");
        State partial = transfer(testInstruction("MOV", "MOV byte ptr [BP + -0x1],0x0",
            "[bp-0x1]", "0x0"), state);
        require(partial.get("stack:-2") == null, "byte overlap invalidates saved slot word");
        Insn unknownWrite = testInstruction("MUL", "MUL CX", "cx");
        unknownWrite.writes.add("di");
        require(transfer(unknownWrite, state).get("di") == null, "unsupported register writes kill provenance");
        require(localKey("[bp-0xc]").equals("stack:-12"), "signed local displacement");
        require(Integer.valueOf(65535).equals(number("-0x1")), "word constant wrapping");
        Insn push = testInstruction("PUSH", "PUSH AX", "ax");
        State pushed = transfer(push, initialSlotState());
        require(pushed.pushes.equals(Collections.singletonList("slot@0")), "slot push provenance");
        pushed = transfer(testInstruction("PUSH", "PUSH 0x3", "0x3"), pushed);
        require(pushed.pushes.get(0).equals("constant:3") && pushed.pushes.get(1).equals("slot@0"),
            "a pending slot must remain below later arguments");
        require(transfer(testInstruction("POP", "POP AX", "ax"), pushed).pushes.size() == 1,
            "POP consumes only the known stack top");
    }

    private static State initialSlotState() {
        State state = new State();
        state.put("ax", "slot");
        return state;
    }

    private void trace(Call call, int entry, PrintWriter out) throws Exception {
        int start = call.offset + (call.near ? 3 : 5);
        State initial = new State();
        initial.put("ax", "slot");
        initial.put("ds", "data-segment");
        traceFrom(call.region, entry, start, initial, out, 0);
    }

    private void traceFrom(Region region, int entry, int start, State initial, PrintWriter out, int depth)
            throws Exception {
        Map<Integer, State> inputs = new TreeMap<>();
        inputs.put(start, initial);
        ArrayDeque<Integer> queue = new ArrayDeque<>();
        queue.add(start);
        Set<String> stops = new TreeSet<>();
        int steps = 0;
        while (!queue.isEmpty() && ++steps <= MAX_NODES * 4 && inputs.size() < MAX_NODES) {
            monitor.checkCancelled();
            int offset = queue.remove();
            Insn insn = instruction(region, offset);
            if (insn == null) { stops.add(address(region.segment, offset) + " undecodable/outside region"); continue; }
            String stop = barrier(insn);
            if (stop != null) { stops.add(address(region.segment, offset) + " " + stop); continue; }
            if (insn.mnemonic.startsWith("CALL") && insn.opcode != 0x9a && insn.opcode != 0xe8) {
                stops.add(address(region.segment, offset) + " indirect call (effects unresolved)");
                continue;
            }
            if (insn.opcode == 0x9a && insn.length == 5 &&
                word(insn.bytes, 3) == 0x2e48 &&
                (word(insn.bytes, 1) == 0x2049 || word(insn.bytes, 1) == 0x2085)) {
                stops.add(address(region.segment, offset) + " next allocation");
                continue;
            }
            State output = insn.mnemonic.startsWith("CALL") ?
                transferCall(region, insn, inputs.get(offset)) : transfer(insn, inputs.get(offset));
            for (Integer next : successors(insn)) {
                if (next == null || next <= entry || next >= region.bytes.length) {
                    stops.add(address(region.segment, offset) + " branch outside enclosing region");
                    continue;
                }
                State previous = inputs.get(next);
                if (previous == null) { inputs.put(next, output.copy()); queue.add(next); }
                else if (previous.intersect(output)) { queue.add(next); }
            }
        }
        if (!queue.isEmpty()) {
            out.println("  UNRESOLVED: analysis budget reached; no assignments promoted from this site.");
            return;
        }
        int fields = 0, behaviors = 0;
        Map<Integer, State> helpers = new TreeMap<>();
        for (Map.Entry<Integer, State> point : inputs.entrySet()) {
            Insn insn = instruction(region, point.getKey());
            if (insn == null) { continue; }
            if (insn.mnemonic.startsWith("CALL")) {
                Integer linear = calleeLinear(region, insn);
                int bytes = linear == null ? -1 : cleanupBytes(linear);
                List<String> stack = argumentStack(insn, point.getValue());
                for (int i = 0; i < stack.size(); ++i) {
                    String fact = stack.get(i);
                    if (!fact.startsWith("slot@") || (bytes >= 0 && i >= bytes / 2)) { continue; }
                    String target = insn.opcode == 0x9a ?
                        address(word(insn.bytes, 3) - 0x1000, word(insn.bytes, 1)) :
                        address(region.segment, branch(insn));
                    out.printf("  %s %s -> %s: slot PUSH at %s, callee BP+%02X;%n",
                        bytes < 0 ? "PENDING-SLOT CANDIDATE" : "SLOT ARGUMENT",
                        address(region.segment, insn.offset), target,
                        address(region.segment, Integer.parseInt(fact.substring(5))), 6 + i * 2);
                    out.println(bytes < 0 ? "    cleanup unresolved; pending slot may belong to a later call." :
                        "    cleanup established from bounded RETF paths; child behavior requires helper tracing.");
                    if (bytes >= 0 && depth == 0 && linear != null) {
                        State helper = new State();
                        helper.put("ds", "data-segment");
                        for (int argument = 0; argument < Math.min(stack.size(), bytes / 2); ++argument) {
                            String argumentValue = stack.get(argument);
                            if (argumentValue.startsWith("slot@")) { argumentValue = "slot"; }
                            if (!argumentValue.equals("unknown")) {
                                helper.put("stack:" + (6 + argument * 2), argumentValue);
                            }
                        }
                        State previous = helpers.get(linear);
                        if (previous == null) { helpers.put(linear, helper); } else { previous.intersect(helper); }
                    }
                }
            }
            Integer field = objectField(insn, point.getValue());
            if (field == null) { continue; }
            ++fields;
            Integer assigned = constant(value(point.getValue(), insn.operands[1]));
            boolean wordWrite = insn.text.toLowerCase(Locale.ROOT).contains("word ptr") &&
                !insn.text.toLowerCase(Locale.ROOT).contains("dword ptr");
            if (field == 8 && wordWrite && assigned != null) { ++behaviors; }
            out.printf("  %s field +%02X%s = %s ; %s%n",
                address(region.segment, insn.offset), field,
                field == 8 ? (wordWrite ? " BEHAVIOR WORD" : " behavior byte/width unresolved") : "",
                assigned == null ? "UNRESOLVED" : String.format("%d (0x%04X)", assigned, assigned),
                insn.text);
        }
        out.printf("  Provenance-linked stores: %d; constant behavior-word stores: %d%n", fields, behaviors);
        if (behaviors == 0) { out.println("  UNRESOLVED: no constant child behavior established."); }
        for (String stop : stops) { out.println("  Trace boundary: " + stop); }
        out.println("  Calls: register facts invalidated; explicit frame-address arguments also invalidate BP facts.");
        out.println("  Other BP facts assume no hidden callee alias mutation.");
        out.println("  Stack suffix tracks PUSH/POP and bounded RETF cleanup; unknown cleanup discards pending facts.");
        out.println("  Branches: both arms explored, not proven feasible; merged facts use intersection.");
        for (Map.Entry<Integer, State> helper : helpers.entrySet()) {
            Region targetRegion = regionAt(helper.getKey());
            if (targetRegion == null) { continue; }
            int targetOffset = helper.getKey() - targetRegion.segment * 16;
            Insn prologue = instruction(targetRegion, targetOffset);
            if (prologue == null || !prologue.mnemonic.equals("ENTER")) {
                out.println("  UNRESOLVED helper: no supported ENTER prologue at " +
                    address(targetRegion.segment, targetOffset));
                continue;
            }
            out.println("  HELPER " + address(targetRegion.segment, targetOffset) +
                " (one-level slot-argument trace; caller argument facts intersected)");
            traceFrom(targetRegion, targetOffset, targetOffset + prologue.length, helper.getValue(), out, 1);
        }
    }

    @Override
    public void run() throws Exception {
        selfTest();
        String[] args = getScriptArgs();
        if (args.length != 2) { throw new IllegalArgumentException("Expected original EXE path and new report path."); }
        if (Files.exists(Path.of(args[1]))) { throw new IllegalArgumentException("Report already exists."); }
        if (!HASH.equalsIgnoreCase(currentProgram.getExecutableSHA256()) ||
            !"x86:LE:16:Real Mode".equals(currentProgram.getLanguageID().toString())) {
            throw new IllegalStateException("Unexpected executable or language.");
        }
        byte[] file = Files.readAllBytes(Path.of(args[0]));
        StringBuilder hash = new StringBuilder();
        for (byte b : MessageDigest.getInstance("SHA-256").digest(file)) { hash.append(String.format("%02x", b & 255)); }
        if (!HASH.equals(hash.toString())) { throw new IllegalStateException("EXE input hash differs."); }
        int relocationCount = word(file, 6), table = word(file, 24);
        Set<Integer> relocatedSegments = new HashSet<>();
        for (int i = 0; i < relocationCount; ++i) {
            int at = table + i * 4;
            relocatedSegments.add(word(file, at + 2) * 16 + word(file, at));
        }
        space = (SegmentedAddressSpace) currentProgram.getAddressFactory().getDefaultAddressSpace();
        decoder = new PseudoDisassembler(currentProgram);
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            if (!block.isInitialized() || !block.getName().startsWith("CODE_")) { continue; }
            long linear = block.getStart().getOffset() - 0x10000;
            Region region = new Region();
            region.segment = (int) (linear / 16);
            region.firstOffset = (int) (linear & 15);
            byte[] loaded = new byte[(int) block.getSize()];
            if (currentProgram.getMemory().getBytes(block.getStart(), loaded) != loaded.length) {
                throw new IllegalStateException("Incomplete code region.");
            }
            region.bytes = new byte[region.firstOffset + loaded.length];
            System.arraycopy(loaded, 0, region.bytes, region.firstOffset, loaded.length);
            regions.add(region);
            for (int offset = region.firstOffset; offset + 2 < region.bytes.length; ++offset) {
                if (region.segment == 0x1e48 && (region.bytes[offset] & 255) == 0xe8) {
                    int target = (offset + 3 + (short) word(region.bytes, offset + 1)) & 65535;
                    if (target == 0x2049 || target == 0x2085) {
                        Call call = new Call();
                        call.region = region; call.offset = offset;
                        call.targetSegment = region.segment; call.targetOffset = target;
                        call.near = true;
                        calls.add(call);
                    }
                }
                if (offset + 4 >= region.bytes.length) { continue; }
                if ((region.bytes[offset] & 255) != 0x9a) { continue; }
                int targetSegment = word(region.bytes, offset + 3) - 0x1000;
                if (targetSegment < 0) { continue; }
                Call call = new Call();
                call.region = region; call.offset = offset;
                call.targetSegment = targetSegment; call.targetOffset = word(region.bytes, offset + 1);
                call.relocated = relocatedSegments.contains(region.segment * 16 + offset + 3);
                calls.add(call);
            }
        }
        StringWriter text = new StringWriter();
        try (PrintWriter out = new PrintWriter(text)) {
            out.println("Desperabis object-allocation discovery, version 1");
            out.println("SHA-256: " + HASH);
            out.println("Read-only ephemeral decoding; no saved instructions/functions/references.");
            out.println("Self-tests: PASS (object/segment provenance, register clobbers, branch merge, partial-local writes).");
            out.println("Scope: all initialized CODE blocks, direct far and same-segment near CALL encodings");
            out.println("targeting 1E48:2049 or 1E48:2085. All byte offsets searched, not blindly decoded as code.");
            out.println("A relocation-backed CALL plus a local ENTER-to-call CFG path is structural evidence,");
            out.println("NOT proof of runtime reachability. Unvalidated matches remain candidates.");
            out.println("Not covered: indirect allocator calls, aliases/wrappers without a direct call edge,");
            out.println("reserved-slot writes, copied/reactivated records, or general existing-object transitions.");
            out.println("Provenance is conditional on DS being the native data segment, bounded CFG decoding,");
            out.println("and callees not overwriting tracked BP locals. Constants/branches are not path-feasibility proofs.");
            out.println("INT 34h..3Fh paths stop before inline emulator data. Other inline data remains a limitation.");
            out.println("Known slot arguments are traced through one ENTER-based helper level, not arbitrary recursion.");
            int total = 0, validated = 0;
            for (Call call : calls) {
                if (!call.allocator()) { continue; }
                ++total;
                int entry = call.relocated || call.near ? enclosing(call) : -1;
                out.println();
                out.printf("SITE %s -> %s (%s)%n", address(call.region.segment, call.offset),
                    address(call.targetSegment, call.targetOffset),
                    call.targetOffset == 0x2049 ? "first zero health / fallback 165" : "free or lowest health");
                out.printf("  MZ relocation at far segment operand: %s; near call: %s%n", call.relocated, call.near);
                if (entry < 0) {
                    out.println("  CANDIDATE ONLY: no validated local ENTER-to-call path; initialization not traced.");
                    continue;
                }
                ++validated;
                out.println("  Local CFG boundary established from ENTER " + address(call.region.segment, entry));
                for (Call caller : calls) {
                    if (caller.relocated && caller.targetSegment == call.region.segment && caller.targetOffset == entry) {
                        out.println("  Relocation-backed caller of enclosing routine (boundary not checked): " +
                            address(caller.region.segment, caller.offset));
                    }
                }
                trace(call, entry, out);
            }
            out.println();
            out.printf("TOTAL: %d direct-call encoding matches; %d local-CFG sites; %d unresolved candidates.%n",
                total, validated, total - validated);
        }
        Files.writeString(Path.of(args[1]), text.toString(), StandardCharsets.UTF_8,
            StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE);
        println("Exported read-only allocation discovery to " + args[1]);
    }
}

// Export bounded linear decoding without changing the analysis database.
// @category Desperabis

import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.app.util.PseudoInstruction;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.SegmentedAddressSpace;
import ghidra.program.model.lang.UnknownInstructionException;

public class InspectCode extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("Expected output path and module segment:offset:byteCount ranges (hex).");
        }
        if (!"219525080e93e7a9d2b072b0dd75ed35bb7e41024e01e837c7889566723a9318"
                .equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
            throw new IllegalStateException("Unexpected executable hash.");
        }
        SegmentedAddressSpace space =
            (SegmentedAddressSpace) currentProgram.getAddressFactory().getDefaultAddressSpace();
        PseudoDisassembler decoder = new PseudoDisassembler(currentProgram);
        StringWriter text = new StringWriter();
        try (PrintWriter out = new PrintWriter(text)) {
            out.println("SHA-256: " + currentProgram.getExecutableSHA256());
            out.println("Bounded linear decoding; not proof of reachability or function boundaries.");
            out.println("Addresses below are module-relative; instruction operands use Ghidra analysis addresses.");
            for (int range = 1; range < args.length; range++) {
                String[] fields = args[range].split(":");
                if (fields.length != 3) {
                    throw new IllegalArgumentException("Invalid range: " + args[range]);
                }
                int segment = Integer.parseInt(fields[0], 16);
                int offset = Integer.parseInt(fields[1], 16);
                int count = Integer.parseInt(fields[2], 16);
                if (segment < 0 || segment > 0xefff || offset < 0 || offset > 0xffff ||
                    count < 1 || count > 0x10000 - offset) {
                    throw new IllegalArgumentException("Range outside supported module address bounds.");
                }
                out.println();
                out.println("Range " + args[range]);
                Address cursor = space.getAddress(segment + 0x1000, offset);
                for (int consumed = 0; consumed < count;) {
                    monitor.checkCancelled();
                    PseudoInstruction instruction;
                    try {
                        instruction = decoder.disassemble(cursor);
                    } catch (UnknownInstructionException ex) {
                        out.printf("%04X:%04X  DECODE STOP: %s%n",
                            segment, offset + consumed, ex.getMessage());
                        break;
                    }
                    if (instruction == null) {
                        throw new IllegalStateException("Decoder returned no instruction at " + cursor);
                    }
                    int length = instruction.getLength();
                    if (length > count - consumed) {
                        out.println("STOP: instruction crosses requested range end.");
                        break;
                    }
                    byte[] bytes = new byte[length];
                    if (currentProgram.getMemory().getBytes(cursor, bytes) != length) {
                        throw new IllegalStateException("Incomplete bytes at " + cursor);
                    }
                    StringBuilder hex = new StringBuilder();
                    for (byte value : bytes) {
                        hex.append(String.format("%02X ", value & 0xff));
                    }
                    out.printf("%04X:%04X  %-24s %s", segment, offset + consumed, hex, instruction);
                    int opcode = bytes[0] & 0xff;
                    Integer relative = null;
                    if ((opcode == 0xe8 || opcode == 0xe9) && length == 3) {
                        relative = (int) (short) ((bytes[1] & 0xff) | ((bytes[2] & 0xff) << 8));
                    } else if ((opcode == 0xeb || (opcode >= 0x70 && opcode <= 0x7f) ||
                            (opcode >= 0xe0 && opcode <= 0xe3)) && length == 2) {
                        relative = (int) bytes[1];
                    } else if (opcode == 0x0f && length == 4 &&
                            (bytes[1] & 0xff) >= 0x80 && (bytes[1] & 0xff) <= 0x8f) {
                        relative = (int) (short) ((bytes[2] & 0xff) | ((bytes[3] & 0xff) << 8));
                    }
                    if (relative != null) {
                        out.printf(" ; logical module target %04X:%04X",
                            segment, (offset + consumed + length + relative) & 0xffff);
                    }
                    if (length == 5 && bytes[0] == (byte) 0x9a) {
                        int targetOffset = (bytes[1] & 0xff) | ((bytes[2] & 0xff) << 8);
                        int targetSegment = (bytes[3] & 0xff) | ((bytes[4] & 0xff) << 8);
                        out.printf(" ; encoded analysis target %04X:%04X", targetSegment, targetOffset);
                        if (targetSegment >= 0x1000) {
                            out.printf(", module %04X:%04X", targetSegment - 0x1000, targetOffset);
                        }
                    }
                    out.println();
                    consumed += length;
                    cursor = cursor.add(length);
                }
            }
        }
        Files.writeString(Path.of(args[0]), text.toString(), StandardCharsets.UTF_8,
            StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE);
        println("Exported bounded decoding to " + args[0]);
    }
}

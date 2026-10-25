// Verify the original executable's DOS import without creating speculative functions.
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
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.mem.MemoryBlock;

public class VerifyDosImport extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("Expected one report output path.");
        }
        if (!"219525080e93e7a9d2b072b0dd75ed35bb7e41024e01e837c7889566723a9318"
                .equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
            throw new IllegalStateException("The program is not the baselined original executable.");
        }
        if (!"x86:LE:16:Real Mode".equals(currentProgram.getLanguageID().toString())) {
            throw new IllegalStateException("Unexpected language: " + currentProgram.getLanguageID());
        }

        Address entry = currentProgram.getAddressFactory().getAddress("1000:ed60");
        boolean foundEntry = false;
        AddressIterator entries = currentProgram.getSymbolTable().getExternalEntryPointIterator();
        while (entries.hasNext()) {
            if (entries.next().equals(entry)) {
                foundEntry = true;
            }
        }
        if (!foundEntry) {
            throw new IllegalStateException("Expected loader entry point at 1000:ed60.");
        }
        byte[] entryBytes = new byte[32];
        if (currentProgram.getMemory().getBytes(entry, entryBytes) != entryBytes.length ||
            entryBytes[0] != (byte) 0x9a || entryBytes[1] != 0 || entryBytes[2] != 0 ||
            entryBytes[3] != (byte) 0xe4 || entryBytes[4] != (byte) 0x3c) {
            throw new IllegalStateException("Unexpected relocated entry bytes.");
        }

        StringWriter report = new StringWriter();
        try (PrintWriter out = new PrintWriter(report)) {
            out.println("Desperabis original executable: initial Ghidra import");
            out.println("SHA-256: " + currentProgram.getExecutableSHA256());
            out.println("Format: " + currentProgram.getExecutableFormat());
            out.println("Language: " + currentProgram.getLanguageID());
            out.println("Compiler spec: " + currentProgram.getCompilerSpec().getCompilerSpecID());
            out.println("Analysis: disabled (-noanalysis); no inferred function boundaries");
            out.println("Module load segment in this analysis: 1000 (not a runtime observation)");
            out.println("Module-relative entry: 0000:ED60; file offset: 0x11AA0");
            out.println("Ghidra entry: " + entry);
            out.println("Entry relocation verified: far-call segment 2CE4 -> 3CE4");
            out.print("Relocated entry bytes:");
            for (byte value : entryBytes) {
                out.printf(" %02X", value & 0xff);
            }
            out.println();
            out.println();
            out.println("Memory blocks (Ghidra addresses):");
            for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
                out.printf("%s %s..%s size=0x%X initialized=%s%n",
                    block.getName(), block.getStart(), block.getEnd(),
                    block.getSize(), block.isInitialized());
            }
            out.println();
            out.println("First 12 entry instructions (ephemeral decoding, not function recovery):");
            PseudoDisassembler disassembler = new PseudoDisassembler(currentProgram);
            Address cursor = entry;
            for (int i = 0; i < 12; i++) {
                PseudoInstruction instruction = disassembler.disassemble(cursor);
                if (instruction == null) {
                    throw new IllegalStateException("Cannot decode instruction at " + cursor);
                }
                out.println(cursor + "  " + instruction);
                cursor = cursor.add(instruction.getLength());
            }
            out.println();
            out.println("Verification: PASS");
        }
        Files.writeString(Path.of(args[0]), report.toString(), StandardCharsets.UTF_8,
            StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE);
        println("PASS: original hash, real-mode language, entry mapping, and entry relocation.");
    }
}

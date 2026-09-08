// Export private Ghidra analysis for source review; decompiler output is not proof.
// @category Xenogears.OriginalAnalysis
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.framework.Application;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Program;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.pcode.PcodeOp;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.HexFormat;

public class ExportOriginalAnalysis extends GhidraScript {
    public void run() throws Exception {
        Path output = Path.of(getScriptArgs()[0]);
        if (Files.exists(output))
            throw new IllegalStateException("Private analysis output already exists");
        JsonObject result = new JsonObject();
        result.addProperty("program", currentProgram.getName());
        result.addProperty("language", currentProgram.getLanguageID().toString());
        result.addProperty("loader", currentProgram.getExecutableFormat());
        result.addProperty("ghidra_version", Application.getApplicationVersion());
        result.addProperty(
            "profile",
            currentProgram.getOptions(Program.PROGRAM_INFO).getString("XEM Source Profile", ""));
        result.addProperty(
            "overlay_sha256",
            currentProgram.getOptions(Program.PROGRAM_INFO).getString("XEM Overlay SHA256", ""));
        result.addProperty("functions", currentProgram.getFunctionManager().getFunctionCount());
        JsonArray inventory = new JsonArray();
        FunctionIterator identified = currentProgram.getFunctionManager().getFunctions(true);
        while (identified.hasNext()) {
            Function function = identified.next();
            JsonObject data = new JsonObject();
            data.addProperty("entry", function.getEntryPoint().toString());
            data.addProperty("name", function.getName());
            data.addProperty("name_source", function.getSymbol().getSource().toString());
            data.addProperty("body", function.getBody().toString());
            data.addProperty("signature", function.getSignature().toString());
            inventory.add(data);
        }
        result.add("automatic_function_inventory", inventory);
        JsonArray blocks = new JsonArray();
        MemoryBlock overlay = null;
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            JsonObject data = new JsonObject();
            data.addProperty("name", block.getName());
            data.addProperty("start", block.getStart().toString());
            data.addProperty("size", block.getSize());
            data.addProperty("initialized", block.isInitialized());
            data.addProperty("overlay", block.isOverlay());
            blocks.add(data);
            if (block.getName().startsWith("field_"))
                overlay = block;
        }
        if (overlay == null)
            throw new IllegalStateException("Qualified overlay absent");
        result.add("blocks", blocks);
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram))
            throw new IllegalStateException("Decompiler cannot open program");
        JsonArray functions = new JsonArray();
        long[] starts = {0x80048c4cL, 0x80048d7cL, 0x8004a414L, 0x8004a70cL, 0x8004b32cL,
                         0x800222bcL, 0x800223b0L, 0x80023538L, 0x800245d8L, 0x8007b6c4L,
                         0x8007b814L, 0x8007bac0L, 0x8007bef4L, 0x8007c694L, 0x800821f4L,
                         0x80082bb8L, 0x8008492cL, 0x80084a40L, 0x80099a4cL};
        for (long start : starts) {
            Address at = start >= 0x8006faf0L
                             ? overlay.getStart().getAddressSpace().getAddress(start)
                             : toAddr(start);
            Function function = getFunctionAt(at);
            if (function == null) {
                disassemble(at);
                function = createFunction(at, null);
            }
            JsonObject data = new JsonObject();
            data.addProperty("entry", at.toString());
            if (function == null)
                data.addProperty("error", "No function at source boundary");
            else {
                DecompileResults decompiled = decompiler.decompileFunction(function, 30, monitor);
                data.addProperty("name", function.getName());
                data.addProperty("body", function.getBody().toString());
                data.addProperty("signature", function.getSignature().toString());
                data.addProperty("complete", decompiled.decompileCompleted());
                data.addProperty("error", decompiled.getErrorMessage());
                if (decompiled.decompileCompleted())
                    data.addProperty("c", decompiled.getDecompiledFunction().getC());
                JsonArray instructions = new JsonArray();
                for (Instruction instruction :
                     currentProgram.getListing().getInstructions(function.getBody(), true)) {
                    JsonObject item = new JsonObject();
                    item.addProperty("address", instruction.getAddress().toString());
                    item.addProperty("bytes", HexFormat.of().formatHex(instruction.getBytes()));
                    item.addProperty("text", instruction.toString());
                    item.addProperty("delay_slot_depth", instruction.getDelaySlotDepth());
                    item.addProperty("in_delay_slot", instruction.isInDelaySlot());
                    JsonArray pcode = new JsonArray();
                    for (PcodeOp operation : instruction.getPcode())
                        pcode.add(operation.toString());
                    item.add("pcode", pcode);
                    instructions.add(item);
                }
                data.add("instructions", instructions);
            }
            functions.add(data);
            println("XEM_DECOMPILED " + at);
        }
        decompiler.dispose();
        result.add("decompilation", functions);
        result.addProperty(
            "scope",
            "Private tool-generated analysis, requiring source/control-flow/type and "
                + "original-execution review. Library signature names are tool suggestions.");
        Files.writeString(output,
                          new GsonBuilder().setPrettyPrinting().create().toJson(result) + "\n",
                          StandardOpenOption.CREATE_NEW);
        println("XEM_ANALYSIS_EXPORTED " + output);
    }
}

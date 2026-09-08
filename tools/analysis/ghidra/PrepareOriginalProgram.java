// Source-qualified private original executable/overlay preparation.
// @category Xenogears.OriginalAnalysis
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import ghidra.app.script.GhidraScript;
import ghidra.app.util.importer.MessageLog;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Program;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;
import java.io.ByteArrayInputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.HexFormat;
import psx.PsxLoader;

public class PrepareOriginalProgram extends GhidraScript {
    private static String digest(byte[] data) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(data));
    }
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1)
            throw new IllegalArgumentException("Expected source manifest");
        JsonObject source =
            JsonParser.parseString(Files.readString(Path.of(args[0]))).getAsJsonObject();
        byte[] exe = Files.readAllBytes(Path.of(source.get("exe_path").getAsString()));
        byte[] overlay = Files.readAllBytes(Path.of(source.get("overlay_path").getAsString()));
        if (!digest(exe).equals(source.get("exe_sha256").getAsString()) ||
            !digest(overlay).equals(source.get("overlay_sha256").getAsString()))
            throw new IllegalStateException("Original source fingerprint mismatch");
        if (!currentProgram.getLanguageID().toString().equals("PSX:LE:32:default") ||
            !currentProgram.getExecutableFormat().equals("PSX Executables Loader"))
            throw new IllegalStateException("Required PSX loader/language was not used");
        byte[] expected =
            Arrays.copyOfRange(exe, 0x800, 0x800 + source.get("text_size").getAsInt());
        byte[] actual = new byte[expected.length];
        currentProgram.getMemory().getBytes(toAddr(source.get("text_address").getAsLong()), actual);
        if (!Arrays.equals(expected, actual))
            throw new IllegalStateException("Imported resident bytes differ");
        String name = "field_" + source.get("overlay_sha256").getAsString().substring(0, 16);
        if (currentProgram.getMemory().getBlock(name) != null)
            throw new IllegalStateException("Overlay already exists");
        MemoryBlock block = currentProgram.getMemory().createInitializedBlock(
            name, toAddr(source.get("overlay_base").getAsLong()), new ByteArrayInputStream(overlay),
            overlay.length, monitor, true);
        block.setRead(true);
        block.setWrite(true);
        block.setExecute(true);
        block.setComment("Exact decoded original field overlay: " +
                         source.get("overlay_sha256").getAsString());
        block.setSourceName(source.get("overlay_path").getAsString());
        long gp = PsxLoader.getGpBase(currentProgram);
        MessageLog log = new MessageLog();
        PsxLoader.setRegisterValue(currentProgram, "gp", block.getStart(), block.getEnd(), gp, log);
        String psyq = PsxLoader.getProgramPsyqVersion(currentProgram);
        currentProgram.getOptions(Program.ANALYSIS_PROPERTIES)
            .setString("PsyQ Signatures.PsyQ Version if not found", "");
        if (psyq.isEmpty())
            currentProgram.getOptions(Program.ANALYSIS_PROPERTIES)
                .setBoolean("PsyQ Signatures", false);
        currentProgram.getOptions(Program.PROGRAM_INFO)
            .setString("XEM Source Profile", source.get("profile").getAsString());
        currentProgram.getOptions(Program.PROGRAM_INFO)
            .setString("XEM Source Manifest", Path.of(args[0]).toAbsolutePath().toString());
        currentProgram.getOptions(Program.PROGRAM_INFO)
            .setString("XEM Overlay SHA256", source.get("overlay_sha256").getAsString());
        long[] starts = {0x8007b07cL, 0x8007b6c4L, 0x8007b814L, 0x8007bac0L,
                         0x8007bef4L, 0x8007c694L, 0x80081f80L, 0x800821f4L,
                         0x80082bb8L, 0x8008492cL, 0x80084a40L, 0x80099a4cL};
        String[] names = {"field_height_and_normal",
                          "field_edge_projection",
                          "field_special_sweep",
                          "field_ordinary_sweep",
                          "field_ordinary_query",
                          "field_special_query",
                          "field_velocity",
                          "field_animation_selection_unreviewed",
                          "field_active_motion_unreviewed",
                          "field_idle_predicate_unreviewed",
                          "field_position_integrator_unreviewed",
                          "field_planar_length"};
        for (int i = 0; i < starts.length; i++) {
            Address at = block.getStart().getAddressSpace().getAddress(starts[i]);
            createLabel(at, names[i], true, SourceType.USER_DEFINED);
            disassemble(at);
            if (getFunctionAt(at) == null)
                createFunction(at, names[i]);
        }
        println("XEM_SOURCE_QUALIFIED profile=" + source.get("profile").getAsString() +
                " overlay=" + block.getName() + " gp=" + Long.toHexString(gp) +
                " detected_psyq=" + psyq);
    }
}

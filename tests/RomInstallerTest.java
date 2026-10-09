import org.timecrisis.quest.RomInstaller;
import org.timecrisis.frame.FrameStorage;
import java.io.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;
import java.util.zip.*;

public final class RomInstallerTest {
    static void check(boolean ok,String message){if(!ok)throw new AssertionError(message);}
    static String sha(byte[] data)throws Exception{
        StringBuilder out=new StringBuilder();for(byte b:MessageDigest.getInstance("SHA-256").digest(data))out.append(String.format("%02x",b&255));return out.toString();
    }
    static void zip(File file,String name,byte[] data)throws Exception{
        try(ZipOutputStream out=new ZipOutputStream(new FileOutputStream(file))){out.putNextEntry(new ZipEntry(name));out.write(data);out.closeEntry();}
    }
    public static void main(String[] args)throws Exception{
        Path temp=Files.createTempDirectory("tcvr-rom-test-");
        try{
            byte[] data={1,3,7,9};Map<String,String> expected=new LinkedHashMap<>();expected.put("chip.1",sha(data));
            File dir=temp.resolve("roms").toFile(),archive=temp.resolve("rom.zip").toFile();
            check(!RomInstaller.ready(dir,expected),"Missing ROMs cannot start");
            zip(archive,"legacy-name.bin",data);RomInstaller.importZip(archive,dir,expected);
            check(RomInstaller.ready(dir,expected),"Hash-matched renamed chip imported");
            Files.write(dir.toPath().resolve("chip.1"),new byte[]{0});check(!RomInstaller.ready(dir,expected),"Corruption detected");
            RomInstaller.importZip(archive,dir,expected);check(RomInstaller.ready(dir,expected),"Repair works");
            zip(archive,"../chip.1",data);
            try{RomInstaller.importZip(archive,dir,expected);throw new AssertionError("Traversal entry accepted");}catch(IOException correct){}
            check(RomInstaller.ready(dir,expected),"Failed import preserves existing data");
            zip(archive,"chip.1",new byte[]{0});
            try{RomInstaller.importZip(archive,dir,expected);throw new AssertionError("Wrong ROM accepted");}catch(IOException correct){}
            try{RomInstaller.manifest(new ByteArrayInputStream((sha(data)+"  ../bad\n").getBytes("UTF-8")));throw new AssertionError("Bad manifest accepted");}catch(IOException correct){}
            File persistent=temp.resolve("Documents/TimeCrisisVR").toFile();FrameStorage.ensureWritable(persistent);
            check(FrameStorage.ROOT.getPath().equals("/sdcard/Documents/TimeCrisisVR"),"Frame production root is persistent across app resets");
            check(FrameStorage.archive(persistent).getPath().endsWith("Documents/TimeCrisisVR/timecris.zip"),"Frame archive uses the persistent root");
            check(FrameStorage.models(persistent).getPath().endsWith("Documents/TimeCrisisVR/models"),"Frame models use the persistent root");
            File sentinel=new File(persistent,".write-test");Files.write(sentinel.toPath(),new byte[]{4,2});FrameStorage.ensureWritable(persistent);
            check(Arrays.equals(Files.readAllBytes(sentinel.toPath()),new byte[]{4,2}),"Storage probe preserves a pre-existing sentinel");
            zip(FrameStorage.archive(persistent),"ts2ver-b.1",data);
            File frameRoms=FrameStorage.prepareRoms(persistent,expected);
            check(frameRoms.getName().equals("roms")&&RomInstaller.ready(frameRoms,expected),"Frame importer writes renamed chips to persistent storage");
            File blocker=temp.resolve("not-a-directory").toFile();Files.write(blocker.toPath(),new byte[]{1});
            try{FrameStorage.ensureWritable(new File(blocker,"TimeCrisisVR"));throw new AssertionError("Unwritable persistent root accepted");}
            catch(IOException correct){check(correct.getMessage().contains("Cannot create persistent storage directory"),"Storage failure explains the directory problem");}
            File missing=temp.resolve("missing-archive").toFile();FrameStorage.ensureWritable(missing);
            try{FrameStorage.prepareRoms(missing,expected);throw new AssertionError("Missing Frame archive was accepted");}
            catch(IOException correct){check(correct.getMessage().contains("timecris.zip"),"Missing archive error names the expected path");}
            if(args.length==2){
                Map<String,String> real=RomInstaller.manifest(new FileInputStream(args[1]));
                File game=temp.resolve("actual-game/Documents/TimeCrisisVR").toFile();FrameStorage.ensureWritable(game);
                Files.copy(new File(args[0]).toPath(),FrameStorage.archive(game).toPath());
                File gameRoms=FrameStorage.prepareRoms(game,real);
                check(real.size()==31&&RomInstaller.ready(gameRoms,real),"All real game ROMs verified under persistent Frame storage");
            }
            System.out.println("PASS: first-run, renamed chips, corruption, repair, invalid ZIP contents, traversal, manifest validation and persistent Frame ROM import.");
        }finally{
            try(java.util.stream.Stream<Path> paths=Files.walk(temp)){paths.sorted(Comparator.reverseOrder()).forEach(p->{try{Files.delete(p);}catch(IOException e){throw new UncheckedIOException(e);}});}
        }
    }
}

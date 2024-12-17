import java.net.*;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.text.Normalizer;

public class Main {
    public static String processString(String input){
        String output = input.replace("<br />", "");
        output = Normalizer.normalize(output, Normalizer.Form.NFC);
        output = output.replaceAll("[^\\p{L}\\p{N}]", " ");
        return output.toLowerCase(java.util.Locale.ENGLISH);
    }

    private static void scanFolderRecursive(File rootFolder, File currentFolder, List<String> relativeFilePaths, List<String> absoluteFilePaths) {
        File[] filesAndDirs = currentFolder.listFiles();
        if (filesAndDirs != null) {
            for (File file : filesAndDirs) {
                if (file.isFile()) {
                    String relativePath = rootFolder.toURI().relativize(file.toURI()).getPath();
                    relativeFilePaths.add(relativePath);
                    absoluteFilePaths.add(file.getAbsolutePath());
                } else if (file.isDirectory()) {
                    scanFolderRecursive(rootFolder, file, relativeFilePaths, absoluteFilePaths);
                }
            }
        }
    }

    public static void main(String[] args) throws IOException {
        System.out.println("=== JAVA CLIENT ===");
        Socket clientSocket = new Socket("127.0.0.1", 6969);
        System.out.println("Socket created successfully.");
        OutputStream output = clientSocket.getOutputStream();
        InputStream input = clientSocket.getInputStream();
        Scanner in = new Scanner(System.in);

        System.out.println("Waiting for the server signal to start sending data.");
        byte[] buffer = new byte[2];
        //Arrays.fill(serverBuffer, (byte) 0);
        input.read(buffer);
        System.out.println("Server started processing the client.");
        buffer = new byte[4096];
        Arrays.fill(buffer, (byte) 0);
        System.out.println("Select query type.");
        System.out.println(" 1 - Word query. Search for one or more words in the database.");
        System.out.println(" 2 - Add file(s). Enter a folder whose files will be sent over to be added to the index.");
        int choice = in.nextInt();
        while (choice < 1 || choice > 2) {
            System.out.println("Invalid input. Please try again.");
            choice = in.nextInt();
        }
        if(choice == 1){
            // Client
            buffer = new byte[2];
            char op = 'C';
            buffer[0] = (byte)op;
            output.write(buffer);
            System.out.println("Enter your word query (space separated words, all punctuation will be ignored):");
            in.nextLine();
            String query = in.nextLine();
            query = processString(query);
            buffer = new byte[4096];
            byte[] stringBytes = query.getBytes(StandardCharsets.UTF_8);
            System.arraycopy(stringBytes, 0, buffer, 0, Math.min(stringBytes.length, buffer.length));
            output.write(buffer);

            buffer = new byte[2];
            input.read(buffer);
            if(buffer[0] == 'Q'){
                System.out.println("Server started searching for the query in the index.");
            }
            input.read(buffer);
            if(buffer[0] == 'C'){
                System.out.println("Server finished searching for the query in the index.");
            }
            buffer = new byte[5];
            byte[] nFiles = new byte[4];
            input.read(buffer);
            System.arraycopy(buffer, 0, nFiles, 0, 4);
            int count = ByteBuffer.wrap(nFiles).order(ByteOrder.BIG_ENDIAN).getInt();
            if(count == 0){
                System.out.println("No files found, exiting.");
                return;
            }
            System.out.println(count + " files found.");
            String[] foundFiles = new String[count];
            buffer = new byte[4096];
            for(int i = 0; i < count; i++){
                input.read(buffer);
                foundFiles[i] = new String(buffer, StandardCharsets.UTF_8).replaceAll("\0", "");
            }
            while(true){
                for(int i = 0; i < foundFiles.length; i++){
                    System.out.println(i+1 + ": " + foundFiles[i]);
                }
                System.out.println("Select a number from 1-" + count + " to preview the file. Type 0 to exit");
                choice = in.nextInt();
                buffer = new byte[5];
                buffer[0] = (byte) ((choice >>> 24) & 0xFF);
                buffer[1] = (byte) ((choice >>> 16) & 0xFF);
                buffer[2] = (byte) ((choice >>> 8) & 0xFF);
                buffer[3] = (byte) (choice & 0xFF);
                output.write(buffer);
                if(choice == 0){
                    System.out.println("Exiting.");
                    return;
                }
                buffer = new byte[3];
                input.read(buffer);
                short nBlocks = ByteBuffer.wrap(buffer).slice(0, 2).order(ByteOrder.BIG_ENDIAN).getShort();
                buffer = new byte[4096];
                StringBuilder fileStringBuilder = new StringBuilder();
                for(int i = 0; i < nBlocks; i++){
                    input.read(buffer);
                    String chunk = new String(buffer, StandardCharsets.UTF_8).replaceAll("\0", "");
                    fileStringBuilder.append(chunk);
                }
                String fileContent = fileStringBuilder.toString();
                System.out.println("=== BEGIN FILE ===");
                System.out.println(fileContent);
                System.out.println("=== END FILE ===");
                System.out.println("Do you want to save the file? y/n");
                in.nextLine();
                String answer = in.nextLine();
                if(answer.charAt(0) == 'y'){
                    try {
                        File downloadsDir = new File("downloads");
                        if (!downloadsDir.exists()) {
                            if (downloadsDir.mkdir()) {
                                System.out.println("Created 'downloads' directory.");
                            } else {
                                System.out.println("Failed to create 'downloads' directory.");
                                return;
                            }
                        }
                        String fileName = new File(foundFiles[choice]).getName();
                        File newFile = new File(downloadsDir, fileName);
                        try (FileWriter writer = new FileWriter(newFile)) {
                            writer.write(fileContent);
                            System.out.println("File saved successfully at: " + newFile.getAbsolutePath());
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }
        } else {
            // Admin
            buffer = new byte[2];
            char op = 'A';
            buffer[0] = (byte)op;
            output.write(buffer);
            System.out.println("Enter full path for the folder you want scanned:");
            in.nextLine();
            String folderPath = in.nextLine();
            System.out.println("Scanning folder and subfolders for files.");
            File rootFolder = new File(folderPath);
            if (!rootFolder.exists() || !rootFolder.isDirectory()) {
                throw new IOException("Invalid folder path: " + folderPath);
            }
            List<String> relativeFilePaths = new ArrayList<>();
            List<String> absoluteFilePaths = new ArrayList<>();
            scanFolderRecursive(rootFolder, rootFolder, relativeFilePaths, absoluteFilePaths);
            System.out.println("Finished scanning, found " + relativeFilePaths.size() + " files.");
            for(int i = 0; i < absoluteFilePaths.size(); i++){
                System.out.println(absoluteFilePaths.get(i));
            }
            int foundFiles = relativeFilePaths.size();
            buffer = new byte[5];
            buffer[0] = (byte) ((foundFiles >>> 24) & 0xFF);
            buffer[1] = (byte) ((foundFiles >>> 16) & 0xFF);
            buffer[2] = (byte) ((foundFiles >>> 8) & 0xFF);
            buffer[3] = (byte) (foundFiles & 0xFF);
            output.write(buffer);
            if(relativeFilePaths.isEmpty()){
                System.out.println("Found no files, exiting.");
                return;
            }
            for(int i = 0; i < foundFiles; i++){
                buffer = new byte[4096];
                Arrays.fill(buffer, (byte)0);
                byte[] pathBytes = relativeFilePaths.get(i).getBytes(StandardCharsets.UTF_8);
                System.arraycopy(pathBytes, 0, buffer, 0, Math.min(pathBytes.length, buffer.length));
                output.write(buffer);

                Arrays.fill(buffer, (byte)0);
                int bytesRead;
                ArrayList<byte[]> chunksToSend = new ArrayList<>();
                short nBlocks = 0;
                FileInputStream fileInputStream = new FileInputStream(absoluteFilePaths.get(i));
                while ((bytesRead = fileInputStream.read(buffer, 0, 4095)) != -1) {
                    if (bytesRead < 4095) {
                        Arrays.fill(buffer, bytesRead, 4095, (byte) 0);
                    }
                    buffer[4095] = 0;
                    chunksToSend.add(buffer);
                    nBlocks++;
                }
                buffer = new byte[3];
                buffer[0] = (byte) ((nBlocks >> 8) & 0xFF);
                buffer[1] = (byte) (nBlocks & 0xFF);
                output.write(buffer);
                for(int j = 0; j < nBlocks; j++){
                    output.write(chunksToSend.get(j));
                }
            }
        }
    }
}
import java.io.FileInputStream;
import java.io.IOException;

// Test reading really big file
class Test14 {
    @SuppressWarnings("CallToPrintStackTrace")
    public static void main(String[] args) throws IOException {
        try {
            FileInputStream fis = new FileInputStream("../data/test14.txt");
            byte[] buf = new byte[2097152];
            fis.read(buf);
        } catch (IOException e) {
            throw e;
        }
    }
}
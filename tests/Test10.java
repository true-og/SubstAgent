import java.io.FileInputStream;
import java.io.IOException;
import java.util.Arrays;

// Test $SAME_LENGTH_ENV at the end of the file
class Test10 {
    static String correctString = "According to all known laws of aviation, there is no way a bee should be able to fly abcdefghijklmno";

    public static String toHexDump(String s) {
        byte[] bytes = s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        StringBuilder sb = new StringBuilder(bytes.length * 3);
        for (int i = 0; i < bytes.length; i++) {
            sb.append(String.format("%02X ", bytes[i] & 0xFF));
        }
        return sb.toString().trim();
    }

    @SuppressWarnings("CallToPrintStackTrace")
    public static void main(String[] args) throws IOException {
        try {
            FileInputStream fis = new FileInputStream("../data/test10.txt");
            byte[] buf = new byte[101];
            int n = fis.read(buf);
            byte[] content = Arrays.copyOfRange(buf, 0, n);
            String contentString = new String(content);

            if (contentString.equals(correctString)) {
                System.err.println("Correct output");
                System.exit(0);
            } else {
                System.out.println("=== Incorrect output ===");
                System.out.printf("Content string was \"%s\", should be \"%s\"\n", contentString, correctString);
                System.out.printf("Content string hex: \"%s\"\n", toHexDump(contentString));
                System.out.println("========================");
                System.exit(1);
            }
        } catch (IOException e) {
            throw e;
        }
    }
}
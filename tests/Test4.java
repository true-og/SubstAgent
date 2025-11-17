import java.io.FileInputStream;
import java.io.IOException;
import java.util.Arrays;

// Test $SAME_LENGTH_ENV split in 2 byte arrays, only the read content overflows
class Test4 {
    static String correctFirstString = "Hi abcdefghijklmno ";
    static String correctSecondString = "According to all known laws of aviation, there is no way a bee should be able to fly";

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
            FileInputStream fis = new FileInputStream("../data/test4.txt");
            byte[] firstBuf = new byte[20];
            int firstN = fis.read(firstBuf);
            byte[] firstContent = Arrays.copyOfRange(firstBuf, 0, firstN);
            String firstContentString = new String(firstContent);

            byte[] secondBuf = new byte[256];
            int secondN = fis.read(secondBuf);
            byte[] secondContent = Arrays.copyOfRange(secondBuf, 0, secondN);
            String secondContentString = new String(secondContent);

            if (firstContentString.equals(correctFirstString) && secondContentString.equals(correctSecondString)) {
                System.err.println("Correct output");
                System.exit(0);
            } else {
                System.out.printf("Incorrect output. Was \"%s\" should be \"%s\"\n", firstContentString + secondContentString, correctFirstString + correctSecondString);
                System.exit(1);
            }
        } catch (IOException e) {
            throw e;
        }
    }
}
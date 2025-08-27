package nl.skbotnl.substagent;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

@SuppressWarnings("unused")
public class Hook {

    static Pattern pattern = Pattern.compile("\\$([a-zA-Z_][a-zA-Z0-9_]*)");

    public static String substituteEnvVariables(String line) {

        if (line == null) {

            return null;

        }

        return pattern.matcher(line).replaceAll(matchResult -> {

            String env = matchResult.group(1);
            String envValue = System.getenv(env);
            if (envValue == null) {

                return Matcher.quoteReplacement(matchResult.group(0));

            }

            return Matcher.quoteReplacement(envValue);

        });

    }

    public static char[] substituteEnvVariables(char[] text) {

        if (text == null) {

            return null;

        }

        String textString = String.valueOf(text);

        String replacedString = pattern.matcher(textString).replaceAll(matchResult -> {

            String env = matchResult.group(1);
            String envValue = System.getenv(env);
            if (envValue == null) {

                return Matcher.quoteReplacement(matchResult.group(0));

            }

            return Matcher.quoteReplacement(envValue);

        });

        return replacedString.toCharArray();

    }

}

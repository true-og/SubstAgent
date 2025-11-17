# SubstAgent-JVMTI
A Java Agent written in C that listens for strings containing environment variables and substitutes them accordingly. It is intended for use with Spigot plugins to enable environment variable support in their configuration files.

# How to use
`java -agentpath:/full/path/to/libsubstagent.so [rest of your command as normal]`\
For example:\
`java -agentpath:/full/path/to/libsubstagent.so -jar server.jar nogui`

# Tests
The test script assumes that the generator is set to make (`-G "Unix Makefiles"`)
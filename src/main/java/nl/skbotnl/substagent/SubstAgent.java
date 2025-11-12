package nl.skbotnl.substagent;

import java.lang.instrument.Instrumentation;

public class SubstAgent {

    public static void premain(String agentArgs, Instrumentation inst) {

        String version = SubstAgent.class.getPackage().getImplementationVersion();
        inst.addTransformer(new BufferedReaderTransformer());
        System.out.println("[SubstAgent] Loaded version " + version);

    }

}

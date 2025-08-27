package nl.skbotnl.substagent;

import java.lang.instrument.ClassFileTransformer;
import java.security.ProtectionDomain;
import javassist.ClassPool;
import javassist.CtClass;
import javassist.CtMethod;
import javassist.LoaderClassPath;

class BufferedReaderTransformer implements ClassFileTransformer {

    @Override
    public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined, ProtectionDomain domain,
            byte[] classfileBuffer)
    {

        if (!"java/io/BufferedReader".equals(className)) {

            return null;

        }

        try {

            ClassPool cp = ClassPool.getDefault();
            cp.insertClassPath(new LoaderClassPath(loader));

            CtClass ctClass = cp.get("java.io.BufferedReader");
            CtMethod readLineMethod = ctClass.getDeclaredMethod("readLine");
            readLineMethod.insertAfter("{ $_ = nl.skbotnl.substagent.Hook.substituteEnvVariables($_); }");

            CtClass[] paramTypes = { cp.get("char[]"), CtClass.intType, CtClass.intType };
            CtMethod readMethod = ctClass.getDeclaredMethod("read", paramTypes);
            readMethod.insertAfter("{ cbuf = nl.skbotnl.substagent.Hook.substituteEnvVariables(cbuf); }");
            byte[] byteCode = ctClass.toBytecode();
            ctClass.detach();
            return byteCode;

        } catch (Throwable e) {

            throw new RuntimeException(e);

        }

    }

}

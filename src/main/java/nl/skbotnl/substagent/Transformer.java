package nl.skbotnl.substagent;

import java.lang.instrument.ClassFileTransformer;
import java.lang.reflect.Modifier;
import java.security.ProtectionDomain;
import javassist.ClassPool;
import javassist.CtClass;
import javassist.CtMethod;
import javassist.LoaderClassPath;

class Transformer implements ClassFileTransformer {
    @Override
    public byte[] transform(
            ClassLoader loader,
            String className,
            Class<?> classBeingRedefined,
            ProtectionDomain domain,
            byte[] classfileBuffer) {
        try {
            if ("java/io/BufferedReader".equals(className)) {
                ClassPool cp = ClassPool.getDefault();
                cp.insertClassPath(new LoaderClassPath(loader));

                CtClass ctClass = cp.get("java.io.BufferedReader");
                CtMethod readLineMethod = ctClass.getDeclaredMethod("readLine");
                readLineMethod.insertAfter("{ $_ = nl.skbotnl.substagent.Hook.substituteEnvVariables($_); }");

                CtClass[] readParamTypes = {cp.get("char[]"), CtClass.intType, CtClass.intType};
                CtMethod readMethod = ctClass.getDeclaredMethod("read", readParamTypes);
                readMethod.insertAfter("{ cbuf = nl.skbotnl.substagent.Hook.substituteEnvVariables(cbuf); }");

                byte[] byteCode = ctClass.toBytecode();
                ctClass.detach();
                return byteCode;
            } else if ("net/minecraft/server/dedicated/PropertyManager".equals(className)) {
                ClassPool cp = ClassPool.getDefault();
                cp.insertClassPath(new LoaderClassPath(loader));

                CtClass ctClass = cp.get("net.minecraft.server.dedicated.PropertyManager");
                for (CtMethod cm : ctClass.getDeclaredMethods()) {
                    if (!Modifier.isPrivate(cm.getModifiers())) {
                        continue;
                    }
                    if (!cm.getReturnType().getName().equals("java.lang.String")) {
                        continue;
                    }

                    CtClass[] params = cm.getParameterTypes();
                    if (params.length == 2
                            && params[0].getName().equals("java.lang.String")
                            && params[1].getName().equals("java.lang.String")) {
                        cm.insertAfter("{ $_ = nl.skbotnl.substagent.Hook.substituteEnvVariables($_); }");
                        break;
                    }
                }

                byte[] byteCode = ctClass.toBytecode();
                ctClass.detach();
                return byteCode;
            }
        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
        return null;
    }
}

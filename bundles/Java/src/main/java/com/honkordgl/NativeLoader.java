package com.honkordgl;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

final class NativeLoader {
    private NativeLoader() {}

    static void load() {
        final String os = System.getProperty("os.name", "").toLowerCase();
        final String arch = System.getProperty("os.arch", "").toLowerCase();
        final String coreLibName;
        final String jniLibName;
        if (os.contains("win")) {
            coreLibName = "HonkordGL.dll";
            jniLibName = "honkordgl_jni.dll";
        } else if (os.contains("mac")) {
            coreLibName = "libHonkordGL.dylib";
            jniLibName = "libhonkordgl_jni.dylib";
        } else {
            coreLibName = "libHonkordGL.so";
            jniLibName = "libhonkordgl_jni.so";
        }

        final String prefix = "/natives/" + osKey(os) + "-" + archKey(arch) + "/";
        try {
            final Path dir = Files.createTempDirectory("honkordgl_jni_libs_");
            dir.toFile().deleteOnExit();
            final Path core = extract(prefix + coreLibName, dir.resolve(coreLibName));
            final Path jni = extract(prefix + jniLibName, dir.resolve(jniLibName));
            System.load(core.toAbsolutePath().toString());
            System.load(jni.toAbsolutePath().toString());
        } catch (IOException e) {
            throw new UnsatisfiedLinkError("Failed to load JNI library: " + e.getMessage());
        }
    }

    private static Path extract(String resourcePath, Path target) throws IOException {
        try (InputStream in = NativeLoader.class.getResourceAsStream(resourcePath)) {
            if (in == null) {
                throw new UnsatisfiedLinkError("No binary packaged for platform: " + resourcePath);
            }
            target.toFile().deleteOnExit();
            Files.copy(in, target, StandardCopyOption.REPLACE_EXISTING);
            return target;
        }
    }

    private static String osKey(String os) {
        if (os.contains("win")) return "windows";
        if (os.contains("mac")) return "macos";
        return "linux";
    }

    private static String archKey(String arch) {
        if (arch.equals("amd64")) return "x86_64";
        return arch;
    }
}

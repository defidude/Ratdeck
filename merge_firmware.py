"""Post-build script: merge bootloader + partitions + boot_app0 + firmware
into a single .bin for M5Burner and one-step flashing."""

Import("env")

import os
import shlex


def merge_bin(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")

    # boot_app0.bin lives in the Arduino framework tools
    framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    boot_app0 = os.path.join(framework_dir, "tools", "partitions", "boot_app0.bin")

    output = os.path.join(project_dir, "ratdeck-merged.bin")

    python = env.subst("$PYTHONEXE")
    env.Execute(
        f"{shlex.quote(python)} -m esptool --chip esp32s3 merge_bin "
        "--flash_mode qio --flash_size 16MB "
        f"-o {shlex.quote(output)} "
        f"0x0000 {shlex.quote(os.path.join(build_dir, 'bootloader.bin'))} "
        f"0x8000 {shlex.quote(os.path.join(build_dir, 'partitions.bin'))} "
        f"0xe000 {shlex.quote(boot_app0)} "
        f"0x10000 {shlex.quote(os.path.join(build_dir, 'firmware.bin'))}"
    )
    print(f"\n** Merged firmware written to: {output}")


env.AddPostAction("$BUILD_DIR/firmware.bin", merge_bin)

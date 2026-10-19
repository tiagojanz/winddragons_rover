Import("env")

env.AddCustomTarget(
    name="upload_sdcard",
    dependencies=None,
    actions=[
        "python3 scripts/build_web_assets.py",
        "python3 scripts/upload_to_sd.py"
    ],
    title="Upload SDCard",
    description="Build web assets and upload to SD Card via HTTP",
    always_build=True
)

env.AddCustomTarget(
    name="bundle_rover",
    dependencies=None,
    actions=[
        "python3 scripts/bundle_firmware.py"
    ],
    title="Bundle Rover Firmware",
    description="Package dual C6 + Lafvin firmwares into dist/rover_firmware.zip",
    always_build=True
)

env.AddCustomTarget(
    name="bundle_base",
    dependencies=None,
    actions=[
        "python3 scripts/bundle_base_firmware.py"
    ],
    title="Bundle Base Firmware",
    description="Package dual Base C6 + Base Lafvin firmwares into dist/base_firmware.zip",
    always_build=True
)

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

"""
PlatformIO pre-build script.
Generates examples/custom_calendar/build_version.h and build_version.txt
with a version string of the form  vYYMMDDHHMM_XXXXXX.
"""
import random
import string
import datetime
import os

Import("env")  # noqa: F821 — provided by PlatformIO SCons environment

CHARS = string.ascii_uppercase + string.digits
build_id = "".join(random.choices(CHARS, k=6))
now = datetime.datetime.now()
dt_str = now.strftime("%y%m%d%H%M")
version = "v{}_{}".format(dt_str, build_id)

proj_dir = env.subst("$PROJECT_DIR")  # noqa: F821

header_path = os.path.join(proj_dir,
    "examples", "custom_calendar", "build_version.h")
with open(header_path, "w") as f:
    f.write("#pragma once\n")
    f.write('#define BUILD_VERSION "{}"\n'.format(version))

txt_path = os.path.join(proj_dir, "build_version.txt")
with open(txt_path, "w") as f:
    f.write(version)

print("[build_id] version = {}".format(version))

# Copyright (c) Kuba Szczodrzyński 2022-05-04.

import json
import sys
from os.path import isdir, join
from subprocess import PIPE, Popen

from ltchiptool import Family
from platformio.platform.base import PlatformBase
from platformio.platform.board import PlatformBoardConfig
from SCons.Script import DefaultEnvironment, Environment

env: Environment = DefaultEnvironment()


def read_version(platform_dir: str, version: str):
    if not isdir(join(platform_dir, ".git")):
        sys.stderr.write("Warning! Non-Git installations are NOT SUPPORTED.\n")
        return version
    try:
        p = Popen(
            ["git", "rev-parse", "--short", "HEAD"], stdout=PIPE, cwd=platform_dir
        )
        if p.wait() != 0:
            sys.stderr.write(
                f"Warning! Non-zero return code received from Git: {p.returncode}\n"
            )
            return version
        sha = p.stdout.read().decode().strip()

        p = Popen(["git", "status", "--short"], stdout=PIPE, cwd=platform_dir)
        if p.wait() != 0:
            sys.stderr.write(
                f"Warning! Non-zero return code received from Git: {p.returncode}\n"
            )
            return version
        dirty = p.stdout.read().strip()
    except (FileNotFoundError, IndexError):
        sys.stderr.write(
            "Warning! Git executable not found, or unreadable data received. Cannot read version information.\n"
        )
        return version

    ids = [
        sha and "sha",
        sha[:7] or None,
        "dirty" if dirty else None,
    ]
    build_str = ".".join(filter(None, ids))
    return f"{version}+{build_str}" if build_str else version


def env_configure(env: Environment, platform: PlatformBase, board: PlatformBoardConfig):
    # Read external libraries list
    with open(join(platform.get_dir(), "external-libs.json")) as f:
        external_libs = json.load(f)
    # Get Family object for this board
    family = Family.get(short_name=board.get("build.family"))
    # Default environment variables
    vars = dict(
        SDK_DIR=platform.get_package_dir(board.get("package")),
        LT_DIR=platform.get_dir(),
        CORES_DIR=join("${LT_DIR}", "cores"),
        COMMON_DIR=join("${LT_DIR}", "cores", "common"),
        # Build directories & paths
        BOARD_DIR=join("${LT_DIR}", "boards", "${VARIANT}"),
        FAMILY_DIR=join("${LT_DIR}", "cores", "${FAMILY_NAME}"),
        MISC_DIR=join("${FAMILY_DIR}", "misc"),
        LDSCRIPT_PATH=[board.get("build.ldscript")],
        # Board config variables
        MCU=board.get("build.mcu").upper(),
        MCULC=board.get("build.mcu").lower(),
        VARIANT=board.get("build.variant"),
        # ltchiptool config:
        # -r    output raw log messages
        # -i 1  indent log messages
        LTCHIPTOOL='"${PYTHONEXE}" -m ltchiptool -r -i 1',
        # Fix for link2bin to get tmpfile name in argv
        LINKCOM="${LINK} ${LINKARGS}",
        LINKARGS="${TEMPFILE('-o $TARGET $LINKFLAGS $__RPATH $SOURCES $_LIBDIRFLAGS $_LIBFLAGS', '$LINKCOMSTR')}",
        # Store the family object
        FAMILY_OBJ=family,
        EXTERNAL_LIBS=external_libs,
    )
    env.Replace(**vars)
    # Store family parameters as environment variables
    env.Replace(**dict(family))
    # Default build options
    env.Prepend(
        CPPDEFINES=[
            ("LIBRETUYA", 1),
            ("LT_VERSION", read_version(platform.get_dir(), platform.version)),
            ("LT_BOARD", "${VARIANT}"),
            ("F_CPU", board.get("build.f_cpu")),
            ("MCU", "${MCU}"),
            ("FAMILY", "F_${FAMILY}"),
        ],
        LINKFLAGS=[
            '"-Wl,-Map=' + join("$BUILD_DIR", "${PROGNAME}.map") + '"',
        ],
    )


env.AddMethod(env_configure, "ConfigureEnvironment")
